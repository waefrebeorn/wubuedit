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
 * wedit_edl.h — EDL (Edit Decision List) export for WubuEdit.
 *
 * Generates CMX 3600 format EDL files for round-tripping with professional
 * NLEs (Avid, Final Cut Pro, DaVinci Resolve). This is the exporter
 * pattern observed in Gausian Native Editor (2026).
 *
 * EDL format reference: CMX 3600 is the industry standard for representing
 * cut lists, transitions, and effects in a text file consumable by any NLE.
 */
#ifndef WEDIT_EDL_H
#define WEDIT_EDL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wedit_timeline.h"
#include <stddef.h>
#include <stdio.h>

/* EDL event type */
enum wedit_edl_event_type {
    WEDIT_EDL_CUT = 0,      /* standard hard cut */
    WEDIT_EDL_DISSOLVE,     /* cross-dissolve transition */
    WEDIT_EDL_FADE_IN,      /* fade in from black */
    WEDIT_EDL_FADE_OUT,     /* fade out to black */
};

/*
 * Export a timeline to a CMX 3600 EDL file.
 * video_track: index of the video track to export
 * audio_track: index of the audio track to export (or -1 to skip audio)
 * title: EDL title string (project name)
 *
 * Returns: 0 on success, -1 on error.
 */
int wedit_edl_export(const wedit_timeline_t* tl,
                      const char* filepath,
                      int video_track,
                      int audio_track,
                      const char* title);

/*
 * Export a timeline to an in-memory EDL string.
 * Caller must free() the returned string.
 */
char* wedit_edl_export_string(const wedit_timeline_t* tl,
                               int video_track,
                               int audio_track,
                               const char* title);

/*
 * Convert a frame number + fps to a timecode string (HH:MM:SS:FF).
 * tc_out must have room for at least 12 chars ("HH:MM:SS:FF\0").
 */
void wedit_edl_timecode(double seconds, double fps, char* tc_out, size_t size);

/*
 * Convert a frame number + fps to a reel name (for EDL source identification).
 * reel_out must have room for at least 32 chars.
 */
void wedit_edl_reel_name(const char* source_path, char* reel_out, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_EDL_H */
