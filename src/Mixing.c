
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <malloc.h>
#include "samples.h"
#include "main.h"
#include "debug.h"


#define		mixVOLUME_POS_LEFT		1
#define		mixVOLUME_POS_RIGHT		2

#define		mixMODE_SS_DS			(mixSTEREO | (mixSTEREO << 4))
#define		mixMODE_SM_DS			(mixMONO | (mixSTEREO << 4))
#define		mixMODE_SM_DM			(mixMONO | (mixMONO << 4))


// Code from me V-Rally code! Woooooo!
#define		scale_var(min_res, max_res, min_src, max_src, t)				\
	(min_res + (((float)(max_res - min_res) / (float)(max_src - min_src)) * (float)(t - min_src)))


// Values setup for playstation
short			mixMinVolConv = -10000;
short			mixMaxVolConv = 0;

int				mixMasterVolumeL = 255;
int				mixMasterVolumeR = 255;

unsigned char	mixMode = 0;

void (*mixApplyVoice)(unsigned char *buffer, unsigned char voice);

void mixApplyVoice_Ss_Ds(unsigned char *buffer, unsigned char voice);
void mixApplyVoice_Sm_Ds(unsigned char *buffer, unsigned char voice);
void mixApplyVoice_Sm_Dm(unsigned char *buffer, unsigned char voice);
void mixAV_NULL(unsigned char *buffer, unsigned char voice);


/*-----------------------------------------------------------------------------------------------
 * mixSetMode
 * a. Source buffer channels
 * b. Destination buffer channels
 *-----------------------------------------------------------------------------------------------
 */
void mixSetMode(unsigned char src, unsigned char dest)
{
	mixMode = src | (dest << 4);

	switch (mixMode)
	{
		case (mixMODE_SS_DS): mixApplyVoice = mixApplyVoice_Ss_Ds; break;
		case (mixMODE_SM_DS): mixApplyVoice = mixApplyVoice_Sm_Ds; break;
		case (mixMODE_SM_DM): mixApplyVoice = mixApplyVoice_Sm_Dm; break;
		default: mixApplyVoice = mixAV_NULL;
	}
}


/*-----------------------------------------------------------------------------------------------
 * mixSetVolumeConvert
 * a. Minimum volume ever passed
 * b. Maximum volume ever passed
 *-----------------------------------------------------------------------------------------------
 */
void mixSetVolumeConvert(short min, short max)
{
	mixMinVolConv = min;
	mixMaxVolConv = max;
}


/*-----------------------------------------------------------------------------------------------
 * mixSetVoiceVolume
 * a. Voice handle
 * b. Left or right
 * c. Volume (to be scaled)
 *-----------------------------------------------------------------------------------------------
 */
void mixSetVoiceVolume(unsigned long voice, unsigned char where, short volume)
{
	mixVoice	*voice_ptr = (mixVoice *)voice;
	short		converted;

	// Scale the volume to the internal format
	converted = (short)(scale_var(0.0f, 255.0f, mixMinVolConv, mixMaxVolConv, volume));
	converted = (short)(VolumeTable[converted] >> 8);

	if (voice_ptr)
	{
		// Assign the volumes, to both if necessary
		if (where & mixVOLUME_POS_LEFT) voice_ptr->volume_left = converted;
		if (where & mixVOLUME_POS_RIGHT) voice_ptr->volume_right = converted;
	}
}


/*-----------------------------------------------------------------------------------------------
 * mixSetMasterVolume
 * a. Volume (to be scaled)
 *-----------------------------------------------------------------------------------------------
 */
unsigned long mixSetMasterVolume(int volume)
{
	MMRESULT		retval;
	int				converted;
	unsigned long	new_volume;

	// Scale the volume to the internal format
	converted = (int)(scale_var(0.0f, 65535.0f, 0.0f, 100.0f, volume));
	new_volume = (unsigned long) converted | (((unsigned long)converted) << 16);
	retval = waveOutSetVolume(hWaveOut, new_volume);

	if (retval == MMSYSERR_NOERROR)
		return (1);

	return (0);
}


/*-----------------------------------------------------------------------------------------------
 * mixGetMasterVolume
 *-----------------------------------------------------------------------------------------------
 */
unsigned long mixGetMasterVolume(void)
{
	int		converted, volume;

	waveOutGetVolume(hWaveOut, &volume);
	converted = (int)(scale_var(0.0f, 100.0f, 0.0f, 65535.0f, volume));

	return (converted);
}


/*-----------------------------------------------------------------------------------------------
 * mixIsVoiceActive
 * a. Voice handle
 *-----------------------------------------------------------------------------------------------
 */
unsigned char mixIsVoiceActive(unsigned long voice)
{
	return (((mixVoice *)voice)->playing);
}


/*-----------------------------------------------------------------------------------------------
 * mixSamples(Source: Stereo, Destination: Stereo)
 * a. Pointer to sample data
 * b. Mixing destination
 * c. Length to mix (in bytes)
 * d. Left volume
 * e. Right volume
 *
 * Ss: Source stereo
 * Sm: Source mono
 * Ds: Destination stereo
 * Dm: Destination mono
 *-----------------------------------------------------------------------------------------------
 */

void __inline MixSamples_Ss_Ds(short *sample_ptr, short *dest, long length, long step, long vol_l, long vol_r)
{
	short	sample;
	long	pos = 0;

	while (length > 0)
	{
		sample = sample_ptr[(pos >> 8)];
		*(dest++) += (short)((sample * vol_l) >> 8);
		pos += step;

		sample = sample_ptr[(pos >> 8)];
		*(dest++) += (short)((sample * vol_r) >> 8);
		pos += step;

		length--;
	}
}


void __inline MixSamples_Sm_Ds(short *sample_ptr, short *dest, long length, long step, long vol_l, long vol_r)
{
	short	sample;
	long	pos = 0;

	while (length > 0)
	{
		sample = sample_ptr[(pos + 127) >> 8];
		*(dest++) += (short)(((sample * vol_l) >> 8));
		*(dest++) += (short)(((sample * vol_r) >> 8));

		pos += step;
		length--;
	}
}


void __inline MixSamples_Sm_Dm(short *sample_ptr, short *dest, long length, long vol)
{
	short	sample;

	while (length > 0)
	{
		sample = *sample_ptr++;
		*(dest++) += (short)(((sample * vol) >> 8));

		length--;
	}
}


/*-----------------------------------------------------------------------------------------------
 * mixApplyVoice
 * a. Sample data pointer
 * b. Voice index
 *-----------------------------------------------------------------------------------------------
 */
void mixApplyVoice_Ss_Ds(unsigned char *buffer, unsigned char voice)
{
	long			length;
	long			pos, len, vol_l, vol_r, step, next_pos;
	short			*sample_ptr, *dest = (short *)buffer;

	// Return if no sample is playing on this voice
	if (VoiceList[voice].playing == 0)
		return;

	// Figure out the maximum source samples allowed to be processed for 16-bit stereo
	// Divide by 2 to get from bytes to 16-bit units
	// Divide by 2 again for mono to stereo
	length = mixBufferLength_Bytes >> 2;

	// Jump to the current position of the sample
	sample_ptr = (short *)(VoiceList[voice].sample_ptr->data_ptr + VoiceList[voice].position);

	// Get a few vars
	pos = VoiceList[voice].position;
	len = VoiceList[voice].sample_ptr->length;
	vol_l = VoiceList[voice].volume_left;
	vol_r = VoiceList[voice].volume_right;
	step = VoiceList[voice].step;

	// See where the source would end up if processing a maximum amount of samples
	next_pos = pos + (((length * step) >> 8) << 2);

	// Does this sample need to stop playing while applying this voice?
	if (next_pos > len)
	{
		// Figure out how many steps there are going to be for the remaining bytes
		length = (((len - pos) << 8) >> 2) / step;

		// Only mix the samples left
//		MixSamples_Ss_Ds(sample_ptr, (short *)buffer, (len - pos) >> 2, vol_l, vol_r);
		MixSamples_Ss_Ds(sample_ptr, (short *)buffer, length, step, vol_l, vol_r);

		// Stop the voice
		VoiceList[voice].playing = 0;
		VoiceList[voice].position = 0;
		VoiceList[voice].sample_ptr = NULL;
	}
	else
	{
		// Mix the entire voice buffer
		MixSamples_Ss_Ds(sample_ptr, (short *)buffer, length, step, vol_l, vol_r);

		// Move the sample on a bit
		VoiceList[voice].position = next_pos;
	}
}


void mixApplyVoice_Sm_Ds(unsigned char *buffer, unsigned char voice)
{
	long			length;
	unsigned long	pos, len, vol_l, vol_r, step, next_pos;
	short			*sample_ptr, *dest = (short *)buffer;

	// Return if no sample is playing on this voice
	if (VoiceList[voice].playing == 0)
		return;

	// Figure out the maximum source samples allowed to be processed for 16-bit mono
	// Divide by 2 to get from bytes to 16-bit units
	// Divide by 2 again since every source equals 2 destinations
	length = mixBufferLength_Bytes >> 2;

	// Jump to the current position of the sample
	sample_ptr = (short *)(VoiceList[voice].sample_ptr->data_ptr + VoiceList[voice].position);

	// Get a few vars
	pos = VoiceList[voice].position;
	len = VoiceList[voice].sample_ptr->length;
	vol_l = VoiceList[voice].volume_left;
	vol_r = VoiceList[voice].volume_right;

	// Temp fix
	step = VoiceList[voice].sample_ptr->step;
//	step = VoiceList[voice].step;

	// See where the source would end up if processing a maximum amount of samples
	next_pos = pos + (((length * step) >> 8) << 1);

	// Does this sample need to stop playing while applying this voice?
	if (next_pos > len)
	{
		// Figure out how many steps there are going to be for the remaining bytes
		length = (((len - pos) << 8) >> 1) / step;

		// Only mix the samples left
		MixSamples_Sm_Ds(sample_ptr, (short *)buffer, length, step, vol_l, vol_r);

		// Stop the voice
		VoiceList[voice].playing = 0;
		VoiceList[voice].position = 0;
		VoiceList[voice].sample_ptr = NULL;
	}
	else
	{
		// Mix the entire voice buffer
		MixSamples_Sm_Ds(sample_ptr, (short *)buffer, length, step, vol_l, vol_r);

		// Move the sample on a bit
		VoiceList[voice].position = next_pos;
	}
}


void mixApplyVoice_Sm_Dm(unsigned char *buffer, unsigned char voice)
{
	long			length;
	long			pos, len, vol;
	short			*sample_ptr, *dest = (short *)buffer;

	// Return if no sample is playing on this voice
	if (VoiceList[voice].playing == 0)
		return;

	// Figure out the length for 16-bit mono
	length = mixBufferLength_Bytes >> 1;

	// Jump to the current position of the sample
	sample_ptr = (short *)(VoiceList[voice].sample_ptr->data_ptr + VoiceList[voice].position);

	// Get a few vars
	pos = VoiceList[voice].position;
	len = VoiceList[voice].sample_ptr->length;
	vol = VoiceList[voice].volume_left;

	// Does this sample need to stop playing while applying this voice?
	if (len - pos <= (long)mixBufferLength_Bytes)
	{
		// Only mix the samples left
		MixSamples_Sm_Dm(sample_ptr, (short *)buffer, (len - pos) >> 1, vol);

		// Stop the voice
		VoiceList[voice].playing = 0;
		VoiceList[voice].position = 0;
		VoiceList[voice].sample_ptr = NULL;
	}
	else
	{
		// Mix the entire voice buffer
		MixSamples_Sm_Dm(sample_ptr, (short *)buffer, length, vol);

		// Move the sample on a bit
		VoiceList[voice].position += mixBufferLength_Bytes;
	}
}


void mixAV_NULL(unsigned char *buffer, unsigned char voice)
{
	(buffer == buffer);
	(voice == voice);
}


/*-----------------------------------------------------------------------------------------------
 * mixStopVoice
 * a. Voice handle
 *-----------------------------------------------------------------------------------------------
 */
void mixStopVoice(unsigned long voice)
{
	mixVoice	*voice_ptr = (mixVoice *)voice;

	if (voice_ptr)
	{
		voice_ptr->playing = 0;
		voice_ptr->position = 0;
		voice_ptr->sample_ptr = NULL;
	}
}