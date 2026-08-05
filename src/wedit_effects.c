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
 * wedit_effects.c — Effects pipeline implementation (pure C11).
 *
 * Builds FFmpeg filter strings from effect parameters.
 * No FFmpeg headers required — this is pure string generation.
 */
#include "wedit_effects.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define INITIAL_CAPACITY 8
#define MAX_FILTER_LEN 8192

int wedit_effects_init(wedit_effect_list_t* list)
{
    if (!list) return -1;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return 0;
}

int wedit_effects_add(wedit_effect_list_t* list, const wedit_effect_t* effect)
{
    if (!list || !effect) return -1;

    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : INITIAL_CAPACITY;
        wedit_effect_t* new_items = realloc(list->items,
            new_cap * sizeof(wedit_effect_t));
        if (!new_items) return -1;
        list->items = new_items;
        list->capacity = new_cap;
    }

    list->items[list->count] = *effect;
    list->count++;
    return 0;
}

void wedit_effects_free(wedit_effect_list_t* list)
{
    if (!list) return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

char* wedit_effects_build_video_filter(const wedit_effect_list_t* list,
                                        const char* existing)
{
    if (!list || list->count == 0) return NULL;

    char buf[MAX_FILTER_LEN];
    size_t offset = 0;

    /* Copy existing chain if provided */
    if (existing && offset + strlen(existing) < MAX_FILTER_LEN) {
        offset += snprintf(buf + offset, MAX_FILTER_LEN - offset, "%s,", existing);
    }

    for (size_t i = 0; i < list->count; i++) {
        const wedit_effect_t* e = &list->items[i];
        if (offset >= MAX_FILTER_LEN - 256) break;

        switch (e->type) {
        case WEDIT_EFFECT_COLOR_GRADE: {
            const wedit_color_grade_t* g = &e->params.color_grade;
            offset += snprintf(buf + offset, MAX_FILTER_LEN - offset,
                "eq=contrast=%.2f:saturation=%.2f:gamma=%.2f:brightness=%.2f",
                g->contrast, g->saturation, g->gamma, g->brightness);
            break;
        }
        case WEDIT_EFFECT_LUT3D: {
            offset += snprintf(buf + offset, MAX_FILTER_LEN - offset,
                "lut3d=%s", e->params.lut3d.lut_path);
            break;
        }
        case WEDIT_EFFECT_TEXT_OVERLAY: {
            const wedit_text_overlay_t* t = &e->params.text;
            char fontfile_part[512] = {0};
            if (t->fontfile[0]) {
                /* Leave room for the ":fontfile=" prefix (10 chars) */
                snprintf(fontfile_part, sizeof(fontfile_part), ":fontfile=%.501s",
                    t->fontfile);
            }
            const char* fp = t->fontfile[0] ? fontfile_part : "";
            offset += snprintf(buf + offset, MAX_FILTER_LEN - offset,
                "drawtext=text='%s'%s:fontcolor=%s:fontsize=%d"
                ":x=%.0fw*:y=%.0fh*",
                t->text, fp, t->fontcolor, t->fontsize,
                t->x * 1000, t->y * 1000);
            break;
        }
        case WEDIT_EFFECT_OVERLAY: {
            const wedit_overlay_t* o = &e->params.overlay;
            offset += snprintf(buf + offset, MAX_FILTER_LEN - offset,
                "overlay=%d:%d:w=%d:h=%d",
                (int)(o->x * 1920), (int)(o->y * 1080),
                (int)(o->width * 1920), (int)(o->height * 1080));
            break;
        }
        case WEDIT_EFFECT_DEINTERLACE: {
            offset += snprintf(buf + offset, MAX_FILTER_LEN - offset,
                "%s", e->params.overlay.source_path);
            break;
        }
        default:
            break;
        }

        if (i + 1 < list->count)
            offset += snprintf(buf + offset, MAX_FILTER_LEN - offset, ",");
    }

    if (offset == 0) return NULL;
    char* result = malloc(offset + 1);
    if (!result) return NULL;
    memcpy(result, buf, offset + 1);
    return result;
}

/* ── Convenience wrappers ──────────────────────────────────────────────── */

void wedit_effects_add_color_grade(wedit_effect_list_t* list,
    double contrast, double saturation, double gamma, double brightness)
{
    if (!list) return;
    wedit_effect_t e;
    memset(&e, 0, sizeof(e));
    e.type = WEDIT_EFFECT_COLOR_GRADE;
    e.params.color_grade.contrast = contrast;
    e.params.color_grade.saturation = saturation;
    e.params.color_grade.gamma = gamma;
    e.params.color_grade.brightness = brightness;
    wedit_effects_add(list, &e);
}

void wedit_effects_add_lut3d(wedit_effect_list_t* list, const char* lut_path)
{
    if (!list || !lut_path) return;
    wedit_effect_t e;
    memset(&e, 0, sizeof(e));
    e.type = WEDIT_EFFECT_LUT3D;
    strncpy(e.params.lut3d.lut_path, lut_path, sizeof(e.params.lut3d.lut_path) - 1);
    wedit_effects_add(list, &e);
}

void wedit_effects_add_text(wedit_effect_list_t* list,
    const char* text, const char* fontfile, const char* fontcolor,
    int fontsize, double x, double y,
    double start_time, double end_time)
{
    if (!list || !text) return;
    wedit_effect_t e;
    memset(&e, 0, sizeof(e));
    e.type = WEDIT_EFFECT_TEXT_OVERLAY;
    strncpy(e.params.text.text, text, sizeof(e.params.text.text) - 1);
    if (fontfile) strncpy(e.params.text.fontfile, fontfile,
        sizeof(e.params.text.fontfile) - 1);
    if (fontcolor) strncpy(e.params.text.fontcolor, fontcolor,
        sizeof(e.params.text.fontcolor) - 1);
    else strncpy(e.params.text.fontcolor, "white",
        sizeof(e.params.text.fontcolor) - 1);
    e.params.text.fontsize = fontsize;
    e.params.text.x = x;
    e.params.text.y = y;
    e.params.text.start_time = start_time;
    e.params.text.end_time = end_time;
    wedit_effects_add(list, &e);
}

void wedit_effects_add_overlay(wedit_effect_list_t* list,
    const char* source_path, double x, double y,
    double width, double height, double start_time, double end_time)
{
    if (!list || !source_path) return;
    wedit_effect_t e;
    memset(&e, 0, sizeof(e));
    e.type = WEDIT_EFFECT_OVERLAY;
    strncpy(e.params.overlay.source_path, source_path,
        sizeof(e.params.overlay.source_path) - 1);
    e.params.overlay.x = x;
    e.params.overlay.y = y;
    e.params.overlay.width = width;
    e.params.overlay.height = height;
    e.params.overlay.start_time = start_time;
    e.params.overlay.end_time = end_time;
    wedit_effects_add(list, &e);
}

void wedit_effects_add_audio_duck(wedit_effect_list_t* list,
    double duck_level, double start_time, double duration)
{
    if (!list) return;
    wedit_effect_t e;
    memset(&e, 0, sizeof(e));
    e.type = WEDIT_EFFECT_AUDIO_DUCK;
    e.params.duck.duck_level = duck_level;
    e.params.duck.start_time = start_time;
    e.params.duck.duration = duration;
    wedit_effects_add(list, &e);
}

void wedit_effects_add_deinterlace(wedit_effect_list_t* list, int mode, int parity)
{
    if (!list) return;
    wedit_effect_t e;
    memset(&e, 0, sizeof(e));
    e.type = WEDIT_EFFECT_DEINTERLACE;
    snprintf(e.params.overlay.source_path,
        sizeof(e.params.overlay.source_path),
        "yadif=%d:%d", mode, parity);
    wedit_effects_add(list, &e);
}

char* wedit_effects_build_audio_filter(const wedit_effect_list_t* list)
{
    if (!list || list->count == 0) return NULL;

    char buf[MAX_FILTER_LEN];
    size_t offset = 0;

    for (size_t i = 0; i < list->count; i++) {
        const wedit_effect_t* e = &list->items[i];
        if (offset >= MAX_FILTER_LEN - 256) break;

        if (e->type == WEDIT_EFFECT_AUDIO_DUCK) {
            const wedit_audio_duck_t* d = &e->params.duck;
            offset += snprintf(buf + offset, MAX_FILTER_LEN - offset,
                "volume=%.2f:enable='between(t,%.1f,%.1f)'",
                d->duck_level, d->start_time, d->start_time + d->duration);
            if (i + 1 < list->count)
                offset += snprintf(buf + offset, MAX_FILTER_LEN - offset, ",");
        }
    }

    if (offset == 0) return NULL;
    char* result = malloc(offset + 1);
    if (!result) return NULL;
    memcpy(result, buf, offset + 1);
    return result;
}
