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
 * wedit_demux.h — libavformat demuxer public API.
 * Opaque struct + minimal interface. Apps never see AVFormatContext.
 */
#ifndef WEDIT_DEMUX_H
#define WEDIT_DEMUX_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WEDIT_HAS_FFMPEG

#include <stddef.h>

/* Opaque media handle */
struct wedit_media;

/* Open a media file, extract stream info. NULL on failure. */
struct wedit_media* wedit_media_open(const char* path);

/* Getters */
double wedit_media_duration(struct wedit_media* media);
int    wedit_media_width(struct wedit_media* media);
int    wedit_media_height(struct wedit_media* media);

/* Close and free */
void wedit_media_close(struct wedit_media* media);

/* Initialize FFmpeg libraries (call once at program start in native build) */
void wedit_media_init(void);

#else

/* Stub when FFmpeg not available */
#define wedit_media_open(p) ((void*)0)
#define wedit_media_duration(m) (0.0)
#define wedit_media_width(m) (0)
#define wedit_media_height(m) (0)
#define wedit_media_close(m) ((void)0)
#define wedit_media_init() ((void)0)

#endif /* WEDIT_HAS_FFMPEG */

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_DEMUX_H */
