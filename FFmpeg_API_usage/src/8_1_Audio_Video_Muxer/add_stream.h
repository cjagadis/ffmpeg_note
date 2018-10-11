#pragma once
#include <libavformat\avformat.h>
#include <libswscale\swscale.h>
//#include <libswresample\swresample.h>

#define STREAM_FRAME_RATE 25  // 25 image per second
#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P  //defualt pixel format
#define STREAM_DURATION 10.0
#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
	AVStream *st;

	// pts of the next frame that will be generated
	int64_t next_pts;
	int samples_count;

	AVFrame *frame;
	AVFrame *tmp_frame;

	float t, tincr, tincr2;

	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;

	FILE *fp_out;
}OutputStream;

void add_stream(OutputStream *out_st, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id);