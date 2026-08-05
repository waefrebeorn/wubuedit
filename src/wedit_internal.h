/*
 * This file is part of WubuEdit — a SLERM ground-up C11 video editor.
 * Copyright (c) 2026 Waefrebeorn.
 *
 * Licensed under the Waefrebeorn Umbrella License v3.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   https://github.com/waefrebeorn/waefrebeorn-umbrella-license
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * wedit_internal.h — Internal struct definitions for wubuedit.
 * This file is NOT part of the public API and is never installed.
 * It exposes struct layouts to modules within the same library build.
 *
 * Per soul.md §10: no monoliths, but internal headers are fine within a lib.
 */
#ifndef WEDIT_INTERNAL_H
#define WEDIT_INTERNAL_H

#ifdef WEDIT_HAS_FFMPEG

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

/* Media handle (demuxer) */
struct wedit_media {
    AVFormatContext* fmt_ctx;
    int video_stream_idx;
    int audio_stream_idx;
    double duration;
    int width;
    int height;
    int fps_num;
    int fps_den;
};

/* Decoder handle */
struct wedit_decoder {
    AVCodecContext* codec_ctx;
    struct SwsContext* sws_ctx;
    int stream_idx;
    int is_video;
    int dst_width;
    int dst_height;
    enum AVPixelFormat dst_pix_fmt;
};

/* Encoder handle */
struct wedit_encoder {
    AVCodecContext* codec_ctx;
    int is_video;
    int frame_count;
};

/* Muxer handle */
struct wedit_muxer {
    AVFormatContext* fmt_ctx;
    int video_stream_idx;
    int audio_stream_idx;
    int have_video;
    int have_audio;
};

#endif /* WEDIT_HAS_FFMPEG */

#endif /* WEDIT_INTERNAL_H */
