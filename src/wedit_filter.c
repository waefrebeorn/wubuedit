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
 * wedit_filter.c — Native filter chain implementation.
 *
 * Each filter applies its pixel transformation directly to RGBA buffers.
 * The math is identical to the GLSL shaders in wugui_shaders.h —
 * this is the CPU fallback / AGI reasoning layer.
 *
 * SLERM note: Diffusion Studio applies effects via FFmpeg filter_complex
 * strings piped to a subprocess. Our filter chain processes pixels
 * in-process in C11. This allows per-frame adaptive filtering
 * (the AGI path).
 */

#include "wedit_filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static inline float clampf_f(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── Filter chain management ──────────────────────────────────────────── */

int wedit_filter_chain_init(wedit_filter_chain_t* chain, size_t initial_capacity)
{
    if (!chain) return -1;
    memset(chain, 0, sizeof(wedit_filter_chain_t));
    if (initial_capacity > 0) {
        chain->items = malloc(sizeof(wedit_filter_t) * initial_capacity);
        if (!chain->items) return -1;
        chain->capacity = initial_capacity;
    }
    return 0;
}

int wedit_filter_chain_add(wedit_filter_chain_t* chain, const wedit_filter_t* filter)
{
    if (!chain || !filter) return -1;
    if (chain->count >= chain->capacity) {
        size_t new_cap = chain->capacity == 0 ? 4 : chain->capacity * 2;
        wedit_filter_t* new_items = malloc(sizeof(wedit_filter_t) * new_cap);
        if (!new_items) return -1;
        if (chain->items) {
            memcpy(new_items, chain->items, sizeof(wedit_filter_t) * chain->count);
            free(chain->items);
        }
        chain->items = new_items;
        chain->capacity = new_cap;
    }
    chain->items[chain->count++] = *filter;
    return 0;
}

int wedit_filter_chain_apply(wedit_filter_chain_t* chain,
                              unsigned char* pixels,
                              int width, int height,
                              double frame_time)
{
    if (!chain || !pixels) return -1;
    for (size_t i = 0; i < chain->count; i++) {
        wedit_filter_t* f = &chain->items[i];
        if (!f->enabled) continue;
        /* Check timing */
        if (frame_time < f->start_time) continue;
        if (f->end_time > 0.0 && frame_time >= f->end_time) continue;
        if (f->apply) {
            f->apply(pixels, width, height, &f->params);
        }
    }
    return 0;
}

void wedit_filter_chain_free(wedit_filter_chain_t* chain)
{
    if (!chain) return;
    free(chain->items);
    memset(chain, 0, sizeof(wedit_filter_chain_t));
}

/* ── Filter implementations ───────────────────────────────────────────── */

static void filter_color_apply(unsigned char* pixels, int width, int height,
                               const void* params_ptr)
{
    const wedit_color_params* p = (const wedit_color_params*)params_ptr;

    for (int i = 0; i < width * height * 4; i += 4) {
        float r = pixels[i + 0] / 255.0f;
        float g = pixels[i + 1] / 255.0f;
        float b = pixels[i + 2] / 255.0f;

        /* Brightness */
        r += p->brightness; g += p->brightness; b += p->brightness;

        /* Contrast */
        r = (r - 0.5f) * p->contrast + 0.5f;
        g = (g - 0.5f) * p->contrast + 0.5f;
        b = (b - 0.5f) * p->contrast + 0.5f;

        /* Saturation */
        float luma = 0.299f * r + 0.587f * g + 0.114f * b;
        r = luma + (r - luma) * p->saturation;
        g = luma + (g - luma) * p->saturation;
        b = luma + (b - luma) * p->saturation;

        /* Gamma */
        if (p->gamma > 0.001f) {
            r = powf(fmaxf(r, 0.0f), 1.0f / p->gamma);
            g = powf(fmaxf(g, 0.0f), 1.0f / p->gamma);
            b = powf(fmaxf(b, 0.0f), 1.0f / p->gamma);
        }

        pixels[i + 0] = (unsigned char)(clampf_f(r, 0.0f, 1.0f) * 255.0f);
        pixels[i + 1] = (unsigned char)(clampf_f(g, 0.0f, 1.0f) * 255.0f);
        pixels[i + 2] = (unsigned char)(clampf_f(b, 0.0f, 1.0f) * 255.0f);
    }
}

/* Two-pass separable Gaussian blur — matches shader blur_fragment */
static void filter_blur_apply(unsigned char* pixels, int width, int height,
                              const void* params_ptr)
{
    const wedit_blur_params* p = (const wedit_blur_params*)params_ptr;
    int radius = (int)ceilf(p->radius);
    if (radius < 1) radius = 1;
    if (radius > 64) radius = 64;

    /* Allocate temp buffer */
    unsigned char* tmp = malloc((size_t)width * height * 4);
    if (!tmp) return;

    /* Horizontal pass */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f, wsum = 0.0f;
            for (int dx = -radius; dx <= radius; dx++) {
                int sx = clampf_f((float)(x + dx), 0, width - 1);
                int idx = (y * width + sx) * 4;
                float w = expf(-(float)(dx * dx) / (2.0f * p->sigma * p->sigma));
                r += pixels[idx + 0] * w;
                g += pixels[idx + 1] * w;
                b += pixels[idx + 2] * w;
                a += pixels[idx + 3] * w;
                wsum += w;
            }
            int idx = (y * width + x) * 4;
            tmp[idx + 0] = (unsigned char)(r / wsum);
            tmp[idx + 1] = (unsigned char)(g / wsum);
            tmp[idx + 2] = (unsigned char)(b / wsum);
            tmp[idx + 3] = (unsigned char)(a / wsum);
        }
    }

    /* Vertical pass */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f, wsum = 0.0f;
            for (int dy = -radius; dy <= radius; dy++) {
                int sy = (int)clampf_f((float)(y + dy), 0, height - 1);
                int idx = (sy * width + x) * 4;
                float w = expf(-(float)(dy * dy) / (2.0f * p->sigma * p->sigma));
                r += tmp[idx + 0] * w;
                g += tmp[idx + 1] * w;
                b += tmp[idx + 2] * w;
                a += tmp[idx + 3] * w;
                wsum += w;
            }
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = (unsigned char)(r / wsum);
            pixels[idx + 1] = (unsigned char)(g / wsum);
            pixels[idx + 2] = (unsigned char)(b / wsum);
            pixels[idx + 3] = (unsigned char)(a / wsum);
        }
    }
    free(tmp);
}

/* Unsharp mask sharpen */
static void filter_sharpen_apply(unsigned char* pixels, int width, int height,
                                 const void* params_ptr)
{
    const wedit_sharpen_params* p = (const wedit_sharpen_params*)params_ptr;
    unsigned char* blurred = malloc((size_t)width * height * 4);
    if (!blurred) return;
    memcpy(blurred, pixels, (size_t)width * height * 4);

    /* Blur with radius 1 for the kernel */
    wedit_blur_params bp = {1.0f, 1.0f};
    filter_blur_apply(blurred, width, height, &bp);

    /* Unsharp: original + (original - blurred) * amount */
    for (int i = 0; i < width * height * 4; i += 4) {
        for (int c = 0; c < 3; c++) {
            float orig = (float)pixels[i + c];
            float blur = (float)blurred[i + c];
            float val = orig + (orig - blur) * p->amount;
            pixels[i + c] = clampf_f(val, 0.0f, 255.0f);
        }
    }
    free(blurred);
}

/* ── Factory functions ────────────────────────────────────────────────── */

wedit_filter_t wedit_filter_color(float brightness, float contrast,
                                   float saturation, float gamma,
                                   double start_time, double end_time)
{
    wedit_filter_t f;
    memset(&f, 0, sizeof(f));
    f.type = WEDIT_FILTER_COLOR_CORRECT;
    strncpy(f.name, "color_correct", sizeof(f.name) - 1);
    f.apply = filter_color_apply;
    f.params.color.brightness = brightness;
    f.params.color.contrast = contrast;
    f.params.color.saturation = saturation;
    f.params.color.gamma = gamma;
    f.start_time = start_time;
    f.end_time = end_time;
    f.enabled = 1;
    return f;
}

wedit_filter_t wedit_filter_blur(float radius, float sigma,
                                  double start_time, double end_time)
{
    wedit_filter_t f;
    memset(&f, 0, sizeof(f));
    f.type = WEDIT_FILTER_BLUR;
    strncpy(f.name, "gaussian_blur", sizeof(f.name) - 1);
    f.apply = filter_blur_apply;
    f.params.blur.radius = radius;
    f.params.blur.sigma = sigma;
    f.start_time = start_time;
    f.end_time = end_time;
    f.enabled = 1;
    return f;
}

wedit_filter_t wedit_filter_sharpen(float amount,
                                     double start_time, double end_time)
{
    wedit_filter_t f;
    memset(&f, 0, sizeof(f));
    f.type = WEDIT_FILTER_SHARPEN;
    strncpy(f.name, "sharpen", sizeof(f.name) - 1);
    f.apply = filter_sharpen_apply;
    f.params.sharpen.amount = amount;
    f.start_time = start_time;
    f.end_time = end_time;
    f.enabled = 1;
    return f;
}
