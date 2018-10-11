#pragma once

#include "open_media.h"
#include <libavutil\avutil.h>
#include <libavutil\timestamp.h>
#include <libavutil\avassert.h>

int write_video_frame(AVFormatContext *oc, OutputStream *ost);
int write_audio_frame(AVFormatContext *oc, OutputStream *ost);