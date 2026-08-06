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
 * wedit_edl.c — CMX 3600 EDL export implementation.
 *
 * EDL format is the industry standard for exchange between NLEs.
 * Each event represents a clip or transition with timecode in/out points.
 *
 * Format reference (CMX 3600):
 *   TITLE: MY PROJECT
 *   FCM: NON-DROP FRAME
 *   001  V     C        001:00:00:00 001:00:05:00 00:00:00:00
 *   * REEL    MY_CLIP    PATH: /path/to/clip.mp4
 *
 * This implementation generates EDLs compatible with Avid Media Composer,
 * Final Cut Pro (via 7-point EDL importer), and DaVinci Resolve.
 */

#include "wedit_edl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Extract the filename (without path) from a source path.
 * Returns a pointer within the source string, or "unknown" if path is empty.
 */
static const char* filename_from_path(const char* path)
{
    if (!path || path[0] == '\0') {
        return "unknown";
    }
    const char* last_slash = strrchr(path, '/');
    return last_slash ? (last_slash + 1) : path;
}

/* Convert seconds to timecode string "HH:MM:SS:FF" */
void wedit_edl_timecode(double seconds, double fps, char* tc_out, size_t size)
{
    if (!tc_out || size < 12 || fps <= 0.0) {
        if (tc_out && size > 0) tc_out[0] = '\0';
        return;
    }

    double total_frames = seconds * fps;
    long total = (long)llround(total_frames);

    int frames = (int)(total % (long)(fps + 0.5));
    total /= (long)(fps + 0.5);

    int secs = total % 60;
    total /= 60;

    int mins = total % 60;
    total /= 60;

    int hours = total;

    snprintf(tc_out, size, "%02d:%02d:%02d:%02d", hours, mins, secs, frames);
}

/* Build a reel name from a source path (filename without extension).
 * Truncates to 8 characters (EDL standard for 35mm-style reel names).
 */
void wedit_edl_reel_name(const char* source_path, char* reel_out, size_t size)
{
    if (!reel_out || size == 0) return;

    const char* filename = filename_from_path(source_path);

    /* Strip extension */
    char stem[256];
    const char* dot = strrchr(filename, '.');
    if (dot) {
        size_t stem_len = (size_t)(dot - filename);
        if (stem_len > sizeof(stem) - 1) stem_len = sizeof(stem) - 1;
        memcpy(stem, filename, stem_len);
        stem[stem_len] = '\0';
    } else {
        strncpy(stem, filename, sizeof(stem) - 1);
        stem[sizeof(stem) - 1] = '\0';
    }

    /* Truncate to 8 chars for EDL standard */
    if (strlen(stem) > 8) {
        stem[8] = '\0';
    }

    strncpy(reel_out, stem, size - 1);
    reel_out[size - 1] = '\0';
}

/* Export timeline to EDL string.
 * Caller must free() the returned string.
 */
char* wedit_edl_export_string(const wedit_timeline_t* tl,
                               int video_track,
                               int audio_track,
                               const char* title)
{
    if (!tl || video_track < 0 || video_track >= wedit_timeline_track_count(tl)) {
        return NULL;
    }

    double fps = wedit_timeline_fps(tl);
    if (fps <= 0.0) fps = 24.0;  /* safe default */

    /* Allocate a generous buffer */
    size_t buf_size = 8192;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    buf[0] = '\0';

    int offset = 0;

    /* Header */
    const char* use_title = title ? title : "WubuEdit Project";
    offset += snprintf(buf + offset, buf_size - (size_t)offset,
                       "TITLE: %s\n"
                       "FCM: NON-DROP FRAME\n",
                       use_title);

    /* Collect sorted clips on video track */
    int v_count = wedit_timeline_clip_count(tl, video_track);
    if (v_count == 0) {
        offset += snprintf(buf + offset, buf_size - (size_t)offset,
                           "WARNING: No clips on video track %d\n", video_track);
        return buf;
    }

    wedit_clip_spec_t* v_clips = calloc((size_t)v_count, sizeof(wedit_clip_spec_t));
    if (!v_clips) {
        free(buf);
        return NULL;
    }
    wedit_timeline_clips_on_track(tl, video_track, v_clips, (size_t)v_count);

    /* Collect audio clips if track specified */
    int a_count = 0;
    wedit_clip_spec_t* a_clips = NULL;
    if (audio_track >= 0 && audio_track < wedit_timeline_track_count(tl)) {
        a_count = wedit_timeline_clip_count(tl, audio_track);
        if (a_count > 0) {
            a_clips = calloc((size_t)a_count, sizeof(wedit_clip_spec_t));
            if (!a_clips) {
                free(buf);
                free(v_clips);
                return NULL;
            }
            wedit_timeline_clips_on_track(tl, audio_track, a_clips, (size_t)a_count);
        }
    }

    /* Track counter for EDL event numbers */
    int event_num = 1;

    /* Generate EDL events for video clips */
    /*
     * CMX 3600 format per event:
     *   001  V     C        01:00:00:00 01:00:05:00 01:00:00:00
     *   * REEL_NAME                               PATH: /path/to/file.mp4
     */
    for (int i = 0; i < v_count; i++) {
        char tc_in[16], tc_out[16], tc_dur[16];
        char reel[32];

        double clip_start = v_clips[i].start_time;
        double clip_end = clip_start + v_clips[i].duration;
        double src_in = v_clips[i].source_in;

        wedit_edl_timecode(clip_start, fps, tc_in, sizeof(tc_in));
        wedit_edl_timecode(clip_end, fps, tc_out, sizeof(tc_out));
        wedit_edl_timecode(src_in, fps, tc_dur, sizeof(tc_dur));

        wedit_edl_reel_name(v_clips[i].source_path, reel, sizeof(reel));

        /* Video event line */
        offset += snprintf(buf + offset, buf_size - (size_t)offset,
                           "%03d  V     C        %s %s %s\n",
                           event_num, tc_in, tc_out, tc_dur);

        /* Source reel name */
        offset += snprintf(buf + offset, buf_size - (size_t)offset,
                           "* %-16s                        PATH: %s\n",
                           reel, v_clips[i].source_path);

        /* Handle transitions */
        if (i < v_count - 1 && v_clips[i].transition != WEDIT_TRANSITION_NONE &&
            v_clips[i].transition_duration > 0.0) {

            /* For transitions, we emit an additional event for the overlap */
            double trans_end = clip_end + v_clips[i].transition_duration;
            char tc_trans_start[16], tc_trans_end[16];

            wedit_edl_timecode(clip_end - v_clips[i].transition_duration, fps,
                               tc_trans_start, sizeof(tc_trans_start));
            wedit_edl_timecode(trans_end, fps, tc_trans_end, sizeof(tc_trans_end));

            event_num++;

            switch (v_clips[i].transition) {
                case WEDIT_TRANSITION_DISSOLVE:
                    offset += snprintf(buf + offset, buf_size - (size_t)offset,
                                       "%03d  V     D        %s %s %s  /sec\n",
                                       event_num, tc_trans_start, tc_trans_end,
                                       tc_trans_start);
                    break;
                case WEDIT_TRANSITION_PIXELISE:
                    /* Pixelise is not standard in CMX 3600 — emit as D with effect note */
                    offset += snprintf(buf + offset, buf_size - (size_t)offset,
                                       "%03d  V     D        %s %s %s  /effect pixelise /sec\n",
                                       event_num, tc_trans_start, tc_trans_end,
                                       tc_trans_start);
                    break;
                default:
                    /* FADE as transition */
                    offset += snprintf(buf + offset, buf_size - (size_t)offset,
                                       "%03d  V     D        %s %s %s  /sec\n",
                                       event_num, tc_trans_start, tc_trans_end,
                                       tc_trans_start);
                    break;
            }
        }

        /* Add audio event if available */
        if (a_count > 0 && i < a_count) {
            char a_tc_in[16], a_tc_out[16], a_tc_dur[16];
            char a_reel[32];

            double a_clip_start = a_clips[i].start_time;
            double a_clip_end = a_clip_start + a_clips[i].duration;
            double a_src_in = a_clips[i].source_in;

            wedit_edl_timecode(a_clip_start, fps, a_tc_in, sizeof(a_tc_in));
            wedit_edl_timecode(a_clip_end, fps, a_tc_out, sizeof(a_tc_out));
            wedit_edl_timecode(a_src_in, fps, a_tc_dur, sizeof(a_tc_dur));

            wedit_edl_reel_name(a_clips[i].source_path, a_reel, sizeof(a_reel));

            event_num++;
            offset += snprintf(buf + offset, buf_size - (size_t)offset,
                               "%03d  A     C        %s %s %s\n",
                               event_num, a_tc_in, a_tc_out, a_tc_dur);
            offset += snprintf(buf + offset, buf_size - (size_t)offset,
                               "* %-16s                        PATH: %s\n",
                               a_reel, a_clips[i].source_path);
        }

        event_num++;
    }

    /* Footer */
    offset += snprintf(buf + offset, buf_size - (size_t)offset,
                       "\nEND\n");

    free(v_clips);
    free(a_clips);

    return buf;
}

/* Export timeline to EDL file */
int wedit_edl_export(const wedit_timeline_t* tl,
                      const char* filepath,
                      int video_track,
                      int audio_track,
                      const char* title)
{
    char* edl_str = wedit_edl_export_string(tl, video_track, audio_track, title);
    if (!edl_str) {
        return -1;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        free(edl_str);
        return -1;
    }

    fprintf(fp, "%s", edl_str);
    fclose(fp);
    free(edl_str);

    return 0;
}
