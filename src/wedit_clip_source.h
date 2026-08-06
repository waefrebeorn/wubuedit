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
 * wedit_clip_source.h — Decode a media file to RGBA frames at arbitrary timestamps.
 *
 * This is the bridge between the demux/decode layer (wedit_demux, wedit_decode)
 * and the render pipeline (wedit_render). It caches the last decoded frame
 * and its timestamp so the render loop doesn't re-decode the same frame
 * when scrubbing or re-rendering at the same timestamp.
 *
 * Usage:
 *   wedit_clip_source_t* src = wedit_clip_source_open("/path/to/clip.mp4", 1920, 1080);
 *   if (src) {
 *       // Seek to 5.0s into the clip's source, decode, get RGBA pixels
 *       if (wedit_clip_source_seek(src, 5.0) == 0) {
 *           const unsigned char* rgba = wedit_clip_source_get_pixels(src);
 *           int w = wedit_clip_source_width(src);
 *           int h = wedit_clip_source_height(src);
 *           // ... composite rgba onto render buffer ...
 *       }
 *   }
 *   wedit_clip_source_close(src);
 */

#ifndef WEDIT_CLIP_SOURCE_H
#define WEDIT_CLIP_SOURCE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle — internals in wedit_internal.h */
typedef struct wedit_clip_source wedit_clip_source_t;

/*
 * Open a media file for RGBA frame decoding.
 *
 * src_path:   path to the media file (any format libavformat supports).
 * out_width:  desired width of decoded RGBA frames (source is scaled to fit).
 * out_height: desired height of decoded RGBA frames.
 *
 * Returns an opaque handle, or NULL on failure (file not found, no video
 * stream, codec not found, etc.).
 */
wedit_clip_source_t* wedit_clip_source_open(const char* src_path,
                                             int out_width, int out_height);

/*
 * Seek the clip to the given timestamp (in seconds, relative to the start
 * of the source file) and decode the nearest frame.
 *
 * On success, the decoded RGBA pixels are available via wedit_clip_source_get_pixels().
 * Returns 0 on success, -1 on failure (seek/decode error).
 */
int wedit_clip_source_seek(wedit_clip_source_t* src, double timestamp);

/*
 * Get the most recently decoded RGBA pixel buffer.
 * The buffer is owned by the source and remains valid until the next seek.
 * Returns NULL if no frame has been decoded yet.
 */
const unsigned char* wedit_clip_source_get_pixels(wedit_clip_source_t* src);

/* Returns the width of the decoded RGBA frames. */
int wedit_clip_source_width(const wedit_clip_source_t* src);

/* Returns the height of the decoded RGBA frames. */
int wedit_clip_source_height(const wedit_clip_source_t* src);

/* Returns the timestamp (seconds) of the most recently decoded frame,
 * or -1.0 if no frame has been decoded. */
double wedit_clip_source_timestamp(const wedit_clip_source_t* src);

/*
 * Close the source and free all associated memory.
 * Safe to call with NULL.
 */
void wedit_clip_source_close(wedit_clip_source_t* src);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_CLIP_SOURCE_H */
