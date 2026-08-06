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
 * wedit_framebuffer.h — Virtual framebuffer abstraction for WuBuEdit.
 *
 * A virtualized compositing surface that abstracts the backing store:
 *   - CPU: raw RGBA buffer in system memory (default, always available)
 *   - GPU: OpenGL framebuffer via wugui_renderer (when SDL2/GLEW available)
 *
 * All compositing operations (blend, fade, color grade, watermark) operate
 * on this abstract surface, not directly on raw pixel buffers. This is the
 * single rendering target that the render pipeline writes to per frame.
 *
 * The GPU path uses an offscreen framebuffer for headless rendering.
 * After composite_blend() draws all clips, the framebuffer is read back
 * to CPU RGBA for encoding via avcodec_send_frame.
 */

#ifndef WEDIT_FRAMEBUFFER_H
#define WEDIT_FRAMEBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Virtual framebuffer backend type */
enum wedit_fb_backend {
    WEDIT_FB_CPU = 0,    /* Software RGBA buffer in system memory */
    WEDIT_FB_GPU,        /* GPU framebuffer via wugui_renderer (offscreen) */
};

/* Opaque virtual framebuffer handle */
typedef struct wedit_framebuffer wedit_framebuffer_t;

/*
 * Create a virtual framebuffer.
 *   width, height: dimensions in pixels
 *   backend: WEDIT_FB_CPU or WEDIT_FB_GPU
 *
 * For WEDIT_FB_GPU, returns NULL if SDL2/OGL context cannot be created
 * (e.g., headless without EGL). Caller should fall back to WEDIT_FB_CPU.
 */
wedit_framebuffer_t* wedit_framebuffer_create(int width, int height,
                                               enum wedit_fb_backend backend);

/* Destroy the framebuffer and free all resources. */
void wedit_framebuffer_destroy(wedit_framebuffer_t* fb);

/* Clear the framebuffer to the given RGBA color (0-255 per channel). */
void wedit_framebuffer_clear(wedit_framebuffer_t* fb,
                             unsigned char r, unsigned char g,
                             unsigned char b, unsigned char a);

/*
 * Composite source RGBA pixels onto the framebuffer using source-over blend.
 *   src: RGBA pixel data (src_w * src_h * 4 bytes)
 *   src_w, src_h: dimensions of the source
 *   dst_x, dst_y: placement position in the framebuffer (top-left origin)
 *   blend_mode: 0=source-over, 1=multiply, 2=screen, 3=overlay, 4=difference
 */
void wedit_framebuffer_blend(wedit_framebuffer_t* fb,
                              const unsigned char* src,
                              int src_w, int src_h,
                              int dst_x, int dst_y,
                              int blend_mode);

/*
 * Apply a fade transition (multiply all pixels by opacity).
 * opacity: 0.0 = fully transparent, 1.0 = fully opaque
 */
void wedit_framebuffer_fade(wedit_framebuffer_t* fb, float opacity);

/*
 * Apply color grading: brightness -> contrast -> saturation -> gamma.
 *   brightness: -1.0 to 1.0
 *   contrast: 0.0 = flat, 1.0 = no change, >1.0 increases contrast
 *   saturation: 0.0 = grayscale, 1.0 = no change
 *   gamma: 1.0 = no change, e.g., 2.2 = apply gamma, 0.45 = remove gamma
 */
void wedit_framebuffer_color_grade(wedit_framebuffer_t* fb,
                                    float brightness, float contrast,
                                    float saturation, float gamma);

/*
 * Draw the WuBu watermark with beat-synced pulse.
 *   beat_phase: 0.0 to 1.0, drives the pulse animation
 */
void wedit_framebuffer_watermark(wedit_framebuffer_t* fb, float beat_phase);

/*
 * Read back the framebuffer to CPU RGBA memory.
 *   out: must have at least width*height*4 bytes allocated.
 * This is how the GPU path delivers pixels to the encoder.
 */
void wedit_framebuffer_readback(const wedit_framebuffer_t* fb,
                                 unsigned char* out);

/* Get the framebuffer dimensions. */
void wedit_framebuffer_size(const wedit_framebuffer_t* fb,
                             int* width, int* height);

/* Get the backend type. */
enum wedit_fb_backend wedit_framebuffer_backend(const wedit_framebuffer_t* fb);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_FRAMEBUFFER_H */
