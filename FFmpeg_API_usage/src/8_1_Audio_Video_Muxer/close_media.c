
#include "close_media.h"

void close_stream(AVFormatContext *oc, OutputStream *ost) {
	avcodec_close(ost->st->codec);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}