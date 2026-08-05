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
 * wedit_encode.h — libavcodec encoder public API.
 * Opaque struct + minimal interface.
 */
#ifndef WEDIT_ENCODE_H
#define WEDIT_ENCODE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WEDIT_HAS_FFMPEG

#include <libavcodec/avcodec.h>

struct wedit_encoder;

/* Create a video encoder (H.264 by default) */
struct wedit_encoder* wedit_encoder_create_video(
    enum AVCodecID codec_id,
    int width, int height,
    int fps_num, int fps_den,
    int crf,
    const char* preset);

/* Create an audio encoder (AAC by default) */
struct wedit_encoder* wedit_encoder_create_audio(
    enum AVCodecID codec_id,
    int sample_rate,
    int channels,
    int bit_rate);

/* Get the underlying codec context (for stream setup in muxer) */
AVCodecContext* wedit_encoder_context(struct wedit_encoder* enc);

/* Send a frame for encoding */
int wedit_encoder_send_frame(struct wedit_encoder* enc, AVFrame* frame);

/* Receive an encoded packet (poll after send_frame) */
int wedit_encoder_receive_packet(struct wedit_encoder* enc, AVPacket* pkt);

/* Flush the encoder (call after all frames sent) */
void wedit_encoder_flushing(struct wedit_encoder* enc);

/* Destroy encoder */
void wedit_encoder_destroy(struct wedit_encoder* enc);

#else

/* Stubs */
#define wedit_encoder_create_video(...) ((void*)0)
#define wedit_encoder_create_audio(...) ((void*)0)
#define wedit_encoder_context(e) ((void*)0)
#define wedit_encoder_send_frame(e, f) (-1)
#define wedit_encoder_receive_packet(e, p) (-1)
#define wedit_encoder_flushing(e) ((void)0)
#define wedit_encoder_destroy(e) ((void)0)

#endif /* WEDIT_HAS_FFMPEG */

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_ENCODE_H */
