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
 * wedit_demux.c — libavformat demuxer for reading source media.
 *
 * Clean-room implementation: uses libavformat as a SYSTEM LIBRARY
 * (format oracle), not as copied source. We read packets and extract
 * stream info. The timeline model (wedit_timeline) is our from-scratch code.
 */
#include "wedit_timeline.h"
#include "wedit_demux.h"

#ifdef WEDIT_HAS_FFMPEG

#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wedit_internal.h"

/* Opaque media handle — struct is defined in wedit_internal.h */

/*
 * Open a media file and extract stream info.
 * Returns opaque handle on success, NULL on failure.
 */
struct wedit_media* wedit_media_open(const char* path)
{
    if (!path) return NULL;

    struct wedit_media* media = calloc(1, sizeof(struct wedit_media));
    if (!media) return NULL;

    /* Open the container */
    if (avformat_open_input(&media->fmt_ctx, path, NULL, NULL) < 0) {
        free(media);
        return NULL;
    }

    /* Retrieve stream info */
    if (avformat_find_stream_info(media->fmt_ctx, NULL) < 0) {
        avformat_close_input(&media->fmt_ctx);
        free(media);
        return NULL;
    }

    media->video_stream_idx = -1;
    media->audio_stream_idx = -1;

    AVStream* st = NULL;
    for (unsigned int i = 0; i < media->fmt_ctx->nb_streams; i++) {
        st = media->fmt_ctx->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            media->video_stream_idx < 0) {
            media->video_stream_idx = (int)i;
            media->width = st->codecpar->width;
            media->height = st->codecpar->height;
            if (st->avg_frame_rate.den && st->avg_frame_rate.num) {
                media->fps_num = st->avg_frame_rate.num;
                media->fps_den = st->avg_frame_rate.den;
            }
        } else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                   media->audio_stream_idx < 0) {
            media->audio_stream_idx = (int)i;
        }
    }

    /* Get duration */
    if (media->fmt_ctx->duration != AV_NOPTS_VALUE) {
        media->duration = (double)media->fmt_ctx->duration / AV_TIME_BASE;
    }

    if (media->video_stream_idx < 0) {
        /* No video stream */
        avformat_close_input(&media->fmt_ctx);
        free(media);
        return NULL;
    }

    return media;
}

void wedit_media_init(void)
{
    avformat_network_init();
}

double wedit_media_duration(struct wedit_media* media)
{
    return media ? media->duration : 0.0;
}

int wedit_media_width(struct wedit_media* media)
{
    return media ? media->width : 0;
}

int wedit_media_height(struct wedit_media* media)
{
    return media ? media->height : 0;
}

void wedit_media_close(struct wedit_media* media)
{
    if (!media) return;
    if (media->fmt_ctx) {
        avformat_close_input(&media->fmt_ctx);
    }
    free(media);
}

#endif /* WEDIT_HAS_FFMPEG */
