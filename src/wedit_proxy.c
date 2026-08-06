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
 * wedit_proxy.c — Proxy clip generation for timeline scrubbing.
 *
 * Implementation of the proxy workflow from Gausian Native Editor (2026).
 * Generates low-res H.264 proxy clips for smooth timeline preview.
 *
 * The proxy is generated via FFmpeg invoked through system(). The proxy
 * file is named by appending "_proxy" to the source filename stem.
 * A .proxy_info file stores the source mtime/size for cache invalidation.
 */

#include "wedit_proxy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

/* Default proxy options */
void wedit_proxy_default_opts(wedit_proxy_opts_t* opts)
{
    if (!opts) return;
    opts->width = 0;          /* auto: source / 4 */
    opts->height = 0;         /* auto: source / 4 */
    opts->crf = 23;
    opts->max_fps = 15;
    opts->overwrite = 1;
    strcpy(opts->preset, "fast");
}

/* Build the proxy file path from source path.
 * Rule: insert "_proxy" before the extension.
 * e.g. /path/to/clip.mp4 -> /path/to/clip_proxy.mp4
 */
static void build_proxy_path(const char* source_path,
                              char* proxy_path, size_t size)
{
    /* Find the last path separator */
    const char* last_slash = strrchr(source_path, '/');
    const char* base = last_slash ? (last_slash + 1) : source_path;

    /* Find the last dot (extension) */
    const char* dot = strrchr(base, '.');
    if (!dot) {
        /* No extension — just append _proxy */
        snprintf(proxy_path, size, "%s_proxy", source_path);
        return;
    }

    /* Split: dir + base_without_ext + ".mp4" */
    size_t dir_len = (size_t)(dot - source_path);  /* position of the dot in full path */
    size_t base_len = (size_t)(dot - base);        /* length of name without ext */

    /* Copy directory portion */
    snprintf(proxy_path, size, "%.*s", (int)dir_len, source_path);

    /* Insert "_proxy" before the dot */
    strncat(proxy_path, "_proxy", size - strlen(proxy_path) - 1);

    /* Append new extension .mp4 */
    strncat(proxy_path, ".mp4", size - strlen(proxy_path) - 1);
}

/* Build the proxy info file path (stores mtime + size of source).
 * e.g. /path/to/clip_proxy.mp4 -> /path/to/clip_proxy.mp4.proxy_info
 */
static void build_proxy_info_path(const char* proxy_path,
                                   char* info_path, size_t size)
{
    snprintf(info_path, size, "%s.proxy_info", proxy_path);
}

/* Write proxy info file with source mtime and filesize.
 * Returns 0 on success, -1 on failure.
 */
static int write_proxy_info(const char* info_path,
                             const char* source_path)
{
    struct stat src_stat;
    if (stat(source_path, &src_stat) != 0) {
        return -1;
    }

    FILE* fp = fopen(info_path, "w");
    if (!fp) {
        return -1;
    }
    fprintf(fp, "%ld %ld\n",
            (long)src_stat.st_mtime,
            (long)src_stat.st_size);
    fclose(fp);
    return 0;
}

/* Check if proxy is valid by comparing source mtime/size to stored info.
 * Returns 1 if valid, 0 if stale or missing info.
 */
static int is_proxy_valid(const char* source_path,
                          const char* info_path)
{
    FILE* fp = fopen(info_path, "r");
    if (!fp) {
        return 0;  /* no info file — invalid */
    }

    long stored_mtime, stored_size;
    if (fscanf(fp, "%ld %ld", &stored_mtime, &stored_size) != 2) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    struct stat src_stat;
    if (stat(source_path, &src_stat) != 0) {
        return 0;
    }

    return (src_stat.st_mtime == stored_mtime &&
            src_stat.st_size == stored_size) ? 1 : 0;
}

/* Check if proxy file exists on disk.
 * Returns 1 if the file exists, 0 otherwise.
 */
int wedit_proxy_exists(const char* source_path,
                        char* proxy_path, size_t size)
{
    build_proxy_path(source_path, proxy_path, size);

    FILE* fp = fopen(proxy_path, "r");
    if (!fp) {
        return 0;
    }
    fclose(fp);
    return 1;
}

/* Generate a proxy clip by invoking FFmpeg via system().
 * The command scales the video to a lower resolution at reduced framerate.
 */
enum wedit_proxy_result wedit_proxy_generate(
    const char* source_path,
    char* proxy_path_out,
    size_t proxy_path_size,
    const wedit_proxy_opts_t* opts)
{
    if (!source_path || !proxy_path_out || !opts) {
        return WEDIT_PROXY_FAILED;
    }

    char proxy_path[1024];
    char info_path[1024];

    build_proxy_path(source_path, proxy_path, sizeof(proxy_path));
    build_proxy_info_path(proxy_path, info_path, sizeof(info_path));

    /* Check cache: if proxy exists and info is valid, return early */
    if (opts->overwrite == 0) {
        if (wedit_proxy_exists(source_path, proxy_path_out, proxy_path_size) &&
            is_proxy_valid(source_path, info_path)) {
            return WEDIT_PROXY_EXISTS;
        }
    }

    /* Verify source exists */
    FILE* src_fp = fopen(source_path, "rb");
    if (!src_fp) {
        return WEDIT_PROXY_FAILED;
    }
    fclose(src_fp);

    /* Build the FFmpeg command.
     * Strategy: scale to lower res, cap FPS, use fast preset for speed,
     * H.264 encoding for compatibility.
     */
    char cmd[4096];
    char scale_str[64];

    if (opts->width > 0 && opts->height > 0) {
        snprintf(scale_str, sizeof(scale_str),
                 "scale=%d:%d", opts->width, opts->height);
    } else {
        /* Auto: scale to 25% of original (source/4) */
        snprintf(scale_str, sizeof(scale_str), "scale=iw/4:ih/4");
    }

    /* Build the filter chain */
    char filter_chain[256];
    snprintf(filter_chain, sizeof(filter_chain), "%s,fps=%d",
             scale_str, opts->max_fps);

    /* Build full FFmpeg command */
    int ret = snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -i '%s'"
        " -vf \"%s\""
        " -c:v libx264 -preset %s -crf %d"
        " -an"                        /* no audio needed for preview */
        " -movflags +faststart"       /* web-optimized MP4 */
        " '%s' 2>/dev/null",
        source_path,
        filter_chain,
        opts->preset,
        opts->crf,
        proxy_path);

    if (ret >= (int)sizeof(cmd)) {
        return WEDIT_PROXY_FAILED;
    }

    /* Execute FFmpeg */
    int sys_ret = system(cmd);

    if (sys_ret != 0) {
        /* FFmpeg failed — try without the faststart flag */
        ret = snprintf(cmd, sizeof(cmd),
            "ffmpeg -y -i '%s'"
            " -vf \"%s\""
            " -c:v libx264 -preset %s -crf %d"
            " -an"
            " '%s' 2>/dev/null",
            source_path,
            filter_chain,
            opts->preset,
            opts->crf,
            proxy_path);
        sys_ret = system(cmd);
        if (sys_ret != 0) {
            return WEDIT_PROXY_FAILED;
        }
    }

    /* Verify the proxy was created */
    FILE* pf = fopen(proxy_path, "r");
    if (!pf) {
        return WEDIT_PROXY_FAILED;
    }
    fclose(pf);

    /* Write the proxy info file for cache validation */
    write_proxy_info(info_path, source_path);

    /* Copy the proxy path to the output buffer */
    strncpy(proxy_path_out, proxy_path, proxy_path_size - 1);
    proxy_path_out[proxy_path_size - 1] = '\0';

    return WEDIT_PROXY_OK;
}
