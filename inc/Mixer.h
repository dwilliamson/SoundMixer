
#ifndef _INCLUDED_MIXER_H
#define _INCLUDED_MIXER_H

#ifdef __cplusplus
	extern "C" {
#endif

#pragma comment(lib, "winmm.lib")
#pragma	comment(lib, "Sound Mixer.lib")

#define		mixMONO						1
#define		mixSTEREO					2

#define		mixSAMPLE_TYPE_VAG			1
#define		mixSAMPLE_TYPE_RAW			2
#define		mixSAMPLE_TYPE_FILE			4

#define		mixFILTER_NONE				0
#define		mixFILTER_LERP				1

#define		mixVOLUME_POS_LEFT			1
#define		mixVOLUME_POS_RIGHT			2

#define		mixVOICE_AUTO				0xFFFFFFFF


typedef	unsigned long	mixVoice;


typedef struct sample_t
{
	unsigned char	*data_ptr;					/* Actual sample data */
	unsigned long	length;						/* Length in bytes */
	unsigned char	channels;					/* Stereo or mono */
	unsigned char	bits;						/* 8-bit or 16-bit */
	unsigned long	sample_rate;				/* Rate sample was recorded at */
	unsigned long	step;						/* 24:8 fixed point frequency step */
	unsigned char	name[32];					/* Optional name  (used by VAG) */
} mixSample;


/*
 * mixInit
 * -------
 * a. Stereo or mono (mixSTEREO, mixMONO)
 * b. Frequency in hertz (11025, 22050, 44100, etc...)
 * c. Bits per sample (8 or 16)
 * d. Mixing buffer length in micro-seconds (suggested value: 100000)
 * e. Number of voices to mix
 *
 * Function returns 1 upon success, 0 upon failure.
 */
extern	unsigned char	mixInit(unsigned char channels, unsigned long playback_freq, unsigned short bits, unsigned long buffer_len, unsigned long voices);


/*
 * mixSetMode
 * ----------
 * a. Source buffer number of channels (mixSTEREO, mixMONO)
 * b. Destination buffer number of channels (mixSTEREO, mixMONO)
 *
 * Note: The destination buffer is the buffer which specifies which volume modification controls 
 * are used (left and right or just single mono volume).
 */
extern	void			mixSetMode(unsigned char src, unsigned char dest);


/*
 * mixClose
 * --------
 */
extern	void			mixClose(void);


/*
 * mixNewSample
 * ------------
 * a. Either a pointer to the sample data or a pointer to a filename
 * b. Describes what "data" is (mixSAMPLE_TYPE_VAG and mixSAMPLE_TYPE_RAW can be ORed with
 *    mixSAMPLE_TYPE_FILE if "data" is a filename)
 * c. Length of sample - only needed for RAW data types
 *
 * Function returns pointer to newly created sample structure.
 */
extern	mixSample		*mixNewSample(void *data, unsigned char flags, unsigned long length);


/*
 * mixFreeSample
 * -------------
 * a. Pointer to sample structure to release
 */
extern	void			mixFreeSample(mixSample *sample);


/*
 * mixPlaySample
 * -------------
 * a. Pointer to sample structure
 * b. Voice to play sample on (can be mixVOICE_AUTO for automatic placement)
 *
 * Function returns handle to voice that the sample was played on.
 */
extern	mixVoice		mixPlaySample(mixSample *sample, mixVoice voice);


/*
 * mixStopVoice
 * ------------
 * a. Handle to voice to stop playback on
 *
 * Note: Stopping "samples" from playing cannot be done unless you only allow a sample to be
 * played once at a time. When you play a sample, get the handle of the voice it started
 * playback on and use this handle to stop that voice if need be.
 */
extern	void			mixStopVoice(mixVoice voice);


/*
 * mixGetFreeVoice
 * ---------------
 *
 * Function returns handle to a voice that is available for playback on. This handle can be
 * used as a parameter to "mixPlaySample" if desired.
 */
extern	mixVoice		mixGetFreeVoice(void);


/*
 * mixGetVoice
 * -----------
 * a. Voice index out of total number created with call to "mixInit""
 *
 * Function returns handle to that voice.
 */
extern	mixVoice		mixGetVoice(unsigned long voice);


/*
 * mixIsVoiceActive
 * ----------------
 * a. Handle to voice
 *
 * Function returns if voice has any currently playing samples.
 */
extern	unsigned char	mixIsVoiceActive(mixVoice voice);


/*
 * mixConvertSample
 * ----------------
 * a. Pointer to sample
 * b. Desired number of channels (mixSTEREO or mixMONO)
 * c. Desired bits per sample (8 or 16)
 * d. Desired playback frequency (in hertz)
 *
 * Function returns 1 upon success, 0 upon failure (performs memory allocation so possible
 * chance of failure).
 */
extern	char			mixConvertSample(mixSample *sample, unsigned char channels, unsigned char bits, unsigned long frequency);


/*
 * mixSetVolumeConvert
 * -------------------
 * a. Minimum value that will ever be passed as a volume
 * b. Maximum value that will ever be passed as a volume
 *
 * Note: Since different applications use different scales for volume, use this function so that
 * the library can convert from your volume scale to it's own native scale. This function
 * must be called before setting any volumes.
 */
extern	void			mixSetVolumeConvert(short min, short max);


/*
 * mixSetVoiceVolume
 * -----------------
 * a. Handle to voice
 * b. Left or right speaker (mixVOLUME_POS_LEFT or mixVOLUME_POS_RIGHT)
 * c. Actual volume in your scale
 *
 * Note: Get the voice handle when called "mixPlaySample" - this allows the same sample to
 * wield different volumes when played multiple times.
 */
extern	void			mixSetVoiceVolume(unsigned long voice, unsigned char where, short volume);


/*
 * mixSetMasterVolume
 * ------------------
 * a. Actual volume in your scale
 */
extern	unsigned long	mixSetMasterVolume(int volume);


/*
 * mixGetMasterVolume
 * ------------------
 */
extern	unsigned long	mixGetMasterVolume(void);


/*
 * mixGetSampleFrequency
 * ---------------------
 * a. Pointer to sample structure
 *
 * Function returns recorded frequency of sample.
 */
extern	unsigned long	mixGetSampleFrequency(mixSample *sample);


/*
 * mixSetVoiceFrequency
 * --------------------
 * a. Handle to voice
 * b. Desired playback frequency
 */
extern	void			mixSetVoiceFrequency(unsigned long voice, unsigned long freq);

/*
 * mixEnumActiveVoices
 * -------------------
 * a. Pointer to callback function
 */
extern	void			mixEnumActiveVoices(void (*callback)(mixVoice *, unsigned char *name));

#endif

#ifdef __cplusplus
	};
#endif