
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <malloc.h>
#include "samples.h"
#include "main.h"

#define		mixVOICE_AUTO				0xFFFFFFFF

double	frequency_table[5][2] =
{
	{ 0.0,						 0.0 },
	{ 60.0 / 64.0,				 0.0 },
	{ 115.0 / 64.0,		-52.0 / 64.0 },
	{ 98.0 / 64.0,		-55.0 / 64.0 },
	{ 122.0 / 64.0,		-60.0 / 64.0 }
};

double	samples[28];


unsigned long ByteReverse(unsigned long value)
{
	unsigned long	retval;
	unsigned char	*valptr = (unsigned char *)&value, *retptr = (unsigned char *)&retval;

	retptr[3] = valptr[0];
	retptr[2] = valptr[1];
	retptr[1] = valptr[2];
	retptr[0] = valptr[3];

	return (retval);
}


/*-----------------------------------------------------------------------------------------------
 * mixConvert_VAG_to_RAW
 * a. Pointer to a VAG structure
 * b. Pointer to the destination sample
 *-----------------------------------------------------------------------------------------------
 */
unsigned char mixConvert_VAG_to_RAW(T_VAGHDR *vag, mixSample *sample)
{
	char	*vag_ptr = (char *)vag + 64, *data_ptr;
	int		predict, shift_factor, flags, x;
	int		d, s;
	double	s_1 = 0.0, s_2 = 0.0;
	int		size = 0;

	// Firstly, calculate the number of bytes the RAW will be
	while (1)
	{
		vag_ptr++;
		flags = (int)*vag_ptr++;

		if (flags == 7)
			break;

		vag_ptr += 14;

		size += 56;
	}

	sample->length = size;
	sample->bits = 16;
	sample->channels = 2;

	strcpy(sample->name, vag->name);

	sample->sample_rate = ByteReverse(vag->srate);
	sample->step = (sample->sample_rate << 8) / mixPlaybackFreq;

	if ((sample->data_ptr = malloc(size)) == NULL)
		return (0);

	vag_ptr = (char *)vag + 64;
	data_ptr = sample->data_ptr;

	while (1)
	{
		predict = (int)*vag_ptr++;
		shift_factor  = predict & 0x0F;
		predict >>= 4;

		flags = (int)*vag_ptr++;

		if (flags == 7)
			break;

		for (x = 0; x < 28; x += 2)
		{
			d = (int)*vag_ptr++;

			s = (d & 0x0F) << 12;
			if (s & 0x8000)
				s |= 0xFFFF0000;
			samples[x] = (double)(s >> shift_factor);

			s = (d & 0xF0) << 8;
			if (s & 0x8000)
				s |= 0xFFFF0000;
			samples[x + 1] = (double)(s >> shift_factor);
		}

		for (x = 0; x < 28; x++)
		{
			samples[x] = samples[x] + s_1 * frequency_table[predict][0] + s_2 * frequency_table[predict][1];
			s_2 = s_1;
			s_1 = samples[x];
			d = (int)(samples[x] + 0.5);
			*data_ptr++ = (char)(d & 0xFF);
			*data_ptr++ = (char)(d >> 8);
		}
	}

	return (1);
}


/*-----------------------------------------------------------------------------------------------
 * mixConvertSample_Channels
 * a. Sample pointer
 * b. Number of channels
 *-----------------------------------------------------------------------------------------------
 */
char mixConvertSample_Channels(mixSample *sample, unsigned char channels)
{
	if (sample->channels == channels)
		return (1);

	// 8-bit, 0-255 format
	if (sample->bits == 8)
	{
		// Converting up, from mono to stereo
		if (sample->channels == 1 && channels == 2)
		{
			unsigned char	*data, *data_ptr;
			unsigned long	x;

			// Allocate twice as much, for the extra channel
			if ((data = malloc(sample->length << 1)) == NULL)
			{
				return (0);
			}

			data_ptr = data;

			// Copy the mono data to either channel of the stereo data
			for (x = 0; x < sample->length; x++)
			{
				*data_ptr++ = sample->data_ptr[x];
				*data_ptr++ = sample->data_ptr[x];
			}

			// Replace the old mono buffer with the new stereo one
			free(sample->data_ptr);
			sample->data_ptr = data;
			sample->length <<= 1;
			sample->channels = 2;

			return (1);
		}

		if (sample->channels == 2 && channels == 1)
		{
			unsigned char	*data, *data_ptr;
			unsigned char	*cur_sample;
			unsigned long	x;

			// Allocate twice as little, getting rid of the extra channel
			if ((data = malloc(sample->length >> 1)) == NULL)
			{
				return (0);
			}

			data_ptr = data;
			cur_sample = sample->data_ptr;

			// Average the stereo data of either channel to the single mono channel
			for (x = 0; x < sample->length; x += 2)
			{
				*data_ptr++ = (cur_sample[0] + cur_sample[1]) >> 1;
				cur_sample += 2;
			}

			// Replace the old stereo buffer with the new mono one
			free(sample->data_ptr);
			sample->data_ptr = data;
			sample->length >>= 1;
			sample->channels = 1;

			return (1);
		}
	}

	// 16-bit, ~-32000 - ~32000 format
	if (sample->bits == 16)
	{
		// Converting up, from mono to stereo
		if (sample->channels == 1 && channels == 2)
		{
			short			*data, *data_ptr;
			unsigned long	x;

			// Allocate twice as much, for the extra channel
			if ((data = malloc(sample->length << 1)) == NULL)
			{
				return (0);
			}

			data_ptr = data;

			// Copy the mono data to either channel of the stereo data
			for (x = 0; x < sample->length; x += 2)
			{
				*data_ptr++ = *((unsigned short *)&sample->data_ptr[x]);
				*data_ptr++ = *((unsigned short *)&sample->data_ptr[x]);
			}

			// Replace the old mono buffer with the new stereo one
			free(sample->data_ptr);
			sample->data_ptr = (unsigned char *)data;
			sample->length <<= 1;
			sample->channels = 2;

			return (1);
		}

		if (sample->channels == 2 && channels == 1)
		{
			short			*data, *data_ptr;
			short			*cur_sample;
			unsigned long	x;

			// Allocate twice as little, getting rid of the extra channel
			if ((data = malloc(sample->length >> 1)) == NULL)
			{
				return (0);
			}

			data_ptr = data;
			cur_sample = (short *)sample->data_ptr;

			// Average the stereo data of either channel to the single mono channel
			for (x = 0; x < sample->length; x += 4)
			{
				*data_ptr++ = (cur_sample[0] + cur_sample[1]) >> 1;
				cur_sample += 2;
			}

			// Replace the old stereo buffer with the new mono one
			free(sample->data_ptr);
			sample->data_ptr = (unsigned char *)data;
			sample->length >>= 1;
			sample->channels = 1;

			return (1);
		}
	}

	return (0);
}


/*-----------------------------------------------------------------------------------------------
 * mixConverSample_Bits
 * a. Sample pointer
 * b. Number of bits per sample
 *-----------------------------------------------------------------------------------------------
 */
char mixConvertSample_Bits(mixSample *sample, unsigned char bits)
{
	if (sample->bits == bits)
		return (1);

	// Must be from 8->16
	if (bits == 16)
	{
		unsigned char	*data, *cur_sample;
		short			*data_ptr;
		unsigned long	x;

		// Allocate twice as much
		if ((data = malloc(sample->length << 1)) == NULL)
		{
			return (0);
		}

		data_ptr = (short *)data;
		cur_sample = sample->data_ptr;

		// Convert from 8-bit unsigned to 16-bit unsigned
		for (x = 0; x < sample->length; x++)
			*data_ptr++ = ((short)(*cur_sample++) * 256) - 32768;

		// Replace the two
		free(sample->data_ptr);
		sample->data_ptr = data;
		sample->length <<= 1;
		sample->bits = 16;

		return (1);
	}

	// Must be from 16->8
	if (bits == 8)
	{
		short			*data, *cur_sample;
		unsigned char	*data_ptr;
		unsigned long	x;

		// Allocate twice as little
		if ((data = malloc(sample->length >> 1)) == NULL)
		{
			return (0);
		}

		data_ptr = (unsigned char *)data;
		cur_sample = (short *)sample->data_ptr;

		// Convert from 16-bit signed to 8-bit unsigned
		for (x = 0; x < sample->length; x += 2)
			*data_ptr++ = (unsigned char)((*cur_sample++ + 32768) / 256);

		// Replace the two
		free(sample->data_ptr);
		sample->data_ptr = (unsigned char *)data;
		sample->length >>= 1;
		sample->bits = 8;

		return (1);
	}

	return (0);
}


/*-----------------------------------------------------------------------------------------------
 * mixConvertSample_Frequency
 * a. Pointer to sample
 * b. New frequency
 *-----------------------------------------------------------------------------------------------
 */
char mixConvertSample_Frequency(mixSample *sample, unsigned long frequency)
{
	if (sample->sample_rate == frequency)
		return (1);

	// Consider an interpolated conversion of expanded waves :)

	if (sample->bits == 8)
	{
		unsigned long	step, size, x, pos;
		unsigned char	*data, *data_ptr, *cur_sample;

		// Calculate the fixed point step through the sample
		step = (sample->sample_rate << 16) / frequency;

		// Calculate the size of the resultant buffer (+1)
		size = ((sample->length * step) >> 16) + 1;

		if ((data = malloc(size)) == NULL)
		{
			return (0);
		}

		data_ptr = data;
		cur_sample = sample->data_ptr;
		pos = 0;

		// Step through each sample and copy
		for (x = 0; x < size; x++)
		{
			*data_ptr++ = cur_sample[(pos + 0x7FFF) >> 16];
			pos += step;
		}

		// Replace the old data
		free(sample->data_ptr);
		sample->data_ptr = data;
		sample->length = size;
		sample->sample_rate = frequency;

		return (1);
	}

	if (sample->bits == 16)
	{
		unsigned long	step, size, x, pos;
		short			*data, *data_ptr, *cur_sample;

		// Calculate the fixed point step through the sample
		step = (sample->sample_rate << 16) / frequency;

		// Calculate the size of the resultant buffer (+1)
		size = ((sample->length * step) >> 16) + 1;

		if ((data = malloc(size)) == NULL)
		{
			return (0);
		}

		data_ptr = data;
		cur_sample = (short *)sample->data_ptr;
		pos = 0;

		// Step through each sample and copy
		for (x = 0; x < size; x++)
		{
			*data_ptr++ = cur_sample[(pos + 0x7FFF) >> 16];
			pos += step;
		}

		// Replace the old data
		free(sample->data_ptr);
		sample->data_ptr = (unsigned char *)data;
		sample->length = size;
		sample->sample_rate = frequency;

		return (1);
	}

	return (0);
}


/*-----------------------------------------------------------------------------------------------
 * mixConvertSample
 * a. Pointer to sample
 * b. Number of channels
 * c. Number of bits per sample
 * d. New frequency
 *-----------------------------------------------------------------------------------------------
 */
char mixConvertSample(mixSample *sample, unsigned char channels, unsigned char bits, unsigned long frequency)
{
	if (sample)
	{
		// Convert channels?
		if (channels)
			if (!mixConvertSample_Channels(sample, channels))
				return (0);

		// Convert bits?
		if (bits)
			if (!mixConvertSample_Bits(sample, bits))
				return (0);

		// Convert frequency
		if (frequency)
			if (!mixConvertSample_Frequency(sample, frequency))
				return (0);
	}

	return (1);
}


/*-----------------------------------------------------------------------------------------------
 * mixNewSample
 * a. Pointer to sample data
 * b. What type of sample is it?
 * c. Length of sample (unspecified for VAG)
 *
 * If a RAW sample is specified then the sample returned will not have the parameters setup
 * properly - these MUST be setup by the caller, after creating the sample.
 *-----------------------------------------------------------------------------------------------
 */
mixSample *mixNewSample(void *data, unsigned char flags, unsigned long length)
{
	mixSample	*sample;
	void		*data_ptr;

	// Allocate space for the structure
	if ((sample = malloc(sizeof(mixSample))) == NULL)
	{
		return (NULL);
	}

	// Does the "data" pointer reference a filename?
	if (flags & mixSAMPLE_TYPE_FILE)
	{
		FILE			*fp;
		unsigned long	file_size;

		// Attempt to open the specified file
		if ((fp = fopen((const char *)data, "rb")) == NULL)
		{
			free(sample);
			return (NULL);
		}

		// Find out the file size
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		// Now try and allocate some space for it
		if ((data_ptr = malloc(file_size)) == NULL)
		{
			fclose(fp);
			free(sample);
			return (NULL);
		}

		// Read all the data over from file
		fread(data_ptr, 1, file_size, fp);

		fclose(fp);
	}
	else
	{
		// The data itself is specified here
		data_ptr = data;
	}

	// Determine what type of format the data is in
	if (flags & mixSAMPLE_TYPE_VAG)
	{
		T_VAGHDR	*vag_header;

		vag_header = (T_VAGHDR *)data_ptr;

		// ADPCM Decompression (VAG is the file image)
		mixConvert_VAG_to_RAW(vag_header, sample);
	}
	else if (flags & mixSAMPLE_TYPE_RAW)
	{
		// Length is valid in this case
		sample->length = length;

		// Allocate data for the sample
		if ((sample->data_ptr = malloc(sample->length)) == NULL)
		{
			free(sample);
			return (NULL);
		}

		// Copy everything over
		memcpy(sample->data_ptr, data_ptr, sample->length);
	}

	// Release locally allocated memory
	if (flags & mixSAMPLE_TYPE_FILE)
		free(data_ptr);

	return (sample);
}


/*-----------------------------------------------------------------------------------------------
 * mixFreeSample
 * a. Pointer to the sample to be freed
 *-----------------------------------------------------------------------------------------------
 */
void mixFreeSample(mixSample *sample)
{
	if (sample)
	{
		if (sample->data_ptr) free(sample->data_ptr);
		free(sample);
	}
}


/*-----------------------------------------------------------------------------------------------
 * mixGetFreeVoice
 *-----------------------------------------------------------------------------------------------
 */
unsigned long mixGetFreeVoice(void)
{
	unsigned long	x;

	for (x = 0; x < mixVoices; x++)
	{
		// Free voice?
		if (VoiceList[x].sample_ptr == NULL)
			return ((unsigned long)&VoiceList[x]);
	}

	return (0);
}


/*-----------------------------------------------------------------------------------------------
 * mixGetVoice
 * a. Voice index
 *-----------------------------------------------------------------------------------------------
 */
unsigned long mixGetVoice(unsigned long voice)
{
	return ((unsigned long)&VoiceList[voice]);
}


/*-----------------------------------------------------------------------------------------------
 * mixGetSampleFrequency
 * a. Pointer to sample
 *-----------------------------------------------------------------------------------------------
 */
unsigned long mixGetSampleFrequency(mixSample *sample)
{
	return (sample->sample_rate);
}


/*-----------------------------------------------------------------------------------------------
 * mixGetSampleFrequency
 * a. Voice pointer
 * b. Frequency (hertz)
 *-----------------------------------------------------------------------------------------------
 */
void mixSetVoiceFrequency(unsigned long voice, unsigned long freq)
{
	mixVoice	*voice_ptr = (mixVoice *)voice;

	// Calculate the 24:8 fixed point scaling step
//	voice_ptr->step = (freq << 8) / mixPlaybackFreq;
	voice_ptr->step = (int)((((float)freq / (float)mixPlaybackFreq) * 256.0f) + 0.5f);
}


/*-----------------------------------------------------------------------------------------------
 * mixPlaySample
 * a. Pointer to the sample to be played
 * b. Voice (if any) to play sample on
 *-----------------------------------------------------------------------------------------------
 */
unsigned long mixPlaySample(mixSample *sample, unsigned long voice)
{
	unsigned long	x;

	if (voice == mixVOICE_AUTO)
	{
		for (x = 0; x < mixVoices; x++)
		{
			// Is there a sample playing on this voice?
			if (VoiceList[x].sample_ptr == NULL)
			{
				VoiceList[x].sample_ptr = sample;
				VoiceList[x].playing = 1;
				VoiceList[x].position = 0;

				return ((unsigned long)&VoiceList[x]);
			}
		}
	}
	else
	{
		mixVoice	*voice_ptr = (mixVoice *)voice;

		// Is this voice free?
		if (voice_ptr->sample_ptr == NULL)
		{
			voice_ptr->sample_ptr = sample;
			voice_ptr->playing = 1;
			voice_ptr->position = 0;

			return ((unsigned long)voice_ptr);
		}
	}

	return ((unsigned long)&VoiceList[x]);
}
