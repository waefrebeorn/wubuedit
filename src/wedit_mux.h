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
 * wedit_mux.h — libavformat muxer public API.
 * Opaque struct + minimal interface. Apps never see AVFormatContext.
 */
#ifndef WEDIT_MUX_H
#define WEDIT_MUX_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WEDIT_HAS_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

struct wedit_muxer;

/* Create muxer for output file. has_video/has_audio flags expected streams. */
struct wedit_muxer* wedit_muxer_create(const char* output_path,
                                       int has_video,
                                       int has_audio);

/* Add a video stream to the output. Returns stream index (>=0) or -1. */
int wedit_muxer_add_video_stream(struct wedit_muxer* mux,
                                 AVCodecContext* codec_ctx);

/* Add an audio stream to the output. Returns stream index (>=0) or -1. */
int wedit_muxer_add_audio_stream(struct wedit_muxer* mux,
                                 AVCodecContext* codec_ctx);

/* Write file header (call before writing packets). */
int wedit_muxer_write_header(struct wedit_muxer* mux);

/* Write an encoded packet to the output. */
int wedit_muxer_write_packet(struct wedit_muxer* mux, AVPacket* pkt,
                             int stream_idx);

/* Finalize: write trailer, close file, free context. */
void wedit_muxer_finalize(struct wedit_muxer* mux);

#else

/* Stubs */
#define wedit_muxer_create(...) ((void*)0)
#define wedit_muxer_add_video_stream(...) (-1)
#define wedit_muxer_add_audio_stream(...) (-1)
#define wedit_muxer_write_header(m) (-1)
#define wedit_muxer_write_packet(m, p, i) (-1)
#define wedit_muxer_finalize(m) ((void)0)

#endif /* WEDIT_HAS_FFMPEG */

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_MUX_H */
