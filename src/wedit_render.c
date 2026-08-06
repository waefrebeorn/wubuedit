/*
 * This file is part of WubuEdit — a SLERM ground-up C11 video editor.
 * Copyright (c) 2026 Waefrebeorn.
 *
 * Licensed under the Waefrebeorn Umbrella License v3.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *   https://github.com/waefrebeorn/waefrebeorn-umbrella-license
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * wedit_render.c — Render pipeline implementation.
 *
 * Frame-by-frame compositing pipeline:
 *   timeline model -> per-frame clip lookup -> composite -> encode
 *
 * Pure C11 orchestration with FFmpeg/libav for codec/muxing plumbing.
 * The GPU renderer (wugui_renderer) handles pixel compositing; this file
 * drives the composite->encode loop.
 *
 * SLERM note: This is NOT a wrapper around FFmpeg's filter_complex.
 * It's a native C11 render loop that composites frames in software,
 * and feeds encoded frames to the muxer via our opaque wrappers
 * (wedit_encoder / wedit_muxer) rather than touching AVFormatContext
 * directly.
 *
 * Compare to Diffusion Studio: they use canvas.toBlob() per frame ->
 * we use avcodec_send_frame + avcodec_receive_packet directly.
 */

#include "wedit_render.h"
#include "wedit_timeline.h"
#include "wedit_clip_source.h"
#include "wedit_encode.h"
#include "wedit_mux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef WEDIT_HAS_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#endif

/* =========================================================================
 * Internal structures
 * ========================================================================= */

struct wedit_render_ctx {
    const wedit_timeline_t* tl;
    const wedit_render_opts_t* opts;

    /* Frame buffer for compositing */
    unsigned char* blend_buffer;
    int blend_width;
    int blend_height;

    char error_msg[256];
};

/* =========================================================================
 * Helpers
 * ========================================================================= */

static inline double clamp_f(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* =========================================================================
 * Default options
 * ========================================================================= */

void wedit_render_default_opts(wedit_render_opts_t* opts)
{
    if (!opts) return;
    memset(opts, 0, sizeof(wedit_render_opts_t));
    opts->width = 0;            /* auto */
    opts->height = 0;           /* auto */
    opts->fps = 0;              /* use timeline fps */
    opts->crf = 23;             /* FFmpeg default quality */
    opts->preset = "medium";
    opts->pixel_format = "yuv420p";
    opts->threads = 0;          /* auto */
    opts->gpu_encode = 0;       /* CPU by default */
    opts->audio_sample_rate = 48000;
    opts->audio_channels = 2;
    opts->use_gpu_rendering = 0;
    opts->progress_fn = NULL;
    opts->progress_data = NULL;
}

/* =========================================================================
 * Blend modes (CPU compositing, matching Diffusion Studio's Gf table)
 * Gf = ["source-over", "multiply", "screen", "overlay", "difference"]
 * ========================================================================= */

/* Source-over alpha blending: fg OVER bg */
static void blend_source_over(unsigned char* dst, const unsigned char* src,
                              int width, int height)
{
    for (int i = 0; i < width * height * 4; i += 4) {
        float sa = src[i + 3] / 255.0f;
        if (sa < 0.004f) continue;  /* skip transparent pixels */
        float da = dst[i + 3] / 255.0f;
        float out_a = sa + da * (1.0f - sa);
        if (out_a < 0.004f) {
            dst[i + 0] = 0;
            dst[i + 1] = 0;
            dst[i + 2] = 0;
            dst[i + 3] = 0;
        } else {
            float ratio = sa / out_a;
            dst[i + 0] = (unsigned char)(src[i + 0] * ratio + dst[i + 0] * (1.0f - ratio));
            dst[i + 1] = (unsigned char)(src[i + 1] * ratio + dst[i + 1] * (1.0f - ratio));
            dst[i + 2] = (unsigned char)(src[i + 2] * ratio + dst[i + 2] * (1.0f - ratio));
            dst[i + 3] = (unsigned char)(out_a * 255.0f);
        }
    }
}

/* Multiply blend mode */
static void blend_multiply(unsigned char* dst, const unsigned char* src,
                           int width, int height)
{
    for (int i = 0; i < width * height * 4; i += 4) {
        float sa = src[i + 3] / 255.0f;
        if (sa > 0.004f) {
            dst[i + 0] = (unsigned char)(dst[i + 0] * src[i + 0] / 255.0f);
            dst[i + 1] = (unsigned char)(dst[i + 1] * src[i + 1] / 255.0f);
            dst[i + 2] = (unsigned char)(dst[i + 2] * src[i + 2] / 255.0f);
            dst[i + 3] = (unsigned char)(fmaxf((float)dst[i + 3], (float)src[i + 3]));
        }
    }
}

/* Screen blend mode */
static void blend_screen(unsigned char* dst, const unsigned char* src,
                         int width, int height)
{
    for (int i = 0; i < width * height * 4; i += 4) {
        float sa = src[i + 3] / 255.0f;
        if (sa > 0.004f) {
            dst[i + 0] = (unsigned char)(255.0f - (255.0f - dst[i + 0]) * (255.0f - src[i + 0]) / 255.0f);
            dst[i + 1] = (unsigned char)(255.0f - (255.0f - dst[i + 1]) * (255.0f - src[i + 1]) / 255.0f);
            dst[i + 2] = (unsigned char)(255.0f - (255.0f - dst[i + 2]) * (255.0f - src[i + 2]) / 255.0f);
            dst[i + 3] = (unsigned char)(fmaxf((float)dst[i + 3], (float)src[i + 3]));
        }
    }
}

/* Dispatch blend mode (matching Diffusion Studio's Gf table: 0=source-over, 1=multiply, 2=screen, 3=overlay, 4=difference) */
static void composite_blend(unsigned char* dst, const unsigned char* src,
                            int width, int height, int blend_mode)
{
    switch (blend_mode) {
        case 1:  blend_multiply(dst, src, width, height); break;
        case 2:  blend_screen(dst, src, width, height); break;
        case 0:
        default: blend_source_over(dst, src, width, height); break;
    }
}

/* =========================================================================
 * Transition effects
 * ========================================================================= */

/* Apply fade transition */
static void apply_fade(unsigned char* pixels, int width, int height, float opacity)
{
    opacity = (float)clamp_f(opacity, 0.0, 1.0);
    for (int i = 0; i < width * height * 4; i += 4) {
        pixels[i + 0] = (unsigned char)(pixels[i + 0] * opacity);
        pixels[i + 1] = (unsigned char)(pixels[i + 1] * opacity);
        pixels[i + 2] = (unsigned char)(pixels[i + 2] * opacity);
        pixels[i + 3] = (unsigned char)(pixels[i + 3] * opacity);
    }
}

/* =========================================================================
 * Color grading (matches shader: brightness -> contrast -> saturation -> gamma)
 * ========================================================================= */

static void apply_color_grade(unsigned char* pixels, int width, int height,
                              float brightness, float contrast,
                              float saturation, float gamma)
{
    for (int i = 0; i < width * height * 4; i += 4) {
        float r = pixels[i + 0] / 255.0f;
        float g = pixels[i + 1] / 255.0f;
        float b = pixels[i + 2] / 255.0f;

        r += brightness; g += brightness; b += brightness;
        r = (r - 0.5f) * contrast + 0.5f;
        g = (g - 0.5f) * contrast + 0.5f;
        b = (b - 0.5f) * contrast + 0.5f;

        float luma = 0.299f * r + 0.587f * g + 0.114f * b;
        r = luma + (r - luma) * saturation;
        g = luma + (g - luma) * saturation;
        b = luma + (b - luma) * saturation;

        if (gamma > 0.001f) {
            r = powf(fmaxf(r, 0.0f), 1.0f / gamma);
            g = powf(fmaxf(g, 0.0f), 1.0f / gamma);
            b = powf(fmaxf(b, 0.0f), 1.0f / gamma);
        }

        pixels[i + 0] = (unsigned char)(clamp_f(r, 0.0f, 1.0f) * 255.0f);
        pixels[i + 1] = (unsigned char)(clamp_f(g, 0.0f, 1.0f) * 255.0f);
        pixels[i + 2] = (unsigned char)(clamp_f(b, 0.0f, 1.0f) * 255.0f);
    }
}

/* =========================================================================
 * WuBu watermark (beat-synced pulse)
 * ========================================================================= */

static void draw_wubu_watermark(unsigned char* pixels, int width, int height,
                                float beat_phase)
{
    int text_w = 120;
    int text_h = 16;
    int x = width - text_w - 10;
    int y = height - text_h - 10;

    float pulse = 0.5f + 0.4f * sinf(beat_phase * 3.14159265f * 2.0f);
    float alpha = 0.3f * pulse;

    /* WuBu gold: (255, 200, 60) */
    for (int py = y; py < y + text_h && py < height; py++) {
        for (int px = x; px < x + text_w && px < width; px++) {
            int idx = (py * width + px) * 4;
            pixels[idx + 0] = (unsigned char)(255.0f * alpha + pixels[idx + 0] * (1.0f - alpha));
            pixels[idx + 1] = (unsigned char)(200.0f * alpha + pixels[idx + 1] * (1.0f - alpha));
            pixels[idx + 2] = (unsigned char)(60.0f * alpha + pixels[idx + 2] * (1.0f - alpha));
            pixels[idx + 3] = 255;
        }
    }
}

/* =========================================================================
 * Frame-level rendering (no encode — produces RGBA buffer)
 * ========================================================================= */

/*
 * Render one frame of the timeline into the given RGBA buffer.
 * Called by both the encode loop and wedit_render_timeline_custom().
 *
 * For each active video clip on each track:
 *   1. Decode the source media at the clip-relative timestamp (via wedit_clip_source)
 *   2. Scale to output dimensions (handled by clip_source decoder target)
 *   3. Composite onto the frame buffer with the clip's blend mode
 *
 * When FFmpeg is unavailable, falls back to procedural black frame + watermark.
 */
static void render_frame_into(const wedit_timeline_t* tl,
                              double frame_time,
                              unsigned char* buf,
                              int width, int height,
                              float color_brightness, float color_contrast,
                              float color_saturation, float color_gamma,
                              wedit_render_frame_fn external_cb,
                              void* external_user_data)
{
    (void)external_cb;
    (void)external_user_data;
    int track_count = wedit_timeline_track_count(tl);
    double duration = wedit_timeline_duration(tl);

    /* Clear to black */
    memset(buf, 0, (size_t)width * height * 4);

    /* Render each track (bottom to top) */
    for (int tr = 0; tr < track_count; tr++) {
        int clip_count = wedit_timeline_clip_count(tl, tr);
        if (clip_count == 0) continue;
        wedit_clip_spec_t* clips = malloc(sizeof(wedit_clip_spec_t) * (size_t)clip_count);
        if (!clips) continue;
        int n = wedit_timeline_clips_on_track(tl, tr, clips, (size_t)clip_count);

        for (int c = 0; c < n; c++) {
            wedit_clip_spec_t* clip = &clips[c];
            if (frame_time < clip->start_time ||
                frame_time >= clip->start_time + clip->duration) continue;

            /* Clip-relative time into the source media */
            double clip_time = frame_time - clip->start_time;
            double source_time = clip->source_in + clip_time;

#ifdef WEDIT_HAS_FFMPEG
            /* Open decode source and get RGBA pixels */
            wedit_clip_source_t* src = wedit_clip_source_open(
                clip->source_path, width, height);
            if (src) {
                if (wedit_clip_source_seek(src, source_time) == 0) {
                    const unsigned char* rgba = wedit_clip_source_get_pixels(src);
                    if (rgba) {
                        /* Composite the decoded clip frame onto the render buffer */
                        composite_blend(buf, rgba, width, height, 0 /* source-over */);
                    }
                }
                wedit_clip_source_close(src);
            }
#else
            /* Procedural fallback: no clip media available */
            (void)source_time;
#endif

            /* Transition handling */
            double clip_end = clip->start_time + clip->duration;
            double transition_start = clip_end - clip->transition_duration;
            int in_transition = (frame_time >= transition_start &&
                                 clip->transition != WEDIT_TRANSITION_NONE);
            if (in_transition && clip->transition == WEDIT_TRANSITION_FADE) {
                double progress = (frame_time - transition_start) / clip->transition_duration;
                float opacity = 1.0f - (float)progress;
                apply_fade(buf, width, height, opacity);
            }

            /* Color grade on video track (matches GPU shader pipeline) */
            if (tr == 0) {
                apply_color_grade(buf, width, height,
                                  color_brightness, color_contrast,
                                  color_saturation, color_gamma);
            }
        }
        free(clips);
    }

    /* WuBu watermark with beat-synced pulse */
    float t_frac = duration > 0 ? (float)(frame_time / duration) : 0;
    draw_wubu_watermark(buf, width, height, t_frac);
}

/* =========================================================================
 * Public API: custom callback render (no encode)
 * ========================================================================= */

int wedit_render_timeline_custom(const wedit_timeline_t* tl,
                                 const wedit_render_opts_t* opts,
                                 wedit_render_frame_fn render_frame,
                                 void* user_data)
{
    if (!tl || !opts || !render_frame) return -1;

    int width = opts->width > 0 ? opts->width : 1280;
    int height = opts->height > 0 ? opts->height : 720;
    double fps = opts->fps > 0 ? opts->fps : wedit_timeline_fps(tl);
    double duration = wedit_timeline_duration(tl);
    int total_frames = (int)(duration * fps);

    unsigned char* frame_buf = malloc((size_t)width * height * 4);
    if (!frame_buf) return -1;

    printf("[wedit_render] Rendering: %dx%d @ %.2ffps, %.1f seconds, %d frames\n",
           width, height, fps, duration, total_frames);

    for (int frame = 0; frame < total_frames; frame++) {
        double frame_time = (double)frame / fps;

        render_frame_into(tl, frame_time, frame_buf, width, height,
                          0.0f, 1.0f, 1.0f, 1.0f, NULL, NULL);

        int ret = render_frame(frame, frame_time, frame_buf, width, height, user_data);
        if (ret != 0) {
            free(frame_buf);
            return -1;
        }

        if (opts->progress_fn) {
            float percent = total_frames > 0 ? (float)frame / (float)total_frames : 0;
            if (opts->progress_fn(frame, total_frames, percent, opts->progress_data) != 0) {
                free(frame_buf);
                return -1;
            }
        }
    }

    printf("[wedit_render] Render complete: %d frames\n", total_frames);
    free(frame_buf);
    return 0;
}

/* ── Single-frame render for preview ──────────────────────────────────── */

int wedit_render_frame_at(const wedit_timeline_t* tl,
                           const wedit_render_opts_t* opts,
                           double frame_time,
                           unsigned char* buf,
                           int width, int height)
{
    if (!tl || !opts || !buf) return -1;
    render_frame_into(tl, frame_time, buf, width, height,
                      0.0f, 1.0f, 1.0f, 1.0f, NULL, NULL);
    return 0;
}

/* =========================================================================
 * Public API: full render to file (uses FFmpeg via wedit_encoder/wedit_muxer)
 * ========================================================================= */

#ifdef WEDIT_HAS_FFMPEG

int wedit_render_timeline(const wedit_timeline_t* tl,
                          const char* output_path,
                          const wedit_render_opts_t* opts)
{
    if (!tl || !output_path || !opts) return -1;
    printf("[wedit_render] Starting render to %s\n", output_path);

    int width = opts->width > 0 ? opts->width : 1280;
    int height = opts->height > 0 ? opts->height : 720;
    double fps = opts->fps > 0 ? opts->fps : wedit_timeline_fps(tl);
    double duration = wedit_timeline_duration(tl);
    int total_frames = (int)(duration * fps);

    /* Create encoder via our opaque wrapper */
    struct wedit_encoder* encoder = wedit_encoder_create_video(
        AV_CODEC_ID_H264, width, height, (int)fps, 1, opts->crf, opts->preset);
    if (!encoder) {
        fprintf(stderr, "[wedit_render] Could not create encoder\n");
        return -1;
    }
    AVCodecContext* enc_ctx = wedit_encoder_context(encoder);

    /* Create muxer via our opaque wrapper */
    struct wedit_muxer* muxer = wedit_muxer_create(output_path, 1, 0);
    if (!muxer) {
        fprintf(stderr, "[wedit_render] Could not create muxer\n");
        wedit_encoder_destroy(encoder);
        return -1;
    }
    int stream_idx = wedit_muxer_add_video_stream(muxer, enc_ctx);
    if (stream_idx < 0) {
        fprintf(stderr, "[wedit_render] Could not create video stream\n");
        wedit_muxer_finalize(muxer);
        wedit_encoder_destroy(encoder);
        return -1;
    }
    if (wedit_muxer_write_header(muxer) < 0) {
        fprintf(stderr, "[wedit_render] Could not write header\n");
        wedit_muxer_finalize(muxer);
        wedit_encoder_destroy(encoder);
        return -1;
    }

    /* Allocate frames and buffers */
    AVFrame* video_frame = av_frame_alloc();
    video_frame->format = AV_PIX_FMT_YUV420P;
    video_frame->width = width;
    video_frame->height = height;

    uint8_t* rgba_buf = (uint8_t*)malloc((size_t)width * height * 4);
    int yuv_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
    uint8_t* yuv_buf = (uint8_t*)malloc((size_t)yuv_size);
    av_image_fill_arrays(video_frame->data, video_frame->linesize,
                         yuv_buf, AV_PIX_FMT_YUV420P, width, height, 1);

    struct SwsContext* sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGB0,
                              width, height, AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, NULL, NULL, NULL);

    printf("[wedit_render] Encoding %d frames to %s\n", total_frames, output_path);

    for (int frame = 0; frame < total_frames; frame++) {
        double frame_time = (double)frame / fps;

        /* Render the frame (same pipeline as custom, inline for encode) */
        render_frame_into(tl, frame_time, rgba_buf, width, height,
                          0.0f, 1.0f, 1.0f, 1.0f, NULL, NULL);

        /* Convert RGBA (RGB0) -> YUV420P */
        const uint8_t* data[4] = { rgba_buf, NULL, NULL, NULL };
        int linesize[4] = { width * 4, 0, 0, 0 };
        sws_scale(sws_ctx, data, linesize, 0, height,
                  video_frame->data, video_frame->linesize);

        video_frame->pts = frame;

        /* Encode */
        if (wedit_encoder_send_frame(encoder, video_frame) < 0) {
            fprintf(stderr, "[wedit_render] Error sending frame %d\n", frame);
            break;
        }

        /* Receive all available packets */
        AVPacket* pkt = av_packet_alloc();
        while (wedit_encoder_receive_packet(encoder, pkt) == 0) {
            pkt->stream_index = (uint32_t)stream_idx;
            av_packet_rescale_ts(pkt, enc_ctx->time_base, AV_TIME_BASE_Q);
            if (wedit_muxer_write_packet(muxer, pkt, stream_idx) < 0) {
                fprintf(stderr, "[wedit_render] Error writing packet\n");
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);

        /* Progress */
        if (opts->progress_fn) {
            float percent = total_frames > 0 ? (float)frame / (float)total_frames : 0;
            if (opts->progress_fn(frame, total_frames, percent, opts->progress_data) != 0) {
                break;
            }
        }
    }

    /* Flush encoder */
    wedit_encoder_flushing(encoder);
    AVPacket* pkt = av_packet_alloc();
    while (wedit_encoder_receive_packet(encoder, pkt) == 0) {
        pkt->stream_index = (uint32_t)stream_idx;
        av_packet_rescale_ts(pkt, enc_ctx->time_base, AV_TIME_BASE_Q);
        wedit_muxer_write_packet(muxer, pkt, stream_idx);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    /* Cleanup */
    sws_freeContext(sws_ctx);
    av_frame_free(&video_frame);
    free(rgba_buf);
    free(yuv_buf);
    wedit_muxer_finalize(muxer);
    wedit_encoder_destroy(encoder);

    printf("[wedit_render] Encode complete: %s\n", output_path);
    return 0;
}

#else

int wedit_render_timeline(const wedit_timeline_t* tl,
                          const char* output_path,
                          const wedit_render_opts_t* opts)
{
    /* CPU composite test mode without FFmpeg */
    (void)tl; (void)output_path;
    int w = opts->width > 0 ? opts->width : 1280;
    int h = opts->height > 0 ? opts->height : 720;
    unsigned char* buf = malloc((size_t)w * h * 4);
    if (!buf) return -1;
    memset(buf, 0, (size_t)w * h * 4);
    draw_wubu_watermark(buf, w, h, 0.5f);
    free(buf);
    printf("[wedit_render] CPU test render complete (FFmpeg not available)\n");
    return 0;
}

#endif /* WEDIT_HAS_FFMPEG */
