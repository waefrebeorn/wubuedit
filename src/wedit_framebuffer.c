/* This file is part of WubuEdit — a SLERM ground-up C11 video editor.
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
 * wedit_framebuffer.c — Virtual framebuffer implementation.
 *
 * CPU path: wraps a raw RGBA buffer, all operations are in-place
 *           on system memory (memcpy + pixel loops).
 *
 * GPU path: creates a wugui_renderer with an offscreen framebuffer.
 *           Blend/fade/grade are done as GPU draw calls; readback
 *           pulls the final RGBA to CPU memory for encoding.
 */

#include "wedit_framebuffer.h"
#include "wugui_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Internal structure */
struct wedit_framebuffer {
    int width;
    int height;
    enum wedit_fb_backend backend;
    unsigned char* cpu_pixels;       /* CPU path: RGBA buffer */
#ifdef WUGUI_HAS_SDL2
    wugui_renderer_t* gpu_renderer;  /* GPU path: renderer context */
    wugui_framebuffer_t* gpu_fb;     /* GPU path: offscreen framebuffer */
#endif
};

/* ---- Helpers (shared) ---- */

static float clamp_f(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---- Public API ---- */

wedit_framebuffer_t* wedit_framebuffer_create(int width, int height,
                                               enum wedit_fb_backend backend)
{
    if (width <= 0 || height <= 0) return NULL;

    wedit_framebuffer_t* fb = calloc(1, sizeof(wedit_framebuffer_t));
    if (!fb) return NULL;

    fb->width = width;
    fb->height = height;
    fb->backend = backend;

    if (backend == WEDIT_FB_CPU || backend == WEDIT_FB_GPU) {
        /* Always allocate CPU pixels for readback */
        fb->cpu_pixels = calloc((size_t)width * height, 4);
        if (!fb->cpu_pixels) {
            free(fb);
            return NULL;
        }
    }

#ifdef WUGUI_HAS_SDL2
    if (backend == WEDIT_FB_GPU) {
        wugui_init_opts_t opts = {
            .width = width,
            .height = height,
            .backend = WUGUI_BACKEND_OPENGL,
            .enable_vsnc = 0,
            .msaaSamples = 0,
            .window_title = "wubuedit-render",
        };
        fb->gpu_renderer = wugui_renderer_create(&opts);
        if (!fb->gpu_renderer) {
            /* Fall back to CPU */
            free(fb->cpu_pixels);
            fb->cpu_pixels = NULL;
            free(fb);
            return NULL;
        }
        fb->gpu_fb = wugui_framebuffer_create(fb->gpu_renderer, width, height);
        if (!fb->gpu_fb) {
            wugui_renderer_destroy(fb->gpu_renderer);
            free(fb->cpu_pixels);
            fb->cpu_pixels = NULL;
            free(fb);
            return NULL;
        }
        /* Bind the offscreen framebuffer for all subsequent draws */
        wugui_renderer_bind_framebuffer(fb->gpu_renderer, fb->gpu_fb);
    }
#else
    (void)backend;
#endif

    return fb;
}

void wedit_framebuffer_destroy(wedit_framebuffer_t* fb)
{
    if (!fb) return;
#ifdef WUGUI_HAS_SDL2
    if (fb->gpu_fb && fb->gpu_renderer) {
        wugui_framebuffer_destroy(fb->gpu_renderer, fb->gpu_fb);
    }
    if (fb->gpu_renderer) {
        wugui_renderer_destroy(fb->gpu_renderer);
    }
#endif
    free(fb->cpu_pixels);
    free(fb);
}

void wedit_framebuffer_clear(wedit_framebuffer_t* fb,
                             unsigned char r, unsigned char g,
                             unsigned char b, unsigned char a)
{
    if (!fb) return;

    if (fb->cpu_pixels) {
        for (int i = 0; i < fb->width * fb->height * 4; i += 4) {
            fb->cpu_pixels[i + 0] = r;
            fb->cpu_pixels[i + 1] = g;
            fb->cpu_pixels[i + 2] = b;
            fb->cpu_pixels[i + 3] = a;
        }
    }
#ifdef WUGUI_HAS_SDL2
    else if (fb->gpu_renderer) {
        wugui_renderer_begin_frame(fb->gpu_renderer,
                                   r / 255.0f, g / 255.0f,
                                   b / 255.0f, a / 255.0f);
    }
#endif
}

void wedit_framebuffer_blend(wedit_framebuffer_t* fb,
                              const unsigned char* src,
                              int src_w, int src_h,
                              int dst_x, int dst_y,
                              int blend_mode)
{
    if (!fb || !src) return;

#ifdef WUGUI_HAS_SDL2
    if (fb->backend == WEDIT_FB_GPU && fb->gpu_renderer) {
        wugui_texture_t* tex = wugui_texture_create(fb->gpu_renderer,
            src_w, src_h, WUGUI_FMT_RGBA8, src);
        if (tex) {
            wugui_renderer_draw_texture(fb->gpu_renderer, tex,
                (float)dst_x, (float)dst_y, 1.0f, 1.0f,
                blend_mode == 0 ? 1.0f : 0.5f);
            wugui_texture_destroy(fb->gpu_renderer, tex);
        }
        return;
    }
#endif

    /* CPU source-over blending */
    int total = fb->width * fb->height;
    (void)src_w; (void)src_h; (void)dst_x; (void)dst_y; (void)blend_mode;
    float sa = 1.0f;  /* full opacity for full-frame composite */
    for (int i = 0; i < total * 4; i += 4) {
        float da = fb->cpu_pixels[i + 3] / 255.0f;
        float out_a = sa + da * (1.0f - sa);
        float ratio = sa / out_a;
        fb->cpu_pixels[i + 0] = (unsigned char)(src[i + 0] * ratio + fb->cpu_pixels[i + 0] * (1.0f - ratio));
        fb->cpu_pixels[i + 1] = (unsigned char)(src[i + 1] * ratio + fb->cpu_pixels[i + 1] * (1.0f - ratio));
        fb->cpu_pixels[i + 2] = (unsigned char)(src[i + 2] * ratio + fb->cpu_pixels[i + 2] * (1.0f - ratio));
        fb->cpu_pixels[i + 3] = (unsigned char)(out_a * 255.0f);
    }
}

void wedit_framebuffer_fade(wedit_framebuffer_t* fb, float opacity)
{
    if (!fb) return;
    opacity = clamp_f(opacity, 0.0f, 1.0f);

    if (fb->cpu_pixels) {
        for (int i = 0; i < fb->width * fb->height * 4; i += 4) {
            fb->cpu_pixels[i + 0] = (unsigned char)(fb->cpu_pixels[i + 0] * opacity);
            fb->cpu_pixels[i + 1] = (unsigned char)(fb->cpu_pixels[i + 1] * opacity);
            fb->cpu_pixels[i + 2] = (unsigned char)(fb->cpu_pixels[i + 2] * opacity);
            fb->cpu_pixels[i + 3] = (unsigned char)(fb->cpu_pixels[i + 3] * opacity);
        }
    }
    /* GPU fade handled in render_frame_into via shader */
}

void wedit_framebuffer_color_grade(wedit_framebuffer_t* fb,
                                    float brightness, float contrast,
                                    float saturation, float gamma)
{
    if (!fb) return;

#ifdef WUGUI_HAS_SDL2
    if (fb->backend == WEDIT_FB_GPU && fb->gpu_renderer) {
        wugui_renderer_set_color_grade(fb->gpu_renderer,
            brightness, contrast, saturation, gamma);
        return;
    }
#endif

    if (fb->cpu_pixels) {
        for (int i = 0; i < fb->width * fb->height * 4; i += 4) {
            float r = fb->cpu_pixels[i + 0] / 255.0f;
            float g = fb->cpu_pixels[i + 1] / 255.0f;
            float b = fb->cpu_pixels[i + 2] / 255.0f;

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

            fb->cpu_pixels[i + 0] = (unsigned char)(clamp_f(r, 0.0f, 1.0f) * 255.0f);
            fb->cpu_pixels[i + 1] = (unsigned char)(clamp_f(g, 0.0f, 1.0f) * 255.0f);
            fb->cpu_pixels[i + 2] = (unsigned char)(clamp_f(b, 0.0f, 1.0f) * 255.0f);
        }
    }
}

void wedit_framebuffer_watermark(wedit_framebuffer_t* fb, float beat_phase)
{
    if (!fb || !fb->cpu_pixels) return;

    /* Simple watermark: "WuBu" text in bottom-right corner with pulse */
    int tw = 100, th = 14;
    int x0 = fb->width - tw - 10;
    int y0 = fb->height - th - 10;
    float pulse = 0.7f + 0.3f * sinf(beat_phase * 2.0f * 3.14159f);

    for (int y = y0; y < y0 + th && y < fb->height; y++) {
        for (int x = x0; x < x0 + tw && x < fb->width; x++) {
            int idx = (y * fb->width + x) * 4;
            fb->cpu_pixels[idx + 0] = (unsigned char)(255 * pulse);
            fb->cpu_pixels[idx + 1] = (unsigned char)(255 * pulse);
            fb->cpu_pixels[idx + 2] = (unsigned char)(255 * pulse);
            fb->cpu_pixels[idx + 3] = (unsigned char)(180 * pulse);
        }
    }
}

void wedit_framebuffer_readback(const wedit_framebuffer_t* fb,
                                 unsigned char* out)
{
    if (!fb || !out) return;

#ifdef WUGUI_HAS_SDL2
    if (fb->backend == WEDIT_FB_GPU && fb->gpu_renderer && fb->gpu_fb) {
        wugui_framebuffer_readback(fb->gpu_renderer, fb->gpu_fb, out);
        return;
    }
#endif

    if (fb->cpu_pixels) {
        memcpy(out, fb->cpu_pixels, (size_t)fb->width * fb->height * 4);
    }
}

void wedit_framebuffer_size(const wedit_framebuffer_t* fb,
                             int* width, int* height)
{
    if (!fb) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = fb->width;
    if (height) *height = fb->height;
}

enum wedit_fb_backend wedit_framebuffer_backend(const wedit_framebuffer_t* fb)
{
    return fb ? fb->backend : WEDIT_FB_CPU;
}
