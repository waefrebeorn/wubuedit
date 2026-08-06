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
 * wedit_proxy.h — Proxy clip generation for timeline scrubbing.
 *
 * Inspired by Gausian Native Editor's proxy workflow: generates low-res
 * H.264 proxy files for smooth timeline preview, matching the 2026 Rust
 * editor pattern of from-scratch core + system library for I/O.
 *
 * The proxy path is a simple FFmpeg command invoked via system() —
 * pure C11 calling a CLI tool, consistent with SLERM principles.
 * No FFmpeg deps in the header itself.
 */
#ifndef WEDIT_PROXY_H
#define WEDIT_PROXY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Proxy generation result codes */
enum wedit_proxy_result {
    WEDIT_PROXY_OK = 0,          /* proxy generated successfully */
    WEDIT_PROXY_EXISTS,          /* proxy already exists */
    WEDIT_PROXY_FAILED,          /* FFmpeg command failed */
    WEDIT_PROXY_NOMEM,           /* memory allocation failure */
};

/* Options for proxy generation */
typedef struct {
    int    width;       /* proxy width, 0 = auto (source/4) */
    int    height;      /* proxy height, 0 = auto (source/4) */
    int    crf;         /* quality: 23 default, lower = higher quality */
    int    max_fps;     /* max fps for proxy, 0 = no limit */
    char   preset[32];  /* FFmpeg preset, e.g. "fast", "ultrafast" */
    int    overwrite;   /* overwrite existing proxy files */
} wedit_proxy_opts_t;

/*
 * Generate a proxy clip for the given source file.
 * The proxy path is derived from source_path by appending "_proxy"
 * to the filename stem (e.g. clip.mp4 -> clip_proxy.mp4).
 *
 * Returns WEDIT_PROXY_OK on success, or an error code from the enum.
 * On success, proxy_path is filled with the generated path.
 */
enum wedit_proxy_result wedit_proxy_generate(
    const char* source_path,
    char* proxy_path,       /* output: filled with proxy file path */
    size_t proxy_path_size,
    const wedit_proxy_opts_t* opts);

/*
 * Check if a proxy file exists and is valid for the given source.
 * Returns 1 if valid, 0 if missing or stale.
 */
int wedit_proxy_exists(const char* source_path, char* proxy_path, size_t size);

/*
 * Initialize default proxy options.
 * Default: width=0 (auto), height=0 (auto), crf=23, max_fps=15,
 *        preset="fast", overwrite=1.
 */
void wedit_proxy_default_opts(wedit_proxy_opts_t* opts);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_PROXY_H */
