#pragma once
#include "add_stream.h"
#include <libavutil\opt.h>


void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);

void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionaryEntry *opt_arg);