/*
 ** Copyright (C) 2001-2013 Erik de Castro Lopo <erikd@mega-nerd.com>
 **
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are
 ** met:
 **
 **	 * Redistributions of source code must retain the above copyright
 **	   notice, this list of conditions and the following disclaimer.
 **	 * Redistributions in binary form must reproduce the above copyright
 **	   notice, this list of conditions and the following disclaimer in
 **	   the documentation and/or other materials provided with the
 **	   distribution.
 **	 * Neither the author nor the names of any contributors may be used
 **	   to endorse or promote products derived from this software without
 **	   specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 ** TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 ** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 ** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 ** EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 ** PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 ** OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 ** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 ** OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 ** ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sndfile.h>

#include "lc3.h"

#define		BUFFER_LEN	1024

/* libsndfile can handle more than 6 channels but we'll restrict it to 6. */
#define		MAX_CHANNELS	6

typedef sf_count_t (*read_function) (SNDFILE *sndfile, void *, sf_count_t frames);

struct encoder_context {
	SF_INFO sfi;
	read_function read_frame;
	int bitrate;
	int frame_len;
	int data_interval;
	int blocks_per_sdu;
	int bps;
	void *enc_mem;
	char in_buffer[4096];
	char out_buffer[4096];
	lc3_encoder_t enc;
};


static void init_context(struct encoder_context *ctxt)
{
	memset(ctxt, 0, sizeof(*ctxt));

	/* Taken from android code */
	ctxt->bitrate = 32000;
	ctxt->frame_len = 40;
	ctxt->data_interval = 10000; /* in us */
	ctxt->blocks_per_sdu = 1; /* ?? */
	ctxt->bps = 16;
	ctxt->enc_mem = NULL;
	ctxt->enc = NULL;
	return;
}

static int get_channels(struct encoder_context *ctxt)
{
	return ctxt->sfi.channels;
}

static int get_bitrate(struct encoder_context *ctxt)
{
	return ctxt->bitrate;
}

static int get_bits_sample(struct encoder_context *ctxt)
{
	return ctxt->bps;
}

static int get_samplerate(struct encoder_context *ctxt)
{
	return ctxt->sfi.samplerate;
}

static int get_intervalUs(struct encoder_context *ctxt)
{
	return ctxt->data_interval;
}

static int get_frame_len(struct encoder_context *ctxt)
{
	return ctxt->frame_len;
}

static int get_blocks_per_sdu(struct encoder_context *ctxt)
{
	return ctxt->blocks_per_sdu;
}

static int get_max_sdu_per_chan(struct encoder_context *ctxt)
{
	return get_frame_len(ctxt) * get_blocks_per_sdu(ctxt);
}

static int get_max_sdu_size(struct encoder_context *ctxt)
{
	return get_channels(ctxt) * get_max_sdu_per_chan(ctxt);
}

static int encoder_init(struct encoder_context *ctxt)
{
	int enc_size;

	enc_size = lc3_encoder_size(get_intervalUs(ctxt), get_samplerate(ctxt));

	ctxt->enc_mem = malloc(enc_size);
	if (!ctxt->enc_mem)
		return -1;

	ctxt->enc = lc3_setup_encoder(get_intervalUs(ctxt), get_samplerate(ctxt),
				       ctxt->enc_mem);

	return ctxt->enc ? 0 : -1;
}

static int read_frame(SNDFILE *in, struct encoder_context *ctxt)
{
	int read_frames;
	int number_of_samples = lc3_frame_samples(get_intervalUs(ctxt),
			get_samplerate(ctxt));

	read_frames = ctxt->read_frame(in, ctxt->in_buffer, number_of_samples);

	return read_frames;
}

static int encode_frame(struct encoder_context *ctxt)
{
	memset(ctxt->out_buffer, 0, sizeof(ctxt->out_buffer));

	lc3_encode(ctxt->enc, (void *)ctxt->in_buffer, get_channels(ctxt),
			get_max_sdu_per_chan(ctxt), ctxt->out_buffer);
	return 0;
}

/* get wav file read function */
static read_function get_read_function(SNDFILE *sf, SF_INFO *sfi)
{
	read_function snd_read_function = NULL;

	switch(sfi->format & SF_FORMAT_SUBMASK) {
		case SF_FORMAT_PCM_16:
		case SF_FORMAT_PCM_U8:
		case SF_FORMAT_PCM_S8:
			printf("PCM Format S16NE\n");
			snd_read_function = (read_function) sf_readf_short;
			break;
		case SF_FORMAT_PCM_24:
			printf("PCM format S24NE\n");
			break;
		case SF_FORMAT_PCM_32:
			printf("PCM format S32NE\n");
			snd_read_function = (read_function) sf_readf_int;
			break;
		case SF_FORMAT_ULAW:
			printf("PCM format Ulaw\n");
			break;
		case SF_FORMAT_ALAW:
			printf("PCM format Alaw\n");
			break;
		case SF_FORMAT_FLOAT:
		case SF_FORMAT_DOUBLE:
			printf("PCM format FLOAT32NE");
			snd_read_function = (read_function) sf_readf_float;
			break;
		default:
			printf("Unknown format\n");
			break;
	}
	printf("channels: %d\n", sfi->channels);
	printf("samplerate: %d\n", sfi->samplerate);
	printf("frames: %lu\n", sfi->frames);
	return snd_read_function;
}

static void pprint_few_bytes(char *title, void *p, int bytes)
{
	int i, j;
	unsigned char *buffer;


	buffer = p;

	printf("\n");
	printf("\n----------------START - %s bytes: %d ----------\n", title, bytes);

	for (i = 0, j = 0; j < bytes / 8; j++) {
		printf("%2.2d:  %2.2x %2.2x %2.2x %2.2X %2.2x %2.2x %2.2x %2.2x",
				j, buffer[i], buffer[i+1], buffer[i+2],buffer[i+3],
				buffer[i+4], buffer[i+5], buffer[i+6], buffer[i+7]);
		i = i + 8;
		printf("\n");
	}

	if (i % 8) {
		for (j = i % 8; j > 0; j++, i++)
			printf("%2.2x ", buffer[i]);
		printf("\n");
	}
	printf("\n----------------END - %s---------------------\n", title);
	printf("\n");

}

int main (int argc, char **argv)
{
	int frame_cnt;
	SNDFILE	*infile;
	int	readcount ;
	struct encoder_context ctxt;

	if (argc != 2) {
		printf("Usage:  a.out  <wavfilename>\n");
		exit(EXIT_FAILURE);
	}

	init_context(&ctxt);

	if (!(infile = sf_open(argv[1], SFM_READ, &ctxt.sfi)))
	{
		printf ("Not able to open input file %s.\n", argv[1]) ;
		puts (sf_strerror (NULL)) ;
		return 1 ;
	}

	if (ctxt.sfi.channels > MAX_CHANNELS)
	{
		printf ("Not able to process more than %d channels\n", MAX_CHANNELS) ;
		return 1 ;
	}

	ctxt.read_frame = get_read_function(infile, &ctxt.sfi);

	if (!ctxt.read_frame) {
		printf("Not supported format\n");
		exit(EXIT_FAILURE);
	}

	/* initialize encoder */
	encoder_init(&ctxt);

	frame_cnt = 0;
	while((readcount = read_frame(infile, &ctxt)) > 0) {
		frame_cnt++;
		pprint_few_bytes("input", ctxt.in_buffer, readcount * 2);
		encode_frame(&ctxt);
		pprint_few_bytes("output", ctxt.out_buffer, get_frame_len(&ctxt));
		//printf("Read framecount: %d\n", frame_cnt);
	}

	printf("Read framecount: %d\n", frame_cnt);
	sf_close (infile);

	return 0 ;
} /* main */

