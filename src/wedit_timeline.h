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
 * wubuedit — SLERM ground-up reimplementation of a video editor.
 * Native C11 core with FFmpeg/libav linked as system libraries.
 *
 * wedit_timeline.h — Timeline model public API.
 * The timeline holds tracks, each with clips placed at time positions.
 * This is the heart of the editor: pure C11, zero FFmpeg dependencies.
 */
#ifndef WEDIT_TIMELINE_H
#define WEDIT_TIMELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ── Types ──────────────────────────────────────────────────────────────── */

/* Transition type between clips */
enum wedit_transition_type {
    WEDIT_TRANSITION_NONE = 0,
    WEDIT_TRANSITION_FADE,
    WEDIT_TRANSITION_DISSOLVE,
    WEDIT_TRANSITION_PIXELISE,
};

/* A clip placed on a track at a specific time position */
typedef struct {
    int    track_index;              /* which track this clip belongs to (0-based) */
    double start_time;               /* seconds from timeline start (when clip plays) */
    double duration;                 /* seconds of this clip on timeline */
    char   source_path[512];         /* path to source media file (NUL-terminated) */
    double source_in;                /* seconds into source file (in-point, 0 = start) */
    double source_out;               /* seconds into source file (out-point, 0 = end of clip) */
    enum wedit_transition_type transition;      /* transition type to next clip */
    double transition_duration;               /* crossfade duration in seconds */
} wedit_clip_spec_t;

/* Opaque timeline handle — struct lives in wedit_timeline.c */
typedef struct wedit_timeline wedit_timeline_t;

/* Opaque clip handle */
typedef struct wedit_clip wedit_clip_t;

/* ── Timeline lifecycle ──────────────────────────────────────────────────── */

/*
 * Create a new empty timeline with the given number of tracks.
 * fps is the timeline's working frame rate (e.g. 30.0, 23.976).
 * Returns NULL on allocation failure.
 */
wedit_timeline_t* wedit_timeline_create(int track_count, double fps);

/* Destroy a timeline and free all clips. Safe to call with NULL. */
void wedit_timeline_destroy(wedit_timeline_t* tl);

/* ── Clip management ────────────────────────────────────────────────────── */

/*
 * Add a clip to the timeline at the given spec.
 * Returns a clip_id (>=0) on success, -1 on failure.
 * Clips on the same track must not overlap (caller's responsibility).
 */
int wedit_timeline_add_clip(wedit_timeline_t* tl, const wedit_clip_spec_t* spec);

/*
 * Remove a clip by its clip_id. Returns 0 on success, -1 if not found.
 */
int wedit_timeline_remove_clip(wedit_timeline_t* tl, int clip_id);

/*
 * Move a clip to a new start_time. Returns 0 on success, -1 if not found.
 */
int wedit_timeline_move_clip(wedit_timeline_t* tl, int clip_id, double new_start);

/*
 * Get clip count for a given track.
 */
int wedit_timeline_clip_count(const wedit_timeline_t* tl, int track_index);

/*
 * Get total timeline duration (longest track end point).
 */
double wedit_timeline_duration(const wedit_timeline_t* tl);

/* ── Project serialization ──────────────────────────────────────────────── */

/*
 * Build an FFmpeg-compatible filter_complex string for rendering this timeline.
 * The caller must free() the returned string.
 * video_track is the index of the main video track; audio_track is the audio track.
 * output_path is the destination file.
 */
char* wedit_timeline_build_render_command(const wedit_timeline_t* tl,
                                          int video_track,
                                          int audio_track,
                                          const char* output_path,
                                          int width,
                                          int height);

/* ── Cleveland mode helpers ────────────────────────────────────────────── */

/*
 * Get the clip_id of the clip at a given time on a track.
 * Returns -1 if no clip exists at that time.
 */
int wedit_timeline_clip_at_time(const wedit_timeline_t* tl,
                                int track_index,
                                double time_seconds);

/*
 * Return all clip specs on a track, sorted by start_time.
 * Caller-allocated buffer of size count. count must be >= wedit_timeline_clip_count().
 * Returns the number of clips written.
 */
int wedit_timeline_clips_on_track(const wedit_timeline_t* tl,
                                  int track_index,
                                  wedit_clip_spec_t* out_specs,
                                  size_t max_count);

/* ── Metadata ───────────────────────────────────────────────────────────── */

int   wedit_timeline_track_count(const wedit_timeline_t* tl);
double wedit_timeline_fps(const wedit_timeline_t* tl);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_TIMELINE_H */
