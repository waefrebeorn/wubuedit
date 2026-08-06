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
 * wedit_clip_source.c — Decode a media file to RGBA frames at arbitrary timestamps.
 *
 * This module bridges the demux/decode layer (wedit_demux, wedit_decode) to
 * the render pipeline (wedit_render). It wraps the opaque wedit_media and
 * wedit_decoder handles and provides a simple seek-and-decode API that
 * returns RGBA pixel buffers ready for compositing.
 *
 * Frame caching: if seek() is called with the same (or very close) timestamp
 * as the last call, the cached frame is returned without re-decoding.
 */

#include "wedit_clip_source.h"
#include "wedit_demux.h"
#include "wedit_decode.h"
#include "wedit_internal.h"

#ifdef WEDIT_HAS_FFMPEG

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct wedit_clip_source {
    struct wedit_media* media;       /* demuxer handle */
    struct wedit_decoder* decoder;   /* video decoder handle */
    int video_stream_idx;            /* stream index for video */

    /* Output dimensions */
    int dst_width;
    int dst_height;

    /* Frame cache — last decoded frame */
    unsigned char* rgba_cache;
    double cached_timestamp;      /* timestamp of cached frame, -1 if empty */
};

wedit_clip_source_t* wedit_clip_source_open(const char* src_path,
                                             int out_width, int out_height)
{
    if (!src_path || out_width < 0 || out_height < 0) {
        return NULL;
    }

    /* Open the media file via our demuxer wrapper */
    struct wedit_media* media = wedit_media_open(src_path);
    if (!media) {
        fprintf(stderr, "[wedit_clip_source] Failed to open media: %s\n", src_path);
        return NULL;
    }

    /* Find the video stream index */
    int vidx = -1;
    /* We need stream info from the internal fmt_ctx */
    AVFormatContext* fmt_ctx = NULL;
    /* Access internal struct — it's in wedit_internal.h which we include */
    fmt_ctx = media->fmt_ctx;
    if (!fmt_ctx) {
        wedit_media_close(media);
        return NULL;
    }

    /* Find first video stream */
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vidx = (int)i;
            break;
        }
    }
    if (vidx < 0) {
        fprintf(stderr, "[wedit_clip_source] No video stream in %s\n", src_path);
        wedit_media_close(media);
        return NULL;
    }

    /* Determine native dimensions if output dims are 0 */
    int native_w = media->width;
    int native_h = media->height;
    int target_w = out_width > 0 ? out_width : native_w;
    int target_h = out_height > 0 ? out_height : native_h;

    /* Create video decoder targeting RGBA output */
    struct wedit_decoder* decoder = wedit_decoder_create(
        media, vidx, 1 /* is_video */,
        target_w, target_h, AV_PIX_FMT_RGBA);

    if (!decoder) {
        fprintf(stderr, "[wedit_clip_source] Failed to create decoder for %s\n", src_path);
        wedit_media_close(media);
        return NULL;
    }

    /* Allocate RGBA cache buffer */
    size_t buf_size = (size_t)target_w * target_h * 4;
    unsigned char* rgba_cache = malloc(buf_size);
    if (!rgba_cache) {
        wedit_decoder_destroy(decoder);
        wedit_media_close(media);
        return NULL;
    }

    wedit_clip_source_t* src = calloc(1, sizeof(wedit_clip_source_t));
    if (!src) {
        free(rgba_cache);
        wedit_decoder_destroy(decoder);
        wedit_media_close(media);
        return NULL;
    }

    src->media = media;
    src->decoder = decoder;
    src->video_stream_idx = vidx;
    src->dst_width = target_w;
    src->dst_height = target_h;
    src->rgba_cache = rgba_cache;
    src->cached_timestamp = -1.0;

    return src;
}

int wedit_clip_source_seek(wedit_clip_source_t* src, double timestamp)
{
    if (!src || timestamp < 0.0) {
        return -1;
    }

    /* Cache check: if we already have this frame, return immediately */
    /* Cache hit: if we've already decoded this exact timestamp, return */
    if (src->cached_timestamp >= 0.0 &&
        fabs(src->cached_timestamp - timestamp) < 0.01) {
        return 0;
    }

    AVFormatContext* fmt_ctx = src->media->fmt_ctx;
    if (!fmt_ctx) {
        return -1;
    }

    /* Forward-only optimization: if the requested timestamp is just slightly
     * ahead of the cached timestamp (within ~1 second), skip the expensive
     * av_seek_frame + avcodec_flush_buffers and just decode forward.
     * This is critical for frame-by-frame rendering at 30fps where each
     * frame is only 0.033s apart. */
    int forward_decode = 0;
    if (src->cached_timestamp >= 0.0 &&
        timestamp > src->cached_timestamp &&
        (timestamp - src->cached_timestamp) < 1.0) {
        forward_decode = 1;
    }

    /* Convert timestamp to stream time_base */
    AVRational time_base = fmt_ctx->streams[src->video_stream_idx]->time_base;
    int64_t target_pts = (int64_t)(timestamp * time_base.den / time_base.num);

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    if (!forward_decode) {
        /* Seek to nearest keyframe before target */
        if (av_seek_frame(fmt_ctx, src->video_stream_idx, target_pts,
                          AVSEEK_FLAG_BACKWARD) < 0) {
            /* Fallback: seek to beginning */
            if (av_seek_frame(fmt_ctx, src->video_stream_idx, 0, 0) < 0) {
                av_packet_free(&packet);
                av_frame_free(&frame);
                return -1;
            }
        }
        /* Flush decoder to clear any buffered frames */
        if (src->decoder->codec_ctx) {
            avcodec_flush_buffers(src->decoder->codec_ctx);
        }
    }

    if (!packet || !frame) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        return -1;
    }

    int64_t current_pts = AV_NOPTS_VALUE;

    int found = 0;
    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if ((int)packet->stream_index != src->video_stream_idx) {
            av_packet_unref(packet);
            continue;
        }

        if (wedit_decoder_decode_packet(src->decoder, packet, frame) == 0) {
            /* Successfully decoded a frame */
            current_pts = frame->pts;
            if (frame->pts == AV_NOPTS_VALUE) {
                current_pts = packet->pts;
            }

            /* Copy RGBA pixels to cache.
             * The decoder's decode_packet already converted frame->data to
             * dst_pix_fmt (RGBA) via its internal sws_ctx.
             * For packed RGBA (1 plane), linesize[0] = width * 4. */
            if (frame->data[0]) {
                size_t row_size = (size_t)src->dst_width * 4;
                if (frame->linesize[0] == (int)row_size) {
                    memcpy(src->rgba_cache, frame->data[0],
                           (size_t)src->dst_width * src->dst_height * 4);
                } else {
                    /* Row-by-row copy for non-standard linesize */
                    for (int y = 0; y < src->dst_height; y++) {
                        memcpy(src->rgba_cache + (size_t)y * row_size,
                               frame->data[0] + (size_t)y * frame->linesize[0],
                               row_size);
                    }
                }
            }

            found = 1;
            src->cached_timestamp = timestamp;

            /* If we've reached or passed the target, stop */
            if (current_pts >= target_pts) {
                break;
            }
        }
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    av_frame_free(&frame);

    return found ? 0 : -1;
}

const unsigned char* wedit_clip_source_get_pixels(wedit_clip_source_t* src)
{
    if (!src) {
        return NULL;
    }
    return src->cached_timestamp >= 0.0 ? src->rgba_cache : NULL;
}

int wedit_clip_source_width(const wedit_clip_source_t* src)
{
    return src ? src->dst_width : 0;
}

int wedit_clip_source_height(const wedit_clip_source_t* src)
{
    return src ? src->dst_height : 0;
}

double wedit_clip_source_timestamp(const wedit_clip_source_t* src)
{
    return src ? src->cached_timestamp : -1.0;
}

void wedit_clip_source_close(wedit_clip_source_t* src)
{
    if (!src) {
        return;
    }
    if (src->rgba_cache) {
        free(src->rgba_cache);
    }
    if (src->decoder) {
        wedit_decoder_destroy(src->decoder);
    }
    if (src->media) {
        wedit_media_close(src->media);
    }
    free(src);
}

#else /* !WEDIT_HAS_FFMPEG */

/* Stub implementations when FFmpeg is not available */
wedit_clip_source_t* wedit_clip_source_open(const char* src_path,
                                             int out_width, int out_height)
{
    (void)src_path; (void)out_width; (void)out_height;
    return NULL;
}

int wedit_clip_source_seek(wedit_clip_source_t* src, double ts)
{
    (void)src; (void)ts;
    return -1;
}

const unsigned char* wedit_clip_source_get_pixels(wedit_clip_source_t* src)
{
    (void)src;
    return NULL;
}

int wedit_clip_source_width(const wedit_clip_source_t* src)
{
    (void)src;
    return 0;
}

int wedit_clip_source_height(const wedit_clip_source_t* src)
{
    (void)src;
    return 0;
}

double wedit_clip_source_timestamp(const wedit_clip_source_t* src)
{
    (void)src;
    return -1.0;
}

void wedit_clip_source_close(wedit_clip_source_t* src)
{
    (void)src;
}

#endif /* WEDIT_HAS_FFMPEG */
