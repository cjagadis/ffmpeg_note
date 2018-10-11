#include "write_media.h"

//prepare a dummy image
static void fill_yuv_image(AVFrame *frame, int frame_index, int width, int height, FILE *fp) {
	int x, y, i;
	i = frame_index;
	uint8_t tmp = 0;

	//Y
	for (y = 0; y < height;y++) {
		for (x = 0; x < width; x++) {
			tmp = frame->data[0][y*frame->linesize[0] + x] = x + y + i * 3;
			fwrite(&tmp, 1, 1, fp);
		}
	}

	//Cb and Cr
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			tmp = frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
			fwrite(&tmp, 1, 1, fp);
		}
	}

	//Cr
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			tmp = frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
			fwrite(&tmp, 1, 1, fp);
		}
	}

}



static AVFrame *get_video_frame(OutputStream *ost) {
	AVCodecContext *c = ost->st->codec;

	//check if we want to generate more frames
	if (av_compare_ts(ost->next_pts, ost->st->codec->time_base,
		STREAM_DURATION, (AVRational) { 1, 1 }) >= 0) {
		return NULL;
	}

	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		/* as we only generated a YUV420P picture, we must convert it to 
		 * the codec pixel format if needed */
		if (!ost->sws_ctx) {
			ost->sws_ctx = sws_getContext(c->width, c->height, 
										  AV_PIX_FMT_YUV420P,
				                          c->width, c->height,
				                          c->pix_fmt,
				                          SCALE_FLAGS, NULL, NULL, NULL);
			if (!ost->sws_ctx) {
				printf("Could not initialize the conversion context.\n");
				exit(1);
			}
		}
		fill_yuv_image(ost->tmp_frame, ost->next_pts, c->width, c->height, ost->fp_out);
		sws_scale(ost->sws_ctx,
			     (const uint8_t * const*)ost->tmp_frame->data, ost->tmp_frame->linesize,
			     0, c->height, ost->frame->data, ost->frame->linesize);
	} else {
		fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height, ost->fp_out);
	}

	ost->frame->pts = ost->next_pts++;
	return ost->frame;
}

static log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt) {
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
	printf("pts:%s, pts_time:%s, dts:%s, dts_time:%s, duration:%s, duration_time:%s, stream_index:%d\n",
		  av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		  av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		  av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		  pkt->stream_index);
}

static int write_frame(AVFormatContext *oc, const AVRational *time_base, AVStream *st, AVPacket *pkt){
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	//write the compressed frame to the media file
	log_packet(oc, pkt);

	return av_interleaved_write_frame(oc, pkt);
}

int write_video_frame(AVFormatContext *oc, OutputStream *ost) {
	int ret;
	AVCodecContext *c;
	AVFrame *frame;
	int got_packet = 0;
	AVPacket pkt = {0};

	c = ost->st->codec;

	frame = get_video_frame(ost);

	av_init_packet(&pkt);

	//encode image
	ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		printf("Error encoding video frame: %s.\n", av_err2str(ret));
		exit(1);
	}

	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
	}
	else {
		ret = 0;
	}

	if (ret < 0) {
		printf("Error while writing video frame: %s.\n", av_err2str(ret));
		exit(1);
	}

	return (frame || got_packet) ? 0 : 1;
}

static int audio_cnt = 0;
// prepare a 16 bit dummy audio frame of 'frame_size' samples and 
// nb_channels 
static AVFrame *get_audio_frame(OutputStream *ost) {
	AVFrame *frame = ost->tmp_frame;
	int j, i, v;
	int16_t *q = (int16_t*)frame->data[0];

	// check if we want to generate more frames
	if (av_compare_ts(ost->next_pts, ost->st->codec->time_base,
		STREAM_DURATION, (AVRational) { 1, 1 }) >= 0) {
		return NULL;
	}

	char audio_name[16] = {0};
	sprintf(audio_name, "%d.pcm", audio_cnt++);
	
	for (j = 0; j < frame->nb_samples; j++) {
		v = (int)(sin(ost->t) * 10000);
		for (i = 0; i < ost->st->codec->channels; i++) {
			*q++ = v;
			fwrite(&v,1, 1, ost->fp_out);
		}
		ost->t      += ost->tincr;
		ost->tincr  += ost->tincr2;
	}

	frame->pts = ost->next_pts;
	ost->next_pts += frame->nb_samples;


	return frame;
}


// encode one auido frame and send it to the muxer
// return 1 when encoding is finished, 0 otherweis
int write_audio_frame(AVFormatContext *oc, OutputStream *ost) {
	AVCodecContext *c;
	AVPacket pkt = {0};  // data and size must be 0
	AVFrame *frame;
	int ret;
	int got_packet;
	int dst_nb_samples;

	av_init_packet(&pkt);
	c = ost->st->codec;
	frame = get_audio_frame(ost);

	if (frame) {
		// convert samples from native format to destination codec format, using the resampler
		// compute destination number of samples
		dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
			                            c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);

		//when we pass a frame to the encoder, it may keep a reference to it internally
		// make sure we do not overwrite it here
		ret = av_frame_make_writable(ost->frame);
		if (ret < 0) {
			exit(1);
		}

		// convert to destination format
		ret = swr_convert(ost->swr_ctx, ost->frame->data,dst_nb_samples,
			              (const uint8_t **)frame->data, frame->nb_samples);
		if (ret < 0) {
			printf("error while converting");
			exit(1);
		}
		frame = ost->frame;

		frame->pts = av_rescale_q(ost->samples_count, (AVRational) {1, c->sample_rate}, c->time_base);
		ost->samples_count += dst_nb_samples;
	}

	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		printf("error encoding audio frame: %s.\n", av_err2str(ret));
		exit(1);
	}

	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			printf("Error while writing audio frame: %s.\n", av_err2str(ret));
			exit(1);
		}
	}

	return (frame || got_packet) ? 0: 1;
}