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
 * wedit_preview.h — Real-time GPU preview engine public API.
 *
 * Bridges the timeline model to the wugui_renderer (SDL2 + OpenGL)
 * for real-time playback preview. Uses the same frame-by-frame
 * render pipeline as wedit_render_timeline, but with a callback
 * that uploads RGBA pixels to a GPU texture for display.
 *
 * WuBu AGI preview: renders frames ahead into a cache for smooth
 * scrubbing, then composites via GPU shaders. No JavaScript,
 * no browser — native C11 + SDL2 + OpenGL.
 */
#ifndef WEDIT_PREVIEW_H
#define WEDIT_PREVIEW_H

#include "wedit_render.h"
#include "wedit_timeline.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque preview context */
typedef struct wedit_preview wedit_preview_t;

/* Create a preview context for the given timeline.
 * The timeline must remain valid for the lifetime of the preview. */
wedit_preview_t* wedit_preview_create(wedit_timeline_t* tl,
                                       const wedit_render_opts_t* opts);

/* Destroy the preview context. */
void wedit_preview_destroy(wedit_preview_t* p);

/* Set up GPU rendering via SDL2 + OpenGL.
 * Returns 0 on success, -1 if GPU is unavailable (falls back to CPU). */
int wedit_preview_setup_gpu(wedit_preview_t* p, int window_w, int window_h,
                             const char* window_title);

/* Playback control */
int wedit_preview_play(wedit_preview_t* p);
int wedit_preview_pause(wedit_preview_t* p);
int wedit_preview_seek(wedit_preview_t* p, double seconds);

/* Render the frame at the given time for preview.
 * Uses GPU compositing when available, CPU otherwise.
 * Returns 0 on success. */
int wedit_preview_render_frame(wedit_preview_t* p, double frame_time);

/* Query playback state */
double wedit_preview_current_time(const wedit_preview_t* p);
int wedit_preview_is_playing(const wedit_preview_t* p);
const char* wedit_preview_error(const wedit_preview_t* p);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_PREVIEW_H */
