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
 * wedit_timeline.c — Timeline model implementation.
 *
 * The timeline is a list of tracks, each containing a list of clips.
 * Clips are placed at time positions with source in/out points.
 * This is the heart of the editor: pure C11, zero FFmpeg dependencies.
 */
#include "wedit_timeline.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal clip representation ───────────────────────────────────────── */

struct wedit_clip {
    int    id;           /* unique clip_id, assigned sequentially */
    int    track_index;
    double start_time;
    double duration;
    char   source_path[512];
    double source_in;
    double source_out;
    enum wedit_transition_type transition;
    double transition_duration;
    struct wedit_clip* next;  /* singly-linked list per track */
};

/* ── Internal timeline representation ──────────────────────────────────── */

struct wedit_timeline {
    int     track_count;
    double  fps;
    int     next_clip_id;     /* increments for each new clip */
    struct wedit_clip** tracks; /* array of track_count head pointers */
};

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

wedit_timeline_t* wedit_timeline_create(int track_count, double fps)
{
    if (track_count < 1 || fps <= 0.0) {
        return NULL;
    }

    wedit_timeline_t* tl = calloc(1, sizeof(wedit_timeline_t));
    if (!tl) {
        return NULL;
    }

    tl->track_count = track_count;
    tl->fps = fps;
    tl->next_clip_id = 1;  /* start at 1; 0 is invalid */

    tl->tracks = calloc((size_t)track_count, sizeof(struct wedit_clip*));
    if (!tl->tracks) {
        free(tl);
        return NULL;
    }

    return tl;
}

void wedit_timeline_destroy(wedit_timeline_t* tl)
{
    if (!tl) {
        return;
    }

    if (tl->tracks) {
        for (int i = 0; i < tl->track_count; i++) {
            struct wedit_clip* clip = tl->tracks[i];
            while (clip) {
                struct wedit_clip* next = clip->next;
                free(clip);
                clip = next;
            }
        }
        free(tl->tracks);
    }

    free(tl);
}

/* ── Clip management ───────────────────────────────────────────────────── */

int wedit_timeline_add_clip(wedit_timeline_t* tl, const wedit_clip_spec_t* spec)
{
    if (!tl || !spec) {
        return -1;
    }
    if (spec->track_index < 0 || spec->track_index >= tl->track_count) {
        return -1;
    }
    if (spec->duration <= 0.0) {
        return -1;
    }
    if (spec->source_path[0] == '\0') {
        return -1;
    }

    struct wedit_clip* clip = calloc(1, sizeof(struct wedit_clip));
    if (!clip) {
        return -1;
    }

    clip->id = tl->next_clip_id++;
    clip->track_index = spec->track_index;
    clip->start_time = spec->start_time;
    clip->duration = spec->duration;
    strncpy(clip->source_path, spec->source_path, sizeof(clip->source_path) - 1);
    clip->source_path[sizeof(clip->source_path) - 1] = '\0';
    clip->source_in = spec->source_in;
    clip->source_out = spec->source_out;
    clip->transition = spec->transition;
    clip->transition_duration = spec->transition_duration;

    /* Insert at head of track's linked list */
    clip->next = tl->tracks[spec->track_index];
    tl->tracks[spec->track_index] = clip;

    return clip->id;
}

int wedit_timeline_remove_clip(wedit_timeline_t* tl, int clip_id)
{
    if (!tl || clip_id < 1) {
        return -1;
    }

    for (int i = 0; i < tl->track_count; i++) {
        struct wedit_clip** cur = &tl->tracks[i];
        while (*cur) {
            if ((*cur)->id == clip_id) {
                struct wedit_clip* to_free = *cur;
                *cur = (*cur)->next;
                free(to_free);
                return 0;
            }
            cur = &(*cur)->next;
        }
    }

    return -1;  /* not found */
}

int wedit_timeline_move_clip(wedit_timeline_t* tl, int clip_id, double new_start)
{
    if (!tl || clip_id < 1) {
        return -1;
    }

    for (int i = 0; i < tl->track_count; i++) {
        struct wedit_clip* clip = tl->tracks[i];
        while (clip) {
            if (clip->id == clip_id) {
                clip->start_time = new_start;
                return 0;
            }
            clip = clip->next;
        }
    }

    return -1;  /* not found */
}

/* ── Queries ────────────────────────────────────────────────────────────── */

int wedit_timeline_clip_count(const wedit_timeline_t* tl, int track_index)
{
    if (!tl || track_index < 0 || track_index >= tl->track_count) {
        return 0;
    }

    int count = 0;
    struct wedit_clip* clip = tl->tracks[track_index];
    while (clip) {
        count++;
        clip = clip->next;
    }
    return count;
}

double wedit_timeline_duration(const wedit_timeline_t* tl)
{
    if (!tl) {
        return 0.0;
    }

    double max_end = 0.0;
    for (int i = 0; i < tl->track_count; i++) {
        struct wedit_clip* clip = tl->tracks[i];
        while (clip) {
            double end = clip->start_time + clip->duration;
            if (end > max_end) {
                max_end = end;
            }
            clip = clip->next;
        }
    }
    return max_end;
}

int wedit_timeline_clip_at_time(const wedit_timeline_t* tl,
                                int track_index,
                                double time_seconds)
{
    if (!tl || track_index < 0 || track_index >= tl->track_count) {
        return -1;
    }

    struct wedit_clip* clip = tl->tracks[track_index];
    while (clip) {
        double end = clip->start_time + clip->duration;
        if (time_seconds >= clip->start_time && time_seconds < end) {
            return clip->id;
        }
        clip = clip->next;
    }
    return -1;
}

int wedit_timeline_clips_on_track(const wedit_timeline_t* tl,
                                  int track_index,
                                  wedit_clip_spec_t* out_specs,
                                  size_t max_count)
{
    if (!tl || !out_specs || track_index < 0 || track_index >= tl->track_count) {
        return 0;
    }

    /* Collect clips into an array for sorting */
    int count = 0;
    struct wedit_clip* clip = tl->tracks[track_index];
    struct wedit_clip* arr[256];  /* reasonable max */
    while (clip && count < 256) {
        arr[count++] = clip;
        clip = clip->next;
    }

    /* Simple insertion sort by start_time (small arrays) */
    for (int i = 1; i < count; i++) {
        struct wedit_clip* key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j]->start_time > key->start_time) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }

    /* Copy to output (respecting max_count) */
    int to_copy = count < (int)max_count ? count : (int)max_count;
    for (int i = 0; i < to_copy; i++) {
        out_specs[i].track_index = arr[i]->track_index;
        out_specs[i].start_time = arr[i]->start_time;
        out_specs[i].duration = arr[i]->duration;
        strncpy(out_specs[i].source_path, arr[i]->source_path, 511);
        out_specs[i].source_path[511] = '\0';
        out_specs[i].source_in = arr[i]->source_in;
        out_specs[i].source_out = arr[i]->source_out;
        out_specs[i].transition = arr[i]->transition;
        out_specs[i].transition_duration = arr[i]->transition_duration;
    }

    return to_copy;
}

/* ── Metadata ──────────────────────────────────────────────────────────── */

int wedit_timeline_track_count(const wedit_timeline_t* tl)
{
    return tl ? tl->track_count : 0;
}

double wedit_timeline_fps(const wedit_timeline_t* tl)
{
    return tl ? tl->fps : 0.0;
}

/* ── Render command builder ────────────────────────────────────────────── */

char* wedit_timeline_build_render_command(const wedit_timeline_t* tl,
                                          int video_track,
                                          int audio_track,
                                          const char* output_path,
                                          int width,
                                          int height)
{
    if (!tl || !output_path) {
        return NULL;
    }
    if (video_track < 0 || video_track >= tl->track_count) {
        return NULL;
    }

    /* Build FFmpeg filter_complex string:
     * - For each input, trim to source [in:out]
     * - Set PTS for timeline position
     * - Concatenate all clips on the video track
     * - Same for audio track
     *
     * Example output (simplified):
     *   ffmpeg -i clip1.mp4 -i clip2.mp4 -i song.ogg \
     *     -filter_complex \
     *     "[0:v]trim=0:5,setpts=PTS-STARTPTS[v0]; \
     *      [1:v]trim=0:3,setpts=PTS-STARTPTS[v1]; \
     *      [v0][v1]concat=n=2:v=1:a=0[outv]; \
     *      [2:a]atrim=0:8,asetpts=PTS-STARTPTS[aout]" \
     *     -map "[outv]" -map "[aout]" -c:v libx264 -c:a aac output.mp4
     */

    /* Estimate buffer size: generous for typical timelines */
    size_t buf_size = 4096;
    char* cmd = malloc(buf_size);
    if (!cmd) {
        return NULL;
    }

    /* Collect sorted clips on the video track for filter building */
    int v_count = wedit_timeline_clip_count(tl, video_track);
    wedit_clip_spec_t* v_clips = v_count > 0 ?
        malloc(sizeof(wedit_clip_spec_t) * (size_t)v_count) : NULL;
    if (v_count > 0 && !v_clips) {
        free(cmd);
        return NULL;
    }
    if (v_count > 0) {
        wedit_timeline_clips_on_track(tl, video_track, v_clips, (size_t)v_count);
    }

    /* Collect sorted clips on the audio track */
    int a_count = 0;
    wedit_clip_spec_t* a_clips = NULL;
    if (audio_track >= 0 && audio_track < tl->track_count) {
        a_count = wedit_timeline_clip_count(tl, audio_track);
        if (a_count > 0) {
            a_clips = malloc(sizeof(wedit_clip_spec_t) * (size_t)a_count);
            if (!a_clips) {
                free(cmd);
                free(v_clips);
                return NULL;
            }
            wedit_timeline_clips_on_track(tl, audio_track, a_clips, (size_t)a_count);
        }
    }

    /* Build the filter_complex string */
    int offset = 0;
    offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                       "ffmpeg -y");

    /* Input files: collect unique source paths */
    /* For simplicity, we list each clip's source as a separate input */
    /* In a real implementation, we'd dedupe and use a proper input manager */
    /* For the Cleveland workflow, each clip is already a separate MP4 */
    for (int i = 0; i < v_count; i++) {
        offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                           " -i \"%s\"", v_clips[i].source_path);
    }
    if (a_clips) {
        for (int i = 0; i < a_count; i++) {
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               " -i \"%s\"", a_clips[i].source_path);
        }
    }

    /* Filter complex: trim + concat for video */
    offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                       " -filter_complex \"");

    /* Video clips: trim and set timestamps (skip if single clip, map directly) */
    if (v_count >= 1 && !(v_count == 1 && v_clips[0].source_in == 0.0)) {
        for (int i = 0; i < v_count; i++) {
            double in_t = v_clips[i].source_in;
            double out_t = v_clips[i].source_in + v_clips[i].duration;
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               "[%d:v]trim=%.3f:%.3f,setpts=PTS-STARTPTS[v%d];",
                               i, in_t, out_t, i);
        }
    }

    if (v_count > 1) {
        /* Chain video clips with transitions (xfade) or concat */
        int need_transition = 0;
        for (int i = 0; i < v_count - 1; i++) {
            if (v_clips[i].transition != WEDIT_TRANSITION_NONE &&
                v_clips[i].transition_duration > 0.0) {
                need_transition = 1;
                break;
            }
        }

        if (need_transition) {
            /* Use xfade for chained transitions */
            /* [v0]..[vN] already exist from the input trim above.
               Chain them via xfade: [v0][v1]xfade[out][v1], then
               [v1][v2]xfade -> [v2], etc. */
            double cumulative = v_clips[0].duration;
            for (int i = 1; i < v_count; i++) {
                const char* tname = "fade";
                switch (v_clips[i - 1].transition) {
                    case WEDIT_TRANSITION_DISSOLVE: tname = "dissolve"; break;
                    case WEDIT_TRANSITION_PIXELISE: tname = "pixelise"; break;
                    default: tname = "fade"; break;
                }
                double tdur = v_clips[i - 1].transition_duration;
                offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                    "[v%d][v%d]xfade=transition=%s:duration=%.3f:offset=%.3f[v%d];",
                    i - 1, i, tname, tdur, cumulative, i);
                cumulative += v_clips[i].duration - tdur;
            }
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               "[v%d]format=yuv420p[outv];", v_count - 1);
        } else {
            /* No transitions: use concat */
            for (int i = 0; i < v_count; i++) {
                offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                                   "[v%d]", i);
            }
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               "concat=n=%d:v=1:a=0[outv];", v_count);
        }
    }

    /* Audio clips: trim and concat */
    if (a_count > 0) {
        for (int i = 0; i < a_count; i++) {
            double in_t = a_clips[i].source_in;
            double out_t = a_clips[i].source_in + a_clips[i].duration;
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               "[%d:a]atrim=%.3f:%.3f,asetpts=PTS-STARTPTS[a%d];",
                               v_count + i, in_t, out_t, i);
        }

        if (a_count > 1) {
            /* Concat audio clips: [a0][a1]concat=n=2:v=0:a=1[aout] */
            for (int i = 0; i < a_count; i++) {
                offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                                   "[a%d]", i);
            }
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               "concat=n=%d:v=0:a=1[aout];",
                               a_count);
        } else {
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               "[a0]anull[aout];");
        }
    }

    offset += snprintf(cmd + offset, buf_size - (size_t)offset, "\" ");

    /* Map outputs */
    if (v_count >= 1) {
        if (v_count > 1) {
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               "-map \"[outv]\" ");
        } else {
            /* Single clip: no filter needed, map input directly */
            offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                               "-map 0:v ");
        }
    }
    if (a_count > 0) {
        offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                           "-map \"[aout]\" ");
    }

    /* Encode settings with color grading */
    if (width > 0 && height > 0) {
        offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                           "-vf \"scale=%d:%d,eq=contrast=1.1:saturation=1.05:gamma=0.95\" ",
                           width, height);
    }
    offset += snprintf(cmd + offset, buf_size - (size_t)offset,
                       "-c:v libx264 -preset medium -crf 20 "
                       "-c:a aac -b:a 192k -ar 44100 "
                       "-movflags +faststart \"%s\"",
                       output_path);

    free(v_clips);
    free(a_clips);

    return cmd;
}
