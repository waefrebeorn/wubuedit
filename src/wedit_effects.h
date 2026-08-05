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
 * wedit_effects.h — Effects pipeline public API.
 *
 * Supports: color grading (eq/LUT3D), text overlays (drawtext),
 * picture-in-picture (overlay), and audio ducking.
 * Pure C11 — no FFmpeg deps in this header.
 */
#ifndef WEDIT_EFFECTS_H
#define WEDIT_EFFECTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Effect type */
enum wedit_effect_type {
    WEDIT_EFFECT_COLOR_GRADE,   /* eq filter with contrast/saturation/gamma */
    WEDIT_EFFECT_LUT3D,         /* 3D LUT color grading */
    WEDIT_EFFECT_TEXT_OVERLAY,  /* drawtext filter */
    WEDIT_EFFECT_OVERLAY,       /* video overlay (picture-in-picture) */
    WEDIT_EFFECT_AUDIO_DUCK,    /* volume envelope for ducking */
    WEDIT_EFFECT_DEINTERLACE,   /* yadif deinterlace filter */
};

/* Color grade parameters */
typedef struct {
    double contrast;     /* 0.0 - 3.0 */
    double saturation;   /* 0.0 - 3.0 */
    double gamma;        /* 0.0 - 5.0 */
    double brightness;   /* -1.0 - 1.0 */
} wedit_color_grade_t;

/* LUT3D parameters */
typedef struct {
    char lut_path[512];  /* path to .cube file */
} wedit_lut3d_t;

/* Text overlay parameters */
typedef struct {
    char text[256];
    char fontfile[512];
    char fontcolor[64];  /* e.g. "white", "0xFFFFFF" */
    int  fontsize;
    double x;            /* position, normalized 0.0-1.0 */
    double y;
    double start_time;   /* when to show text (seconds) */
    double end_time;     /* when to hide text (seconds) */
} wedit_text_overlay_t;

/* Video overlay (picture-in-picture) parameters */
typedef struct {
    char source_path[512];
    double x;            /* position, normalized 0.0-1.0 */
    double y;
    double width;        /* normalized 0.0-1.0 */
    double height;
    double start_time;
    double end_time;
} wedit_overlay_t;

/* Audio ducking parameters */
typedef struct {
    double duck_level;    /* 0.0 - 1.0, volume during ducking */
    double start_time;
    double duration;
} wedit_audio_duck_t;

/* A single effect instance on a clip or timeline */
typedef struct {
    enum wedit_effect_type type;
    union {
        wedit_color_grade_t color_grade;
        wedit_lut3d_t lut3d;
        wedit_text_overlay_t text;
        wedit_overlay_t overlay;
        wedit_audio_duck_t duck;
    } params;
} wedit_effect_t;

/* Effect list — owned by the caller */
typedef struct {
    wedit_effect_t* items;
    size_t count;
    size_t capacity;
} wedit_effect_list_t;

/* Initialize an empty effect list. Returns 0 on success. */
int wedit_effects_init(wedit_effect_list_t* list);

/* Add an effect to the list. Returns 0 on success, -1 on failure. */
int wedit_effects_add(wedit_effect_list_t* list, const wedit_effect_t* effect);

/* Free all memory in an effect list. */
void wedit_effects_free(wedit_effect_list_t* list);

/* Build an FFmpeg filter string for video effects.
 * Caller must free the returned string. Returns NULL on failure.
 * Appends to existing filter string if non-NULL. */
char* wedit_effects_build_video_filter(const wedit_effect_list_t* list,
                                        const char* existing);

/* Build an FFmpeg filter string for audio effects.
 * Caller must free the returned string. Returns NULL if no audio effects. */
char* wedit_effects_build_audio_filter(const wedit_effect_list_t* list);

/* Add a deinterlace effect (yadif filter).
 * mode=0 for spatial+temporal (default), mode=1 for temporal-only, mode=2 for spatial-only.
 * parity=-1 auto-detect (default), 0 top-field-first, 1 bottom-field-first. */
void wedit_effects_add_deinterlace(wedit_effect_list_t* list, int mode, int parity);

/* Add a basic color grade effect. */
void wedit_effects_add_color_grade(wedit_effect_list_t* list,
    double contrast, double saturation, double gamma, double brightness);

/* Add a LUT3D color grading effect from a .cube file path. */
void wedit_effects_add_lut3d(wedit_effect_list_t* list, const char* lut_path);

/* Add a text overlay effect. */
void wedit_effects_add_text(wedit_effect_list_t* list,
    const char* text, const char* fontfile, const char* fontcolor,
    int fontsize, double x, double y,
    double start_time, double end_time);

/* Add a video overlay (picture-in-picture). */
void wedit_effects_add_overlay(wedit_effect_list_t* list,
    const char* source_path, double x, double y,
    double width, double height, double start_time, double end_time);

/* Add an audio ducking effect. */
void wedit_effects_add_audio_duck(wedit_effect_list_t* list,
    double duck_level, double start_time, double duration);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_EFFECTS_H */
