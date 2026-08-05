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
 * wedit_project.h — Project file (.wed) serializer public API.
 * Opaque project handle for save/load of WubuEdit timeline projects.
 */
#ifndef WEDIT_PROJECT_H
#define WEDIT_PROJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wedit_timeline.h"

/* Opaque project handle */
typedef struct wedit_project wedit_project_t;

/*
 * Create a project from an existing timeline.
 * The project takes ownership of the timeline.
 */
wedit_project_t* wedit_project_from_timeline(wedit_timeline_t* tl);

/*
 * Save project to a .wed file (JSON format).
 * Returns 0 on success, -1 on failure.
 */
int wedit_project_save(const wedit_project_t* proj, const char* path);

/*
 * Load a project from a .wed file.
 * Returns a new timeline (caller must destroy), or NULL on failure.
 */
wedit_timeline_t* wedit_project_load(const char* path);

/*
 * Get the timeline from a project (for manipulation before save).
 */
wedit_timeline_t* wedit_project_get_timeline(wedit_project_t* proj);

/* Destroy a project. Safe to call with NULL. */
void wedit_project_destroy(wedit_project_t* proj);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_PROJECT_H */