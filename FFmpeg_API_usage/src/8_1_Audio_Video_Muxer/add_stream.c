#include "add_stream.h"

void add_stream(OutputStream *out_st, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id) {
	AVCodecContext *c;
	int i;

	//find the encoder
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		printf("Could not find encoder for %s.\n", avcodec_get_name(codec_id));
		exit(1);
	}

	// create out stream (for video or audio)
	out_st->st = avformat_new_stream(oc, *codec);
	if (!out_st->st) {
		printf("Could not allocate stream");
		exit(1);
	}

	out_st->st->id = oc->nb_streams - 1;
	c = out_st->st->codec;

	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0]: AV_SAMPLE_FMT_FLTP;
		c->bit_rate    = 64000;
		c->sample_rate = 44100;
		if ((*codec)->supported_samplerates) {
			c->sample_rate = (*codec)->supported_samplerates[0];
			for (i = 0; (*codec)->supported_samplerates[i];i++) {
				if ((*codec)->supported_samplerates[i] == 44100)
					c->sample_rate = 44100;
			}
		}
		c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		if ((*codec)->channel_layouts) {
			for (i = 0; (*codec)->channel_layouts[i]; i++) {
				if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					c->channel_layout = AV_CH_LAYOUT_STEREO;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		out_st->st->time_base = (AVRational) {1, c->sample_rate};
		break;
	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;
		c->bit_rate = 600000;

		c->width = 1280;
		c->height = 720;

		/* timebase: This is the fundamental unit of time(in seconds) in terms of
		 * which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be 
		 * identical to 1. */
		out_st->st->time_base = (AVRational){1, STREAM_FRAME_RATE};
		c->time_base = out_st->st->time_base;

		c->gop_size = 12;   //emit one intra frame every 12 frames at most
		c->pix_fmt = STREAM_PIX_FMT;

		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			// for testing, 
			c->max_b_frames = 2;
		}

		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
		break;
	default:
		break;
	}

	// some formats want stream headers to be seprate
	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
}