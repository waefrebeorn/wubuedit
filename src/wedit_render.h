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
 * wedit_render.h — Render pipeline public API.
 *
 * The render pipeline ties together: timeline model → media decode →
 * compositing → encode. This is the heart of WubuEdit: pure C11 with
 * FFmpeg/libav linked as system plumbing (codec/muxing only).
 *
 * The pipeline walks the timeline frame-by-frame at the specified FPS,
 * decodes source clips, composites them with blend modes and transitions,
 * applies color grading, and writes the result to an output file.
 *
 * This is NOT a filter_complex string builder (that's the legacy path).
 * This is the REAL native render pipeline — frame-by-frame decode,
 * GPU-accelerated compositing, and encode. The GPU renderer (wugui_renderer)
 * handles the actual pixel compositing on the GPU; this file orchestrates.
 */
#ifndef WEDIT_RENDER_H
#define WEDIT_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wedit_timeline.h"

/* Opaque render context handle */
typedef struct wedit_render_ctx wedit_render_ctx_t;

/* Progress callback: called once per frame rendered.
 * frame_num: current frame (0-based)
 * total_frames: total frames in output
 * percent: 0.0 to 1.0 completion
 * user_data: caller-provided pointer
 * Return 0 to continue, non-zero to abort. */
typedef int (*wedit_render_progress_fn)(int frame_num, int total_frames,
                                          float percent, void* user_data);

/* Render options */
typedef struct {
    int    width;          /* output width (0 = auto from timeline) */
    int    height;         /* output height (0 = auto from timeline) */
    double fps;            /* output frame rate (0 = use timeline fps) */
    int    crf;            /* rate control: 0=lossless, 23=quality (FFmpeg default) */
    const char* preset;    /* FFmpeg encoding preset: "ultrafast", "fast", "medium", "slow" */
    const char* pixel_format; /* "yuv420p" or "yuv444p" or "rgb24" */
    int    threads;        /* encode threads (0 = auto) */
    int    gpu_encode;     /* 1 = use GPU encoding (h264_nvenc), 0 = CPU (libx264) */

    /* Audio */
    int    audio_sample_rate; /* 0 = use source audio sr */
    int    audio_channels;    /* 0 = use source audio channels */

    /* GPU rendering */
    int    use_gpu_rendering; /* 1 = route through wugui_renderer, 0 = CPU composite */

    /* Progress */
    wedit_render_progress_fn progress_fn;
    void* progress_data;
} wedit_render_opts_t;

/* Set default render options */
void wedit_render_default_opts(wedit_render_opts_t* opts);

/*
 * Render a complete timeline to a video file.
 *
 * This function:
 * 1. Iterates frame-by-frame over the timeline duration
 * 2. For each frame: decodes active clips, composites them with blend modes,
 *    applies GPU color grading, and encodes to the output file
 * 3. Handles transitions (fade, dissolve, pixelise)
 * 4. Writes the result via FFmpeg/libav (libavformat + libavcodec)
 *
 * Returns 0 on success, -1 on failure.
 */
int wedit_render_timeline(const wedit_timeline_t* tl,
                          const char* output_path,
                          const wedit_render_opts_t* opts);

/*
 * Render a timeline with a custom render function.
 * Allows the caller to intercept each frame for previewing/saving.
 *
 * render_frame: called with (frame_num, frame_time, pixels, width, height, user_data)
 *   pixels is RGBA data (width * height * 4 bytes)
 *   Return 0 to continue, non-zero to abort.
 */
typedef int (*wedit_render_frame_fn)(int frame_num, double frame_time,
                                     const unsigned char* pixels,
                                     int width, int height,
                                     void* user_data);

int wedit_render_timeline_custom(const wedit_timeline_t* tl,
                                 const wedit_render_opts_t* opts,
                                 wedit_render_frame_fn render_frame,
                                 void* user_data);

/*
 * Render a single frame at the given time into an RGBA buffer.
 * This is the preview path — fast, no encode, no file output.
 * buf must have at least width*height*4 bytes allocated.
 * Returns 0 on success, -1 on failure.
 */
int wedit_render_frame_at(const wedit_timeline_t* tl,
                           const wedit_render_opts_t* opts,
                           double frame_time,
                           unsigned char* buf,
                           int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_RENDER_H */
