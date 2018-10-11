#include "open_media.h"

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height) {
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;

	//allocate the buffers for the frame data
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		printf("Could not allocate frame data.\n");
		exit(1);
	}

	return picture;
}

void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = ost->st->codec;
	AVDictionary *opt = NULL;

	av_dict_copy(&opt, opt_arg, 0);

	/*open codec*/
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		printf("Could not open video codec: %s.\n", av_err2str(ret));
		exit(1);
	}

	//allocate and init a re-usable frame
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		printf("Could not allocate video frame.\n");
		exit(1);
	}

	/* If the output format is not YUV420P, then a temporary YUV420P 
	 * picture is needed too. It is then converted to the required output format */
	ost->tmp_frame = NULL;
	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!ost->tmp_frame) {
			printf("Could not allocated temporay picture.\n");
			exit(1);
		}
	}
}


// alloc audio frame
static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples) {
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame) {
		printf("Error allocatiing an aduio frame.\n");
		exit(1);
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			printf("Error allocatiing an audio buffer.\n");
			exit(1);
		}
	}

	return frame;
}

void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionaryEntry *opt_arg) {
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;

	c = ost->st->codec;

	//open audio codec
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		printf("Could not open audio codec: %s.\n", av_err2str(ret));
		exit(1);
	}

	// init singel generator
	ost->t = 0;
	ost->tincr  = 2 * M_PI * 110.0 / c->sample_rate;
	// increment frequency by 110 Hz per second
	ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
		nb_samples = 1000;
	} else {
		nb_samples = c->frame_size;  // 1152?
	}

	ost->frame = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);
	ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout, c->sample_rate, nb_samples);

	//create resample context
	ost->swr_ctx = swr_alloc();
	if (!ost->swr_ctx) {
		printf("Could not allocate resmapler context\n");
		exit(1);
	}

	//set options
	av_opt_set_int       (ost->swr_ctx, "in_channel_count", c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "in_sample_rate",   c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",    AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int       (ost->swr_ctx, "out_channel_count", c->channels,      0);
	av_opt_set_int       (ost->swr_ctx, "out_sample_rate",  c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",   c->sample_fmt,     0);

	//initialize the resampling context
	//³õÊ¼»¯Ê§°Ü???
	if ((ret = swr_init(ost->swr_ctx)) < 0) {
		printf("!!! Failed to initialize the resampling context, %s.\n", av_err2str(ret));
		exit(1);
	}
}

