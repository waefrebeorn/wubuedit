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
 * wugui_renderer.h — GPU-accelerated video rendering for WuBuEdit.
 *
 * Rendering pipeline adapted from Diffusion Studio's WebGL2 + GLSL shaders,
 * reimplemented in native C11 with SDL2 + OpenGL (or Vulkan via SDLGPU).
 *
 * Key architecture (from Diffusion Studio reverse-engineering):
 * - Video frames are decoded via FFmpeg (libavcodec) into GPU textures
 * - YUV→RGB conversion happens in a fragment shader (zero CPU round-trip)
 * - Compositing of multiple layers happens in GPU framebuffer objects
 * - Effects (color grading, LUT3D) happen via compute shaders or
 *   fragment shaders on the same GPU texture path
 *
 * GLSL shaders ported from Diffusion Studio's decompiled shader code.
 * The YUV decoder shader is the most performance-critical: it reads a
 * single packed RGBA texture and produces RGB video output entirely on GPU.
 */
#ifndef WUGUI_RENDERER_H
#define WUGUI_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* Rendering backend selection */
enum wugui_backend {
    WUGUI_BACKEND_OPENGL = 0,   /* SDL2 + OpenGL 3.3+ (default, widest support) */
    WUGUI_BACKEND_VULKAN,       /* Vulkan via SDLGPU or Volk (RTX 4050 native) */
    WUGUI_BACKEND_METAL,       /* macOS Metal (future) */
};

/* GPU texture handle (opaque) */
typedef struct wugui_texture wugui_texture_t;

/* GPU framebuffer for compositing */
typedef struct wugui_framebuffer wugui_framebuffer_t;

/* Renderer context (opaque) */
typedef struct wugui_renderer wugui_renderer_t;

/* Pixel format for textures */
enum wugui_pixel_format {
    WUGUI_FMT_RGBA8,     /* 8-bit RGBA (for RGB frames) */
    WUGUI_FMT_YUV420,    /* Planar YUV 4:2:0 (for hardware video decode) */
    WUGUI_FMT_NV12,      /* Semi-planar YUV (NVIDIA decode output) */
    WUGUI_FMT_R8,        /* Single channel 8-bit (for alpha/masks) */
};

/* Renderer initialization options */
typedef struct {
    int           width;
    int           height;
    enum wugui_backend backend;
    int           enable_vsnc;   /* V-sync (0=off, 1=on) */
    int           msaaSamples;   /* MSAA anti-aliasing samples */
    const char*   window_title;  /* Window title string */
} wugui_init_opts_t;

/* Initialize the GPU renderer.
 * Creates an OpenGL/Vulkan context via SDL2.
 * Returns NULL on failure (e.g., no GPU drivers).
 */
wugui_renderer_t* wugui_renderer_create(const wugui_init_opts_t* opts);

/* Destroy the renderer and free all GPU resources. */
void wugui_renderer_destroy(wugui_renderer_t* r);

/* Begin a new render frame. Clear to the given RGBA color. */
void wugui_renderer_begin_frame(wugui_renderer_t* r,
                                 float clear_r, float clear_g, float clear_b, float clear_a);

/* End the current render frame and swap buffers. */
void wugui_renderer_end_frame(wugui_renderer_t* r);

/* Create a texture from raw pixel data.
 * format determines the internal texture format.
 * data should be in the appropriate pixel format for `format`.
 * Returns an opaque texture handle.
 */
wugui_texture_t* wugui_texture_create(wugui_renderer_t* r,
                                       int width, int height,
                                       enum wugui_pixel_format format,
                                       const void* data);

/* Upload new pixel data to an existing texture (for video frame updates). */
void wugui_texture_update(wugui_renderer_t* r,
                           wugui_texture_t* tex,
                           const void* data);

/* Free a texture. */
void wugui_texture_destroy(wugui_renderer_t* r, wugui_texture_t* tex);

/* Draw a textured quad (a video frame) at the given position/size.
 * x, y are top-left corner in pixels (0,0 = top-left of canvas).
 * scale_x/scale_y are multipliers applied to width/height.
 */
void wugui_renderer_draw_texture(wugui_renderer_t* r,
                                  wugui_texture_t* tex,
                                  float x, float y,
                                  float scale_x, float scale_y,
                                  float alpha);

/* Apply a color grading effect (brightness/contrast/saturation/gamma).
 * This uses the WuBu color grading shader (ported from Diffusion Studio's
 * compositing pipeline, adapted for native C11).
 */
void wugui_renderer_set_color_grade(wugui_renderer_t* r,
                                     float brightness,
                                     float contrast,
                                     float saturation,
                                     float gamma);

/* Create a framebuffer for offscreen rendering (for effects compositing). */
wugui_framebuffer_t* wugui_framebuffer_create(wugui_renderer_t* r,
                                                int width, int height);

/* Bind a framebuffer for rendering (NULL = default framebuffer). */
void wugui_renderer_bind_framebuffer(wugui_renderer_t* r,
                                      wugui_framebuffer_t* fb);

/* Read back a framebuffer to CPU memory (for export).
 * format must match WUGUI_FMT_RGBA8.
 * data must have width*height*4 bytes allocated.
 */
void wugui_framebuffer_readback(wugui_renderer_t* r,
                                 wugui_framebuffer_t* fb,
                                 uint8_t* data);

/* Free a framebuffer. */
void wugui_framebuffer_destroy(wugui_renderer_t* r, wugui_framebuffer_t* fb);

/* Get the renderer's viewport dimensions. */
void wugui_renderer_get_size(const wugui_renderer_t* r, int* w, int* h);

#ifdef __cplusplus
}
#endif

#endif /* WUGUI_RENDERER_H */
