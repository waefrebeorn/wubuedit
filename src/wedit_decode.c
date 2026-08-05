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
 * wedit_decode.c — libavcodec decoder for extracting raw video/audio frames.
 *
 * Clean-room: uses libavcodec as system library (codec oracle),
 * not copied source. Decodes packets from the demuxer into raw frames.
 */
#include "wedit_demux.h"

#ifdef WEDIT_HAS_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include "wedit_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Opaque decoder handle — struct is defined in wedit_internal.h */

/*
 * Create a decoder context for a media stream.
 * Returns decoder handle or NULL on failure.
 */
struct wedit_decoder* wedit_decoder_create(struct wedit_media* media,
                                           int stream_idx,
                                           int is_video,
                                           int dst_width,
                                           int dst_height,
                                           enum AVPixelFormat dst_pix_fmt)
{
    if (!media || !media->fmt_ctx) return NULL;
    if (stream_idx < 0 || stream_idx >= (int)media->fmt_ctx->nb_streams) return NULL;

    AVStream* st = media->fmt_ctx->streams[stream_idx];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) return NULL;

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) return NULL;

    if (avcodec_parameters_to_context(codec_ctx, st->codecpar) < 0) {
        avcodec_free_context(&codec_ctx);
        return NULL;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        avcodec_free_context(&codec_ctx);
        return NULL;
    }

    struct wedit_decoder* dec = calloc(1, sizeof(struct wedit_decoder));
    if (!dec) {
        avcodec_free_context(&codec_ctx);
        return NULL;
    }

    dec->codec_ctx = codec_ctx;
    dec->stream_idx = stream_idx;
    dec->is_video = is_video;
    dec->dst_width = dst_width;
    dec->dst_height = dst_height;
    dec->dst_pix_fmt = dst_pix_fmt;

    if (is_video && dst_width > 0 && dst_height > 0) {
        dec->sws_ctx = sws_getContext(
            codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
            dst_width, dst_height, dst_pix_fmt,
            SWS_BILINEAR, NULL, NULL, NULL);
    }

    return dec;
}

/* Decode a single packet into a frame. Returns 0 on success, -1 on error, 1 on EAGAIN/EOF. */
int wedit_decoder_decode_packet(struct wedit_decoder* dec,
                                AVPacket* packet,
                                AVFrame* frame)
{
    if (!dec || !packet || !frame) return -1;

    int ret = avcodec_send_packet(dec->codec_ctx, packet);
    if (ret < 0) return -1;

    ret = avcodec_receive_frame(dec->codec_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 1;
    if (ret < 0) return -1;

    /* If video decoder and scaling context exists, convert pixel format */
    if (dec->is_video && dec->sws_ctx && dec->dst_width > 0) {
        AVFrame* scaled = av_frame_alloc();
        if (scaled) {
            scaled->width = dec->dst_width;
            scaled->height = dec->dst_height;
            scaled->format = (int)dec->dst_pix_fmt;
            ret = av_image_alloc(scaled->data, scaled->linesize,
                                 dec->dst_width, dec->dst_height,
                                 dec->dst_pix_fmt, 32);
            if (ret > 0) {
                sws_scale(dec->sws_ctx,
                          (const uint8_t* const*)frame->data,
                          frame->linesize, 0,
                          dec->codec_ctx->height,
                          scaled->data, scaled->linesize);
                av_frame_move_ref(frame, scaled);
            }
            av_frame_free(&scaled);
        }
    }

    return 0;
}

void wedit_decoder_destroy(struct wedit_decoder* dec)
{
    if (!dec) return;
    if (dec->sws_ctx) sws_freeContext(dec->sws_ctx);
    if (dec->codec_ctx) {
        avcodec_free_context(&dec->codec_ctx);
    }
    free(dec);
}

#endif /* WEDIT_HAS_FFMPEG */
