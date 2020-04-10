## 简介

MAD是一个开源的高精度 MPEG 音频解码库，支持 MPEG-1（Layer I, Layer II 和 LayerIII（也就是 MP3))。

MAD具有如下特点：

- 24位PCM输出
- 100%定点（整数）计算
- 基于ISO/IEC标准的全新实现
- 根据GNU通用公共许可证（GPL）条款提供

### mp3文件解码示例

- 修改并编译mad库中自带的minimad.c测试用例。该用例可将mp3文件解码为pcm。具体修改代码如下：

```c
/*
 * libmad - MPEG audio decoder library
 * Copyright (C) 2000-2004 Underbit Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: minimad.c,v 1.4 2004/01/23 09:41:32 rob Exp $
 */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>

# include "mad.h"

/*
 * This is perhaps the simplest example use of the MAD high-level API.
 * Standard input is mapped into memory via mmap(), then the high-level API
 * is invoked with three callbacks: input, output, and error. The output
 * callback converts MAD's high-resolution PCM samples to 16 bits, then
 * writes them to standard output in little-endian, stereo-interleaved
 * format.
 */

static int decode(unsigned char const *, unsigned long);

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[])
{
	int fd;
	struct stat stat;
	void *fdm;

	if (argc != 2)
		return 1;

	fd = open(argv[1], O_RDONLY);
	if (fd == -1)
		handle_error("open");


	if ((fstat(fd, &stat) == -1) || (stat.st_size == 0))
		return 2;

	fdm = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (fdm == MAP_FAILED)
		return 3;

	decode(fdm, stat.st_size);

	if (munmap(fdm, stat.st_size) == -1)
		return 4;

	close(fd);

	return 0;
}

/*
 * This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct buffer {
	unsigned char const *start;
	unsigned long length;
};

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */

static
enum mad_flow input(void *data,
		struct mad_stream *stream)
{
	struct buffer *buffer = data;

	if (!buffer->length)
		return MAD_FLOW_STOP;

	mad_stream_buffer(stream, buffer->start, buffer->length);

	buffer->length = 0;

	return MAD_FLOW_CONTINUE;
}

/*
 * The following utility routine performs simple rounding, clipping, and
 * scaling of MAD's high-resolution samples down to 16 bits. It does not
 * perform any dithering or noise shaping, which would be recommended to
 * obtain any exceptional audio quality. It is therefore not recommended to
 * use this routine if high-quality output is desired.
 */

static inline
signed int scale(mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - 16));

	/* clip */
	if (sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if (sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/*
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */

static
enum mad_flow output(void *data,
		struct mad_header const *header,
		struct mad_pcm *pcm)
{
	char ch;
	FILE *file = NULL;
	static int cnt = 0;
	unsigned int nchannels, nsamples;
	mad_fixed_t const *left_ch, *right_ch;

	/* pcm->samplerate contains the sampling frequency */

	file = fopen("./out.pcm", "ab");
	nchannels = pcm->channels;
	nsamples  = pcm->length;
	left_ch   = pcm->samples[0];
	right_ch  = pcm->samples[1];

	printf("====%s call time: %10d, ch = %d, nsamples = %d\n", __FUNCTION__, cnt++, nchannels,nsamples);
	while (nsamples--) {
		signed int sample;

		/* output sample(s) in 16-bit signed little-endian PCM */

		sample = scale(*left_ch++);
		ch = ((sample >> 0) & 0xff);
		fwrite(&ch, 1, 1, file);
		ch = ((sample >> 8) & 0xff);
		fwrite(&ch, 1, 1, file);

		if (nchannels == 2) {
			sample = scale(*right_ch++);
			ch = ((sample >> 0) & 0xff);
			fwrite(&ch, 1, 1, file);
			ch = ((sample >> 8) & 0xff);
			fwrite(&ch, 1, 1, file);
			//putchar((sample >> 0) & 0xff);
			//putchar((sample >> 8) & 0xff);
		}
	}
	fclose(file);

	return MAD_FLOW_CONTINUE;
}

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */

static
enum mad_flow error(void *data,
		struct mad_stream *stream,
		struct mad_frame *frame)
{
	struct buffer *buffer = data;

	fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
			stream->error, mad_stream_errorstr(stream),
			stream->this_frame - buffer->start);

	/* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

	return MAD_FLOW_CONTINUE;
}

/*
 * This is the function called by main() above to perform all the decoding.
 * It instantiates a decoder object and configures it with the input,
 * output, and error callback functions above. A single call to
 * mad_decoder_run() continues until a callback function returns
 * MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
 * signal an error).
 */

static
int decode(unsigned char const *start, unsigned long length)
{
	struct buffer buffer;
	struct mad_decoder decoder;
	int result;

	/* initialize our private message structure */

	buffer.start  = start;
	buffer.length = length;

	/* configure input, output, and error functions */

	mad_decoder_init(&decoder, &buffer,
			input, 0 /* header */, 0 /* filter */, output,
			error, 0 /* message */);

	/* start decoding */

	result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

	/* release the decoder */

	mad_decoder_finish(&decoder);

	return result;
}

```

- 编译(gcc -g -O2 -Ioutput/include minimad.c -Loutput/lib -lmad --static)并运行，解码完成后的pcm存放到out.pcm中。如下所示：

```c
yingc@yingc:~/d/thirdparty/libmad$ gcc -g -O2 -Ioutput/include minimad.c -Loutput/lib -lmad --static
minimad.c: In function ‘error’:
minimad.c:269:62: warning: format ‘%u’ expects argument of type ‘unsigned int’, but argument 5 has type ‘long int’ [-Wformat=]
  fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
                                                             ~^
                                                             %lu
minimad.c:271:4:
    stream->this_frame - buffer->start);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                         
yingc@yingc:~/d/thirdparty/libmad$ file a.out 
a.out: ELF 64-bit LSB executable, x86-64, version 1 (GNU/Linux), statically linked, for GNU/Linux 3.2.0, BuildID[sha1]=63ed931812fe394eae288232caa3d9ee6b376c5c, with debug_info, not stripped
yingc@yingc:~/d/thirdparty/libmad$ ./a.out hello.mp3 
decoding error 0x0101 (lost synchronization) at byte offset 0
====output call time:          0, ch = 1, nsamples = 576
====output call time:          1, ch = 1, nsamples = 576
====output call time:          2, ch = 1, nsamples = 576
====output call time:          3, ch = 1, nsamples = 576
====output call time:          4, ch = 1, nsamples = 576
====output call time:          5, ch = 1, nsamples = 576
====output call time:          6, ch = 1, nsamples = 576
====output call time:          7, ch = 1, nsamples = 576
====output call time:          8, ch = 1, nsamples = 576
====output call time:          9, ch = 1, nsamples = 576
====output call time:         10, ch = 1, nsamples = 576
====output call time:         11, ch = 1, nsamples = 576
====output call time:         12, ch = 1, nsamples = 576
====output call time:         13, ch = 1, nsamples = 576
====output call time:         14, ch = 1, nsamples = 576
====output call time:         15, ch = 1, nsamples = 576
====output call time:         16, ch = 1, nsamples = 576
====output call time:         17, ch = 1, nsamples = 576
====output call time:         18, ch = 1, nsamples = 576
====output call time:         19, ch = 1, nsamples = 576
yingc@yingc:~/d/thirdparty/libmad$ 
```

