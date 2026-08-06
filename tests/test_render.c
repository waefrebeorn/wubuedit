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
 * test_render.c — Integration tests for the media-decode render pipeline.
 *
 * Tests that:
 *  1. wedit_clip_source_open correctly opens a real MP4
 *  2. wedit_clip_source_seek + get_pixels produces non-black RGBA frames
 *  3. wedit_render_frame_at composites clips onto a timeline buffer
 *  4. The render output buffer has valid dimensions and content
 *
 * Requires WEDIT_HAS_FFMPEG to be defined.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wedit_clip_source.h"
#include "wedit_timeline.h"
#include "wedit_render.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { tests_run++; printf("  [RUN] %s\n", name); } while(0)
#define PASS() do { tests_passed++; printf("  [PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("  [FAIL] %s\n", msg); } while(0)

/* Find a test media file — check common locations */
static const char* find_test_media(void)
{
    static const char* candidates[] = {
        "/home/wubu/cleveland_video/tiktok_clips/track01_clip_mid.mp4",
        "/home/wubu/cleveland_video/tiktok_clips/track08_clip_start.mp4",
        "/home/wubu/cleveland_video/tiktok_clips/track02_clip_start.mp4",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        FILE* f = fopen(candidates[i], "rb");
        if (f) {
            fclose(f);
            return candidates[i];
        }
    }
    return NULL;
}

/* ── Test: Clip source open and decode ──────────────────────────────────── */
static void test_clip_source_open(void)
{
    TEST("clip_source_open");

#ifdef WEDIT_HAS_FFMPEG
    const char* path = find_test_media();
    if (!path) {
        FAIL("no test media found");
        return;
    }

    wedit_clip_source_t* src = wedit_clip_source_open(path, 320, 180);
    if (!src) {
        FAIL("clip_source_open returned NULL");
        return;
    }

    int w = wedit_clip_source_width(src);
    int h = wedit_clip_source_height(src);
    if (w != 320 || h != 180) {
        FAIL("wrong dimensions");
        printf("    got: %dx%d, expected 320x180\n", w, h);
        wedit_clip_source_close(src);
        return;
    }

    /* Seek to 0.5s and verify we get a frame */
    if (wedit_clip_source_seek(src, 0.5) != 0) {
        FAIL("seek to 0.5 failed");
        wedit_clip_source_close(src);
        return;
    }

    const unsigned char* pixels = wedit_clip_source_get_pixels(src);
    if (!pixels) {
        FAIL("get_pixels returned NULL after seek");
        wedit_clip_source_close(src);
        return;
    }

    /* Verify the frame is not all-black (at least some pixel has content) */
    int has_content = 0;
    for (int i = 0; i < w * h * 4; i++) {
        if (pixels[i] > 10) {
            has_content = 1;
            break;
        }
    }
    if (!has_content) {
        FAIL("decoded frame is all black");
        wedit_clip_source_close(src);
        return;
    }

    /* Verify timestamp is cached */
    double ts = wedit_clip_source_timestamp(src);
    if (ts < 0.0) {
        FAIL("timestamp not set after seek");
        wedit_clip_source_close(src);
        return;
    }

    /* Cache hit: seeking to same time should be fast/no-op */
    if (wedit_clip_source_seek(src, 0.5) != 0) {
        FAIL("cache-hit seek failed");
        wedit_clip_source_close(src);
        return;
    }

    wedit_clip_source_close(src);
    PASS();
#else
    FAIL("WEDIT_HAS_FFMPEG not defined");
#endif
}

/* ── Test: Render frame with real clip on timeline ────────────────────────── */
static void test_render_frame_at_with_clip(void)
{
    TEST("render_frame_at_with_clip");

#ifdef WEDIT_HAS_FFMPEG
    const char* path = find_test_media();
    if (!path) {
        FAIL("no test media found");
        return;
    }

    /* Create a timeline with one clip */
    wedit_timeline_t* tl = wedit_timeline_create(1, 30.0);
    if (!tl) {
        FAIL("timeline create failed");
        return;
    }

    wedit_clip_spec_t clip = {
        .track_index = 0,
        .start_time = 0.0,
        .duration = 5.0,
        .source_in = 0.0,
        .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE,
        .transition_duration = 0.0,
    };
    strncpy(clip.source_path, path, 511);
    clip.source_path[511] = '\0';

    int id = wedit_timeline_add_clip(tl, &clip);
    if (id < 0) {
        FAIL("add_clip failed");
        wedit_timeline_destroy(tl);
        return;
    }

    /* Render at frame 30 (t=1.0s) */
    int w = 320, h = 180;
    unsigned char* buf = malloc((size_t)w * h * 4);
    if (!buf) {
        FAIL("malloc failed");
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_render_opts_t opts;
    wedit_render_default_opts(&opts);

    int ret = wedit_render_frame_at(tl, &opts, 1.0, buf, w, h);
    if (ret != 0) {
        FAIL("render_frame_at returned -1");
        free(buf);
        wedit_timeline_destroy(tl);
        return;
    }

    /* Check that the rendered buffer has some non-black content */
    int non_black = 0;
    for (int i = 0; i < w * h * 4; i += 4) {
        if (buf[i] > 10 || buf[i+1] > 10 || buf[i+2] > 10) {
            non_black = 1;
            break;
        }
    }
    if (!non_black) {
        FAIL("rendered frame is entirely black — no clip decoded");
        free(buf);
        wedit_timeline_destroy(tl);
        return;
    }

    /* Check that alpha channel is set (should be 255 for opaque pixels) */
    int has_opaque = 0;
    for (int i = 3; i < w * h * 4; i += 4) {
        if (buf[i] == 255) {
            has_opaque = 1;
            break;
        }
    }
    if (!has_opaque) {
        FAIL("no opaque pixels in rendered frame");
        free(buf);
        wedit_timeline_destroy(tl);
        return;
    }

    free(buf);
    wedit_timeline_destroy(tl);
    PASS();
#else
    FAIL("WEDIT_HAS_FFMPEG not defined");
#endif
}

/* ── Test: Render blank timeline (no clips) ──────────────────────────────── */
static void test_render_blank_timeline(void)
{
    TEST("render_blank_timeline");

    wedit_timeline_t* tl = wedit_timeline_create(1, 30.0);
    if (!tl) {
        FAIL("timeline create failed");
        return;
    }

    int w = 640, h = 360;
    unsigned char* buf = malloc((size_t)w * h * 4);
    if (!buf) {
        FAIL("malloc failed");
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_render_opts_t opts;
    wedit_render_default_opts(&opts);

    int ret = wedit_render_frame_at(tl, &opts, 0.0, buf, w, h);
    if (ret != 0) {
        FAIL("render_frame_at on blank timeline returned -1");
        free(buf);
        wedit_timeline_destroy(tl);
        return;
    }

    /* Blank timeline should be black with watermark */
    /* Check that most of the buffer is black (watermark is small) */
    int black_pixels = 0;
    int total_pixels = w * h;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 4;
            if (buf[idx] == 0 && buf[idx+1] == 0 && buf[idx+2] == 0) {
                black_pixels++;
            }
        }
    }

    if (black_pixels < total_pixels * 0.95) {
        FAIL("blank timeline should be mostly black");
        printf("    black: %d/%d pixels\n", black_pixels, total_pixels);
        free(buf);
        wedit_timeline_destroy(tl);
        return;
    }

    free(buf);
    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== WubuEdit Render Pipeline Tests ===\n\n");

    test_clip_source_open();
    test_render_frame_at_with_clip();
    test_render_blank_timeline();

    printf("\n=== Results ===\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
