
#ifndef _INCLUDED_MAIN_H
#define _INCLUDED_MAIN_H


#define		mixMONO				1
#define		mixSTEREO			2

typedef struct
{
	mixSample		*sample_ptr;
	char			playing;

	unsigned long	position;
	unsigned long	step;

	unsigned short	volume_left;
	unsigned short	volume_right;
} mixVoice;


extern	unsigned char		mixChannels;
extern	unsigned long		mixPlaybackFreq;
extern	unsigned short		mixBitsPerSample;
extern	unsigned long		mixBufferLength;
extern	unsigned long		mixBufferLength_Bytes;
extern	unsigned long		mixVoices;
extern	mixVoice			*VoiceList;

extern	unsigned long		VolumeTable[256];
extern	HWAVEOUT			hWaveOut;

#endif