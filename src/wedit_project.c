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
 * wedit_project.c — Project file (.wed) serializer implementation.
 *
 * Clean-room JSON serializer for WubuEdit timeline projects.
 * Uses a minimal hand-written JSON writer — no external JSON library.
 * This is the WuBuOffice philosophy: write every byte yourself.
 */
#include "wedit_project.h"
#include "wedit_timeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wedit_project {
    wedit_timeline_t* timeline;
    int version;
};

/* ── Project lifecycle ─────────────────────────────────────────────────── */

wedit_project_t* wedit_project_from_timeline(wedit_timeline_t* tl)
{
    if (!tl) return NULL;
    wedit_project_t* proj = calloc(1, sizeof(wedit_project_t));
    if (!proj) return NULL;
    proj->timeline = tl;
    proj->version = 1;
    return proj;
}

wedit_timeline_t* wedit_project_get_timeline(wedit_project_t* proj)
{
    return proj ? proj->timeline : NULL;
}

void wedit_project_destroy(wedit_project_t* proj)
{
    if (!proj) return;
    if (proj->timeline) {
        wedit_timeline_destroy(proj->timeline);
    }
    free(proj);
}

/* ── Save ──────────────────────────────────────────────────────────────── */

int wedit_project_save(const wedit_project_t* proj, const char* path)
{
    if (!proj || !proj->timeline || !path) {
        return -1;
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        return -1;
    }

    int track_count = wedit_timeline_track_count(proj->timeline);
    double fps = wedit_timeline_fps(proj->timeline);

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"fps\": %.6f,\n", fps);
    fprintf(f, "  \"tracks\": %d,\n", track_count);
    fprintf(f, "  \"clips\": [\n");

    int clip_id = 0;
    for (int t = 0; t < track_count; t++) {
        int count = wedit_timeline_clip_count(proj->timeline, t);
        if (count == 0) continue;

        wedit_clip_spec_t* clips = malloc(sizeof(wedit_clip_spec_t) * (size_t)count);
        if (!clips) {
            fclose(f);
            return -1;
        }
        wedit_timeline_clips_on_track(proj->timeline, t, clips, (size_t)count);

        for (int i = 0; i < count; i++) {
            fprintf(f, "    {\n");
            fprintf(f, "      \"track\": %d,\n", clips[i].track_index);
            fprintf(f, "      \"start\": %.6f,\n", clips[i].start_time);
            fprintf(f, "      \"duration\": %.6f,\n", clips[i].duration);
            fprintf(f, "      \"source\": \"%s\",\n", clips[i].source_path);
            fprintf(f, "      \"in\": %.6f,\n", clips[i].source_in);
            fprintf(f, "      \"out\": %.6f,\n", clips[i].source_out);
            const char* tname = "none";
            switch (clips[i].transition) {
                case WEDIT_TRANSITION_FADE:     tname = "fade"; break;
                case WEDIT_TRANSITION_DISSOLVE: tname = "dissolve"; break;
                case WEDIT_TRANSITION_PIXELISE: tname = "pixelise"; break;
                default: tname = "none"; break;
            }
            fprintf(f, "      \"transition\": \"%s\",\n", tname);
            fprintf(f, "      \"transition_duration\": %.6f\n", clips[i].transition_duration);
            fprintf(f, "    }%s\n", (i < count - 1 || t < track_count - 1) ? "," : "");
            clip_id++;
        }
        free(clips);
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

/* ── Load ─────────────────────────────────────────────────────────────── */

wedit_timeline_t* wedit_project_load(const char* path)
{
    if (!path) {
        return NULL;
    }

    /* Minimal JSON parser — read the file and extract fields */
    FILE* f = fopen(path, "r");
    if (!f) {
        return NULL;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char* data = malloc((size_t)fsize + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, 1, (size_t)fsize, f);
    fclose(f);
    data[read] = '\0';

    /* Parse top-level fields */
    int version = 1;
    double fps = 30.0;
    int track_count = 1;

    char* p = strstr(data, "\"version\"");
    if (p) {
        p = strchr(p, ':');
        if (p) version = atoi(p + 1);
    }
    p = strstr(data, "\"fps\"");
    if (p) {
        p = strchr(p, ':');
        if (p) fps = atof(p + 1);
    }
    p = strstr(data, "\"tracks\"");
    if (p) {
        p = strchr(p, ':');
        if (p) track_count = atoi(p + 1);
    }

    if (version != 1) {
        free(data);
        return NULL;
    }

    wedit_timeline_t* tl = wedit_timeline_create(track_count, fps);
    if (!tl) {
        free(data);
        return NULL;
    }

    /* Parse clips — simplified: scan for clip objects */
    p = data;
    while ((p = strstr(p, "\"track\"")) != NULL) {
        wedit_clip_spec_t spec;
        memset(&spec, 0, sizeof(spec));

        spec.track_index = atoi(strchr(p, ':') + 1);
        p = strstr(p, "\"start\"");
        spec.start_time = atof(strchr(p, ':') + 1);
        p = strstr(p, "\"duration\"");
        spec.duration = atof(strchr(p, ':') + 1);
        p = strstr(p, "\"source\"");
        p = strchr(p, ':') + 1;
        /* skip whitespace and quote */
        while (*p == ' ' || *p == '\"') p++;
        char* end = strchr(p, '\"');
        if (end) {
            size_t len = (size_t)(end - p);
            strncpy(spec.source_path, p, len < 511 ? len : 511);
            spec.source_path[511] = '\0';
        }
        p = strstr(p, "\"in\"");
        spec.source_in = atof(strchr(p, ':') + 1);
        p = strstr(p, "\"out\"");
        spec.source_out = atof(strchr(p, ':') + 1);
        p = strstr(p, "\"transition\"");
        p = strchr(p, ':') + 1;
        while (*p == ' ' || *p == '\"') p++;
        char* tend = strchr(p, '\"');
        char tname[32] = {0};
        if (tend) {
            size_t len = (size_t)(tend - p);
            if (len > 31) len = 31;
            strncpy(tname, p, len);
        }
        if (strcmp(tname, "fade") == 0) spec.transition = WEDIT_TRANSITION_FADE;
        else if (strcmp(tname, "dissolve") == 0) spec.transition = WEDIT_TRANSITION_DISSOLVE;
        else if (strcmp(tname, "pixelise") == 0) spec.transition = WEDIT_TRANSITION_PIXELISE;
        else spec.transition = WEDIT_TRANSITION_NONE;

        p = strstr(p, "\"transition_duration\"");
        spec.transition_duration = atof(strchr(p, ':') + 1);

        wedit_timeline_add_clip(tl, &spec);

        p = strchr(p, '}');
        if (p) p++;
    }

    free(data);
    return tl;
}
