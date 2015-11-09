//----------------------------------
// Sound Mixer
// NOTES
//
// A block of sound data needs to be continuously sent to the audio device, repeating in it's
// play. When a sample is called to be played, it is added to the channel listing of current
// samples playing, along with a time stamp telling when the sample started.
//
// The mixing buffer should be modifiable in it's size but should be fairly small so as to
// reduce latency on played sounds. Try and make the length modifiable in time segments.
//
// Sound volume has been specified in a sort of inverse deci-bel system (unit intensities in
// factors to the power of 10 - logarithmic).
//
// Due to the frequency compensate step being in 24:8 fixed point for each voice, a waveform
// cannot be greater than 16MB in size (streamed or un-streamed). This could be fixed by going
// to 26:6 (67MB) put loses accuracy. So until such need arises, the format shall stay.
//
// Further implementations of this mixer need to contain steps of greater accuracy than
// 24:8 fixed point since high frequency samples are being inaccuractly stepped through,
// missing vital peaks in the waveform. For now, the voice frequency is ignore and the
// default sample frequency is used. Increasing the accuracy of the step to 16:16 is
// desired (0.00001 unit step compared to current 0.004) but this means samples can only
// be 64k max in length. Some form of block management is needed.
//
// Variables...
//
// a. Number of channels (1 or 2)
// b. Bits per sample (8 or 16)
// c. Playback frequency (samples per second)
//
// 1). total_samples = playback_freq * buffer_length (S)
// 2). bit_length = total_samples * bits_per_sample * num_channels
// 3). byte_length = bit_length / 8

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <malloc.h>
#include <stdio.h>
#include <math.h>
#include "samples.h"
#include "main.h"
#include "mixing.h"
#include "debug.h"

#define		NUM_QUEUE_BLOCKS	10
#define		VOL_SCALE			3


/*
=================================================================================================
-------------------------------------------------------------------------------------> Variables
=================================================================================================
*/

		HWAVEOUT		hWaveOut;
static	WAVEHDR			whWaveList[NUM_QUEUE_BLOCKS];
static	void			*BlockQueue = NULL;
static	char			BlockFree[NUM_QUEUE_BLOCKS];
static	char			mix_init = 0;

		unsigned char	mixChannels;
		unsigned long	mixPlaybackFreq;
		unsigned short	mixBitsPerSample;
		unsigned long	mixBufferLength;
		unsigned long	mixBufferLength_Bytes;
		unsigned long	mixVoices;
		mixVoice		*VoiceList = NULL;

// Gravis UltraSound volume table (hope it sounds ok on a SoundBlaster) - 65 entries (0 & 64 inc.)

unsigned short GUSvol[] =
{
        0, 0x1500,
        0x9300,0xA900,0xB400,0xBC00,0xC180,0xC580,0xC980,0xCD80,
        0xCF40,0xD240,0xD440,0xD640,0xD840,0xDA40,0xDC40,0xDE40,
        0xDEF0,0xDFA0,0xE1A0,0xE2A0,0xE3A0,0xE4A0,0xE5A0,0xE6A0,
        0xE7A0,0xE8A0,0xE9A0,0xEAA0,0xEBA0,0xECA0,0xEDA0,0xEEA0,
        0xEEF0,0xEFE0,0xEF60,0xF1E0,0xF160,0xF1E0,0xF260,0xF2E0,
        0xF360,0xF3E0,0xF460,0xF4E0,0xF560,0xF5E0,0xF660,0xF6E0,
        0xF760,0xF7E0,0xF860,0xF8E0,0xF960,0xF9E0,0xFA60,0xFAF0,
        0xFB70,0xFBF0,0xFC70,0xFCF0,0xFD70,0xFD90,0xFDB0,0xFDD0
};

		unsigned long	VolumeTable[256];

/*
=================================================================================================
-------------------------------------------------------------------------------------> Functions
=================================================================================================
*/

/*-----------------------------------------------------------------------------------------------
 * CreateVolumeTable
 *-----------------------------------------------------------------------------------------------
 */
void CreateVolumeTable(void)
{
	unsigned long	x, z;
	unsigned long	difference;

	for (x = 0, z = 0; x < 64; x++)
	{
		VolumeTable[z++] = (GUSvol[x] * VOL_SCALE) / 8;

		difference = GUSvol[x + 1] - GUSvol[x];

		VolumeTable[z++] = ((GUSvol[x] + (difference >> 2)) * VOL_SCALE) / 8;
		VolumeTable[z++] = ((GUSvol[x] + (difference >> 1)) * VOL_SCALE) / 8;
		VolumeTable[z++] = ((GUSvol[x] + ((difference >> 2) * 3)) * VOL_SCALE) / 8;
	}
}


/*-----------------------------------------------------------------------------------------------
 * GetFreeBlock
 *-----------------------------------------------------------------------------------------------
 */
WAVEHDR *GetFreeBlock(void)
{
	unsigned long	x;

	for (x = 0; x < NUM_QUEUE_BLOCKS; x++)
	{
		if (BlockFree[x])
		{
			BlockFree[x] = 0;
			memset(whWaveList[x].lpData, 0, mixBufferLength_Bytes);
			return (&whWaveList[x]);
		}
	}

	return (NULL);
}


/*-----------------------------------------------------------------------------------------------
 * ReleaseBlock
 * a. Pointer to WAVEHDR block to be released
 *-----------------------------------------------------------------------------------------------
 */
void ReleaseBlock(WAVEHDR *wave_hdr)
{
	unsigned long	x;

	for (x = 0; x < NUM_QUEUE_BLOCKS; x++)
	{
		if (&whWaveList[x] == wave_hdr)
		{
			BlockFree[x] = 1;
			return;
		}
	}
}


/*-----------------------------------------------------------------------------------------------
 * ReleaseAllBlocks
 *-----------------------------------------------------------------------------------------------
 */
void ReleaseAllBlocks(void)
{
	unsigned long	x;

	for (x = 0; x < NUM_QUEUE_BLOCKS; x++)
		BlockFree[x] = 1;
}


/*-----------------------------------------------------------------------------------------------
 * CountTakenBlocks
 *-----------------------------------------------------------------------------------------------
 */
char CountTakenBlocks(void)
{
	unsigned long	x;
	char			count = 0;

	for (x = 0; x < NUM_QUEUE_BLOCKS; x++)
		if (!BlockFree[x])
			count++;

	return (count);
}


/*-----------------------------------------------------------------------------------------------
 * mixPlayBlock
 * a. Length of mixing buffer in bytes
 *-----------------------------------------------------------------------------------------------
 */
unsigned char mixPlayBlock(unsigned long total_bytes)
{
	MMRESULT		mr;
	unsigned long	x;
	WAVEHDR			*wave_hdr;

	if (!mix_init)
		return (0);

	// Setup the WAVEHDR structure that represents the mixing buffer
	wave_hdr = GetFreeBlock();

	wave_hdr->dwBufferLength = total_bytes;
	wave_hdr->dwFlags = 0;
	wave_hdr->dwLoops = 0;

	// Prepare the audio data for playback
	if ((mr = waveOutPrepareHeader(hWaveOut, wave_hdr, sizeof(WAVEHDR))) != MMSYSERR_NOERROR)
	{
		return (0);
	}

	for (x = 0; x < mixVoices; x++)
	{
		mixApplyVoice(wave_hdr->lpData, (unsigned char)x);
	}

	// Send the first block of sound data to the audio device
	if ((mr = waveOutWrite(hWaveOut, wave_hdr, sizeof(WAVEHDR))) != MMSYSERR_NOERROR)
	{
		waveOutUnprepareHeader(hWaveOut, wave_hdr, sizeof(WAVEHDR));
		ReleaseBlock(wave_hdr);

		// Release data here

		return (0);
	}

	return (1);
}


/*-----------------------------------------------------------------------------------------------
 * waveOutProc
 * a. Handle to the waveOut device
 * b. Message delivered
 * c. User provided parameter
 * d. Message specific parameter (1)
 * e. Message specific parameter (2)
 *-----------------------------------------------------------------------------------------------
 */
void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	// When the device driver is opened (waveOutOpen)
	if (uMsg == WOM_OPEN)
	{
	}

	// When the device driver is finished sending a data block
	if (uMsg == WOM_DONE)
	{
		waveOutUnprepareHeader(hWaveOut, (WAVEHDR *)dwParam1, sizeof(WAVEHDR));
		ReleaseBlock((WAVEHDR *)dwParam1);

		while (mix_init && CountTakenBlocks() < 3)
			mixPlayBlock(mixBufferLength_Bytes);
	}

	// When the device driver is closed (waveOutClose)
	if (uMsg == WOM_CLOSE)
	{
	}
}


/*-----------------------------------------------------------------------------------------------
 * mixClose
 *-----------------------------------------------------------------------------------------------
 */
void mixClose(void)
{	
	mix_init = 0;

	// Stop playback on the audio device
	waveOutReset(hWaveOut);

	// Close the audio device
	waveOutClose(hWaveOut);

	// Safe time to release voice structures
	if (VoiceList)
	{
		free(VoiceList);
		VoiceList = NULL;
	}
}


/*-----------------------------------------------------------------------------------------------
 * mixInit
 * a. Stereo or Mono playback
 * b. Playback frequency (8kHz, 11.025kHz, 22.05kHz, 44.1kHz, etc...)
 * c. Bits per sample (8-bit, 16-bit)
 * d. Length of mixing buffer in micro-seconds
 *-----------------------------------------------------------------------------------------------
 */
unsigned char mixInit(unsigned char channels, unsigned long playback_freq, unsigned short bits, unsigned long buffer_len, unsigned long voices)
{
	WAVEFORMATEX	wave_format;
	MMRESULT		mr;
	unsigned long	total_samples, total_bytes, x;

	if (mix_init)
		return (1);

	// Create the stretched volume table
	CreateVolumeTable();

	// Release all sound blocks
	ReleaseAllBlocks();

	// Register globals
	mixChannels = channels;
	mixPlaybackFreq = playback_freq;
	mixBitsPerSample = bits;
	mixBufferLength = buffer_len;
	mixVoices = voices;

	// Allocate list of voices
	if ((VoiceList = malloc(voices * sizeof(mixVoice))) == NULL)
		return (0);
	memset(VoiceList, 0, voices * sizeof(mixVoice));

	// This is the mixing buffer, where the final sound is played
	wave_format.wFormatTag = WAVE_FORMAT_PCM;
	wave_format.nChannels = mixChannels;
	wave_format.nSamplesPerSec = mixPlaybackFreq;
	wave_format.wBitsPerSample = mixBitsPerSample;
	wave_format.nBlockAlign = mixChannels * (mixBitsPerSample >> 3);
	wave_format.nAvgBytesPerSec = wave_format.nBlockAlign * mixPlaybackFreq;
	wave_format.cbSize = 0;

	// Open the wave-form audio device for output
	if ((mr = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wave_format, (DWORD)&waveOutProc, (DWORD)NULL, CALLBACK_FUNCTION)) != MMSYSERR_NOERROR)
	{
		free(VoiceList);
		return (0);
	}

	// Get the number of individual samples for the mixing buffer
	total_samples = (mixBufferLength * mixPlaybackFreq) / 1000000;

	// Finally, the length of the mixing buffer in bytes
	total_bytes = (total_samples * mixBitsPerSample * mixChannels) >> 3;

	mixBufferLength_Bytes = total_bytes;

	// Allocate the sound block queue table
	if ((BlockQueue = malloc(NUM_QUEUE_BLOCKS * total_bytes)) == NULL)
	{
		mixClose();
		return (0);
	}

	// Assign a queue block to each WAVEHDR structure
	for (x = 0; x < NUM_QUEUE_BLOCKS; x++)
	{
		whWaveList[x].lpData = (char *)BlockQueue + (x * mixBufferLength_Bytes);
		BlockFree[x] = 1;
	}

	mix_init = 1;

	// Queue up 3 blocks of sound data
	for (x = 0; x < 3; x++)
	{
		if (!mixPlayBlock(total_bytes))
		{
			mixClose();
			return (0);
		}
	}

	return (1);
}


void mixEnumActiveVoices(void (*callback)(mixVoice *, unsigned char *name))
{
	unsigned int	x;

	if (callback)
		for (x = 0; x < mixVoices; x++)
			if (VoiceList[x].playing)
				callback(&VoiceList[x], VoiceList[x].sample_ptr->name);
}