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
 * wedit_filter.h — Native filter chain for AGI video editing.
 *
 * While wedit_effects builds FFmpeg filter_complex strings (legacy path),
 * wedit_filter provides a native C11 filter chain that applies effects
 * directly to RGBA pixel buffers. This is the AGI path: intelligent
 * per-pixel processing that can reason about content structure.
 *
 * The filter chain pipeline:
 *   wedit_filter_chain_t  (ordered list of filters)
 *   -> wedit_filter_t     (one filter: params + apply_fn)
 *   -> wedit_filter_apply (per-frame: pixels + width + height -> pixels)
 *
 * Filters can run on CPU (this file) or be dispatched to GPU via
 * wugui_renderer when use_gpu_rendering is set.
 *
 * WuBu philosophy: the GPU renderer shaders (wugui_shaders.h) and the
 * CPU filter chain (wedit_filter.c) share identical mathematical
 * implementations. No divergence in pixel output.
 */
#ifndef WEDIT_FILTER_H
#define WEDIT_FILTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Filter types ─────────────────────────────────────────────────────── */

enum wedit_filter_type {
    WEDIT_FILTER_BLUR,          /* Gaussian blur */
    WEDIT_FILTER_SHARPEN,       /* Unsharp mask */
    WEDIT_FILTER_COLOR_CORRECT, /* brightness/contrast/saturation/gamma */
    WEDIT_FILTER_CHROMA_KEY,    /* green screen / color key */
    WEDIT_FILTER_VIGNETTE,      /* lens vignette */
    WEDIT_FILTER_PIXELATE,      /* Mosaic / blocky */
    WEDIT_FILTER_MIRROR,        /* X/Y mirror */
    WEDIT_FILTER_ROTATE,        /* 90/180/270 degree rotation */
    WEDIT_FILTER_SCALE,         /* resize */
    WEDIT_FILTER_MASK,          /* alpha mask from grayscale */
    WEDIT_FILTER_GLITCH,        /* AGI-style data corruption effect */
    WEDIT_FILTER_STABILIZE,     /* frame stabilization */
    WEDIT_FILTER_COUNT          /* must be last */
};

/* ── Filter parameters ────────────────────────────────────────────────── */

typedef struct {
    float radius;      /* blur radius in pixels */
    float sigma;       /* Gaussian sigma */
} wedit_blur_params;

typedef struct {
    float amount;      /* sharpen strength 0.0-2.0 */
} wedit_sharpen_params;

typedef struct {
    float brightness;  /* -1.0 to 1.0 */
    float contrast;    /* 0.5 to 2.0 */
    float saturation;  /* 0.0 to 3.0 */
    float gamma;       /* 0.5 to 2.5 */
} wedit_color_params;

typedef struct {
    unsigned char key_r, key_g, key_b;  /* color to key out */
    float tolerance;   /* 0.0-1.0 */
    float similarity;  /* 0.0-1.0 */
} wedit_chroma_key_params;

typedef struct {
    float strength;    /* 0.0-1.0 */
    float radius;      /* vignette radius */
} wedit_vignette_params;

typedef struct {
    float block_size;  /* mosaic block size in pixels */
} wedit_pixelate_params;

typedef struct {
    int horizontal;    /* 1 = mirror X, 0 = mirror Y */
} wedit_mirror_params;

typedef struct {
    int angle;         /* 90, 180, 270 */
} wedit_rotate_params;

typedef struct {
    int new_width;
    int new_height;
} wedit_scale_params;

typedef struct {
    float intensity;   /* 0.0-1.0 */
    float speed;       /* 0.1-3.0 */
} wedit_glitch_params;

/* ── A single filter instance ──────────────────────────────────────────── */

typedef void (*wedit_filter_apply_fn)(unsigned char* pixels,
                                       int width, int height,
                                       const void* params);

typedef struct wedit_filter {
    enum wedit_filter_type type;
    char name[64];
    wedit_filter_apply_fn apply;
    /* Parameters — one of the structs above, based on type */
    union {
        wedit_blur_params       blur;
        wedit_sharpen_params    sharpen;
        wedit_color_params      color;
        wedit_chroma_key_params chroma;
        wedit_vignette_params   vignette;
        wedit_pixelate_params   pixelate;
        wedit_mirror_params     mirror;
        wedit_rotate_params     rotate;
        wedit_scale_params      scale;
        wedit_glitch_params     glitch;
    } params;
    /* Timing: filter only active in [start, end) seconds */
    double start_time;   /* when filter activates (seconds) */
    double end_time;     /* when filter deactivates (seconds, 0 = forever) */
    int enabled;         /* 1 = active, 0 = disabled */
} wedit_filter_t;

/* ── Filter chain ──────────────────────────────────────────────────────── */

typedef struct {
    wedit_filter_t* items;
    size_t count;
    size_t capacity;
} wedit_filter_chain_t;

/* ── Public API ────────────────────────────────────────────────────────── */

/* Initialize an empty filter chain. Returns 0 on success. */
int wedit_filter_chain_init(wedit_filter_chain_t* chain, size_t initial_capacity);

/* Add a filter to the chain. Returns 0 on success, -1 on failure. */
int wedit_filter_chain_add(wedit_filter_chain_t* chain, const wedit_filter_t* filter);

/* Apply all active filters in the chain to an RGBA pixel buffer.
 * frame_time is in seconds (used for timing).
 * Returns 0 on success. */
int wedit_filter_chain_apply(wedit_filter_chain_t* chain,
                              unsigned char* pixels,
                              int width, int height,
                              double frame_time);

/* Free all memory in a filter chain. */
void wedit_filter_chain_free(wedit_filter_chain_t* chain);

/* ── Factory functions for common filters ────────────────────────────── */

/* Create a brightness/contrast/saturation/gamma color correction filter. */
wedit_filter_t wedit_filter_color(float brightness, float contrast,
                                   float saturation, float gamma,
                                   double start_time, double end_time);

/* Create a Gaussian blur filter. */
wedit_filter_t wedit_filter_blur(float radius, float sigma,
                                  double start_time, double end_time);

/* Create a sharpen filter (unsharp mask). */
wedit_filter_t wedit_filter_sharpen(float amount,
                                     double start_time, double end_time);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_FILTER_H */
