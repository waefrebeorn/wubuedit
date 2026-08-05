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
 * wedit_mux.c — libavformat muxer for writing output media containers.
 *
 * Clean-room: uses libavformat as system library (container oracle).
 * Writes MP4/MOV files from encoded packets.
 */
#include "wedit_demux.h"

#ifdef WEDIT_HAS_FFMPEG

#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wedit_internal.h"

/* Opaque muxer handle — struct is defined in wedit_internal.h */

struct wedit_muxer* wedit_muxer_create(const char* output_path,
                                       int has_video,
                                       int has_audio)
{
    if (!output_path) return NULL;

    AVFormatContext* fmt_ctx = NULL;
    AVOutputFormat* fmt = NULL;

    /* Allocate format context */
    fmt = (AVOutputFormat*)av_guess_format(NULL, output_path, NULL);
    if (!fmt) {
        /* Default to MP4 */
        fmt = (AVOutputFormat*)av_guess_format("mp4", NULL, NULL);
        if (!fmt) return NULL;
    }

    if (avformat_alloc_output_context2(&fmt_ctx, fmt, NULL, output_path) < 0) {
        return NULL;
    }

    struct wedit_muxer* mux = calloc(1, sizeof(struct wedit_muxer));
    if (!mux) {
        avformat_free_context(fmt_ctx);
        return NULL;
    }

    mux->fmt_ctx = fmt_ctx;
    mux->have_video = has_video;
    mux->have_audio = has_audio;
    mux->video_stream_idx = -1;
    mux->audio_stream_idx = -1;

    /* Open output file */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, output_path, AVIO_FLAG_WRITE) < 0) {
            avformat_free_context(fmt_ctx);
            free(mux);
            return NULL;
        }
    }

    return mux;
}

int wedit_muxer_add_video_stream(struct wedit_muxer* mux,
                                 AVCodecContext* codec_ctx)
{
    if (!mux || !mux->fmt_ctx) return -1;

    AVStream* st = avformat_new_stream(mux->fmt_ctx, NULL);
    if (!st) return -1;

    mux->video_stream_idx = (int)mux->fmt_ctx->nb_streams - 1;
    st->id = mux->video_stream_idx;
    st->time_base = codec_ctx->time_base;
    st->avg_frame_rate = codec_ctx->framerate;
    st->r_frame_rate = codec_ctx->framerate;
    st->duration = 0;
    st->nb_frames = 0;

    /* Set codec parameters */
    if (codec_ctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER) {
        mux->fmt_ctx->flags |= AVFMT_GLOBALHEADER;
    }
    avcodec_parameters_from_context(st->codecpar, codec_ctx);

    return mux->video_stream_idx;
}

int wedit_muxer_add_audio_stream(struct wedit_muxer* mux,
                                 AVCodecContext* codec_ctx)
{
    if (!mux || !mux->fmt_ctx) return -1;

    AVStream* st = avformat_new_stream(mux->fmt_ctx, NULL);
    if (!st) return -1;

    mux->audio_stream_idx = (int)mux->fmt_ctx->nb_streams - 1;
    st->id = mux->audio_stream_idx;
    st->time_base = codec_ctx->time_base;

    avcodec_parameters_from_context(st->codecpar, codec_ctx);

    return mux->audio_stream_idx;
}

int wedit_muxer_write_header(struct wedit_muxer* mux)
{
    if (!mux || !mux->fmt_ctx) return -1;
    return avformat_write_header(mux->fmt_ctx, NULL);
}

int wedit_muxer_write_packet(struct wedit_muxer* mux, AVPacket* pkt,
                             int stream_idx)
{
    if (!mux || !mux->fmt_ctx || !pkt) return -1;

    /* Rescale packet timestamps to stream time base */
    pkt->stream_index = stream_idx;
    if (pkt->stream_index == mux->video_stream_idx &&
        mux->fmt_ctx->streams[stream_idx]->time_base.num) {
        /* Timestamps already set by encoder */
    }

    return av_interleaved_write_frame(mux->fmt_ctx, pkt);
}

void wedit_muxer_finalize(struct wedit_muxer* mux)
{
    if (!mux || !mux->fmt_ctx) return;

    /* Write trailer */
    av_write_trailer(mux->fmt_ctx);

    /* Close output file if one was opened */
    if (!(mux->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&mux->fmt_ctx->pb);
    }

    avformat_free_context(mux->fmt_ctx);
    free(mux);
}

#endif /* WEDIT_HAS_FFMPEG */
