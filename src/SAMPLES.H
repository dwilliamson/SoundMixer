
#ifndef _INCLUDED_SAMPLES_H
#define _INCLUDED_SAMPLES_H


#define		mixSAMPLE_TYPE_VAG			1
#define		mixSAMPLE_TYPE_RAW			2
#define		mixSAMPLE_TYPE_FILE			4


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

#pragma	pack(push, 1)

typedef struct t_VagHdr
{
	char			id[4];
	unsigned long	version;
	unsigned long	res1;
	unsigned long	size;
	unsigned long	srate;
	unsigned long	res2;
	unsigned long	res3;
	unsigned long	res4;
	unsigned char	name[16];
	unsigned char	data;
} T_VAGHDR;

#pragma pack(pop)


#endif