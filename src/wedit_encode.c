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
 * wedit_encode.c — libavcodec encoder for writing encoded video/audio.
 *
 * Clean-room: uses libavcodec as system library (codec oracle).
 * Encoded frames are then passed to the muxer (wedit_mux).
 */
#include "wedit_demux.h"

#ifdef WEDIT_HAS_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <stdio.h>
#include <stdlib.h>
#include "wedit_internal.h"

/* Opaque encoder handle — struct is defined in wedit_internal.h */

struct wedit_encoder* wedit_encoder_create_video(
    enum AVCodecID codec_id,
    int width, int height,
    int fps_num, int fps_den,
    int crf,
    const char* preset)
{
    (void)crf; /* CRF is set via av_opt_set below */
    const AVCodec* codec = avcodec_find_encoder(codec_id);
    if (!codec) return NULL;

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) return NULL;

    ctx->width = width;
    ctx->height = height;
    ctx->time_base = (AVRational){fps_den, fps_num};
    ctx->framerate = (AVRational){fps_num, fps_den};
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->gop_size = fps_num / fps_den * 2;  /* 2-second GOP */
    ctx->max_b_frames = 0;
    ctx->bit_rate = 0;  /* CBR off; CRF controls quality */

    if (codec_id == AV_CODEC_ID_H264) {
        /* Set H.264 preset via av_opt_set */
        av_opt_set(ctx->priv_data, "preset", preset ? preset : "medium", 0);
        av_opt_set(ctx->priv_data, "crf", "20", 0);
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(ctx, codec, NULL) < 0) {
        avcodec_free_context(&ctx);
        return NULL;
    }

    struct wedit_encoder* enc = calloc(1, sizeof(struct wedit_encoder));
    if (!enc) {
        avcodec_free_context(&ctx);
        return NULL;
    }
    enc->codec_ctx = ctx;
    enc->is_video = 1;
    enc->frame_count = 0;
    return enc;
}

struct wedit_encoder* wedit_encoder_create_audio(
    enum AVCodecID codec_id,
    int sample_rate,
    int channels,
    int bit_rate)
{
    const AVCodec* codec = avcodec_find_encoder(codec_id);
    if (!codec) return NULL;

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) return NULL;

    ctx->sample_rate = sample_rate;
    av_channel_layout_default(&ctx->ch_layout, channels);
    ctx->sample_fmt = codec->sample_fmts[0];
    ctx->bit_rate = bit_rate;
    ctx->time_base = (AVRational){1, sample_rate};

    if (avcodec_open2(ctx, codec, NULL) < 0) {
        avcodec_free_context(&ctx);
        return NULL;
    }

    struct wedit_encoder* enc = calloc(1, sizeof(struct wedit_encoder));
    if (!enc) {
        avcodec_free_context(&ctx);
        return NULL;
    }
    enc->codec_ctx = ctx;
    enc->is_video = 0;
    enc->frame_count = 0;
    return enc;
}

AVCodecContext* wedit_encoder_context(struct wedit_encoder* enc)
{
    return enc ? enc->codec_ctx : NULL;
}

int wedit_encoder_send_frame(struct wedit_encoder* enc, AVFrame* frame)
{
    if (!enc || !enc->codec_ctx) return -1;

    /* Set frame PTS */
    frame->pts = enc->frame_count;
    enc->frame_count++;

    int ret = avcodec_send_frame(enc->codec_ctx, frame);
    if (ret < 0) return -1;
    return 0;
}

int wedit_encoder_receive_packet(struct wedit_encoder* enc, AVPacket* pkt)
{
    if (!enc || !enc->codec_ctx) return -1;
    return avcodec_receive_packet(enc->codec_ctx, pkt);
}

void wedit_encoder_flushing(struct wedit_encoder* enc)
{
    if (!enc || !enc->codec_ctx) return;
    avcodec_send_frame(enc->codec_ctx, NULL);  /* flush */
}

void wedit_encoder_destroy(struct wedit_encoder* enc)
{
    if (!enc) return;
    if (enc->codec_ctx) {
        avcodec_free_context(&enc->codec_ctx);
    }
    free(enc);
}

#endif /* WEDIT_HAS_FFMPEG */
