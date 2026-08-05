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
 * wedit_decode.h — libavcodec decoder public API.
 * Opaque struct + minimal interface. Apps never see AVCodecContext.
 */
#ifndef WEDIT_DECODE_H
#define WEDIT_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WEDIT_HAS_FFMPEG

#include <libavcodec/avcodec.h>

/* Opaque decoder handle */
struct wedit_decoder;

/*
 * Create a decoder for a stream in a media file.
 * is_video: 1 for video stream, 0 for audio.
 * dst_width/dst_height/dst_pix_fmt: for video, target dimensions/pixfmt (0 to keep native).
 */
struct wedit_decoder* wedit_decoder_create(struct wedit_media* media,
                                           int stream_idx,
                                           int is_video,
                                           int dst_width,
                                           int dst_height,
                                           enum AVPixelFormat dst_pix_fmt);

/* Decode a packet into a frame. Returns 0 on success, -1 on error, 1 on EAGAIN/EOF. */
int wedit_decoder_decode_packet(struct wedit_decoder* dec,
                                AVPacket* packet,
                                AVFrame* frame);

void wedit_decoder_destroy(struct wedit_decoder* dec);

#else

/* Stubs when FFmpeg not available */
#define wedit_decoder_create(...) ((void*)0)
#define wedit_decoder_decode_packet(...) (-1)
#define wedit_decoder_destroy(m) ((void)0)

#endif /* WEDIT_HAS_FFMPEG */

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_DECODE_H */
