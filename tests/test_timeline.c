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
 * test_timeline.c — Unit tests for the WubuEdit timeline model.
 *
 * Pure C11, asserts correctness with simple printf-based reporting.
 * Tests cover: create/destroy, add/remove/move clips, duration,
 * clip-at-time, sorted clips, render command generation, transitions.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "wedit_timeline.h"
#include "wedit_demux.h"
#include "wedit_project.h"
#include "wedit_effects.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { tests_run++; printf("  [RUN] %s\n", name); } while(0)
#define PASS() do { tests_passed++; printf("  [PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("  [FAIL] %s\n", msg); } while(0)

/* ── Test: Create and destroy ──────────────────────────────────────────── */

static void test_create_destroy(void)
{
    TEST("create_destroy");
    wedit_timeline_t* tl = wedit_timeline_create(2, 30.0);
    if (!tl) { FAIL("create returned NULL"); return; }

    if (wedit_timeline_track_count(tl) != 2) {
        FAIL("track_count != 2");
        wedit_timeline_destroy(tl);
        return;
    }
    if (wedit_timeline_fps(tl) != 30.0) {
        FAIL("fps != 30.0");
        wedit_timeline_destroy(tl);
        return;
    }
    if (wedit_timeline_duration(tl) != 0.0) {
        FAIL("duration != 0.0 for empty timeline");
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Test: Invalid parameters ──────────────────────────────────────────── */

static void test_create_invalid(void)
{
    TEST("create_invalid");
    if (wedit_timeline_create(0, 30.0) != NULL) {
        FAIL("should reject 0 tracks");
        return;
    }
    if (wedit_timeline_create(2, 0.0) != NULL) {
        FAIL("should reject 0.0 fps");
        return;
    }
    if (wedit_timeline_create(-1, 30.0) != NULL) {
        FAIL("should reject negative tracks");
        return;
    }
    PASS();
}

/* ── Test: Add and remove clips ────────────────────────────────────────── */

static void test_add_remove_clip(void)
{
    TEST("add_remove_clip");
    wedit_timeline_t* tl = wedit_timeline_create(2, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    wedit_clip_spec_t clip = {
        .track_index = 0,
        .start_time = 0.0,
        .duration = 5.0,
        .source_path = "/tmp/clip1.mp4",
        .source_in = 0.0,
        .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE,
        .transition_duration = 0.0,
    };
    int id1 = wedit_timeline_add_clip(tl, &clip);
    if (id1 < 0) { FAIL("add_clip returned -1"); wedit_timeline_destroy(tl); return; }

    if (wedit_timeline_clip_count(tl, 0) != 1) {
        FAIL("clip_count != 1");
        wedit_timeline_destroy(tl);
        return;
    }

    if (wedit_timeline_remove_clip(tl, id1) != 0) {
        FAIL("remove_clip returned -1");
        wedit_timeline_destroy(tl);
        return;
    }

    if (wedit_timeline_clip_count(tl, 0) != 0) {
        FAIL("clip_count != 0 after remove");
        wedit_timeline_destroy(tl);
        return;
    }

    /* Remove non-existent clip */
    if (wedit_timeline_remove_clip(tl, 999) != -1) {
        FAIL("should reject non-existent clip_id");
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Test: Move clip ───────────────────────────────────────────────────── */

static void test_move_clip(void)
{
    TEST("move_clip");
    wedit_timeline_t* tl = wedit_timeline_create(2, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    wedit_clip_spec_t clip = {
        .track_index = 0,
        .start_time = 0.0,
        .duration = 5.0,
        .source_path = "/tmp/clip1.mp4",
        .source_in = 0.0,
        .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE,
        .transition_duration = 0.0,
    };
    int id = wedit_timeline_add_clip(tl, &clip);
    if (id < 0) { FAIL("add failed"); wedit_timeline_destroy(tl); return; }

    if (wedit_timeline_move_clip(tl, id, 10.0) != 0) {
        FAIL("move_clip returned -1");
        wedit_timeline_destroy(tl);
        return;
    }

    /* Verify duration reflects new position + duration */
    if (wedit_timeline_duration(tl) != 15.0) {
        FAIL("duration != 15.0 after move to 10.0");
        wedit_timeline_destroy(tl);
        return;
    }

    /* Move non-existent clip */
    if (wedit_timeline_move_clip(tl, 999, 5.0) != -1) {
        FAIL("should reject non-existent clip_id for move");
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Test: Duration calculation ────────────────────────────────────────── */

static void test_duration(void)
{
    TEST("duration");
    wedit_timeline_t* tl = wedit_timeline_create(3, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    /* Add clips to different tracks at different positions */
    wedit_clip_spec_t c1 = {
        .track_index = 0, .start_time = 0.0, .duration = 5.0,
        .source_path = "/tmp/a.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };
    wedit_clip_spec_t c2 = {
        .track_index = 1, .start_time = 2.0, .duration = 8.0,
        .source_path = "/tmp/b.mp4", .source_in = 0.0, .source_out = 8.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };
    wedit_clip_spec_t c3 = {
        .track_index = 2, .start_time = 10.0, .duration = 3.0,
        .source_path = "/tmp/c.mp4", .source_in = 0.0, .source_out = 3.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };

    wedit_timeline_add_clip(tl, &c1);
    wedit_timeline_add_clip(tl, &c2);
    wedit_timeline_add_clip(tl, &c3);

    /* Track 0: 0.0 + 5.0 = 5.0; Track 1: 2.0 + 8.0 = 10.0; Track 2: 10.0 + 3.0 = 13.0
     * Timeline duration = max = 13.0 */
    double dur = wedit_timeline_duration(tl);
    if (dur != 13.0) {
        FAIL("duration != 13.0");
        printf("    got: %f\n", dur);
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Test: Clip at time ────────────────────────────────────────────────── */

static void test_clip_at_time(void)
{
    TEST("clip_at_time");
    wedit_timeline_t* tl = wedit_timeline_create(1, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    wedit_clip_spec_t c1 = {
        .track_index = 0, .start_time = 0.0, .duration = 5.0,
        .source_path = "/tmp/a.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };
    wedit_clip_spec_t c2 = {
        .track_index = 0, .start_time = 10.0, .duration = 5.0,
        .source_path = "/tmp/b.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };

    int id1 = wedit_timeline_add_clip(tl, &c1);
    int id2 = wedit_timeline_add_clip(tl, &c2);

    /* At t=2.0, should find c1 */
    int found = wedit_timeline_clip_at_time(tl, 0, 2.0);
    if (found != id1) {
        FAIL("clip_at_time(2.0) should return id1");
        wedit_timeline_destroy(tl);
        return;
    }

    /* At t=7.0, should find nothing (gap between clips) */
    found = wedit_timeline_clip_at_time(tl, 0, 7.0);
    if (found != -1) {
        FAIL("clip_at_time(7.0) should return -1 (gap)");
        wedit_timeline_destroy(tl);
        return;
    }

    /* At t=12.0, should find c2 */
    found = wedit_timeline_clip_at_time(tl, 0, 12.0);
    if (found != id2) {
        FAIL("clip_at_time(12.0) should return id2");
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Test: Clips on track sorted ───────────────────────────────────────── */

static void test_clips_on_track_sorted(void)
{
    TEST("clips_on_track_sorted");
    wedit_timeline_t* tl = wedit_timeline_create(1, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    /* Add clips out of order — should still come back sorted */
    wedit_clip_spec_t c1 = {
        .track_index = 0, .start_time = 10.0, .duration = 5.0,
        .source_path = "/tmp/late.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };
    wedit_clip_spec_t c2 = {
        .track_index = 0, .start_time = 0.0, .duration = 5.0,
        .source_path = "/tmp/early.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };
    wedit_clip_spec_t c3 = {
        .track_index = 0, .start_time = 5.0, .duration = 5.0,
        .source_path = "/tmp/middle.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };

    wedit_timeline_add_clip(tl, &c1);
    wedit_timeline_add_clip(tl, &c2);
    wedit_timeline_add_clip(tl, &c3);

    wedit_clip_spec_t clips[3];
    int count = wedit_timeline_clips_on_track(tl, 0, clips, 3);
    if (count != 3) {
        FAIL("clips_on_track returned wrong count");
        wedit_timeline_destroy(tl);
        return;
    }

    /* Should be sorted by start_time: 0.0, 5.0, 10.0 */
    if (clips[0].start_time != 0.0 ||
        clips[1].start_time != 5.0 ||
        clips[2].start_time != 10.0) {
        FAIL("clips not sorted by start_time");
        printf("    got: %f, %f, %f\n",
               clips[0].start_time, clips[1].start_time, clips[2].start_time);
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Test: Build render command (with transitions) ─────────────────────── */

static void test_build_render_command_with_xfade(void)
{
    TEST("build_render_command_with_xfade");
    wedit_timeline_t* tl = wedit_timeline_create(2, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    /* Two video clips with fade transition between them */
    wedit_clip_spec_t clip1 = {
        .track_index = 0, .start_time = 0.0, .duration = 5.0,
        .source_path = "/tmp/clip1.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_FADE, .transition_duration = 0.5,
    };
    wedit_clip_spec_t clip2 = {
        .track_index = 0, .start_time = 5.0, .duration = 5.0,
        .source_path = "/tmp/clip2.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };

    wedit_timeline_add_clip(tl, &clip1);
    wedit_timeline_add_clip(tl, &clip2);

    /* Audio clip */
    wedit_clip_spec_t aclip = {
        .track_index = 1, .start_time = 0.0, .duration = 10.0,
        .source_path = "/tmp/song.ogg", .source_in = 0.0, .source_out = 10.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };
    wedit_timeline_add_clip(tl, &aclip);

    char* cmd = wedit_timeline_build_render_command(tl, 0, 1, "/tmp/out.mp4", 720, 1280);
    if (!cmd) { FAIL("build_render_command returned NULL"); wedit_timeline_destroy(tl); return; }

    /* Verify the command contains expected components */
    if (strstr(cmd, "ffmpeg") == NULL) {
        FAIL("no ffmpeg in command");
        free(cmd); wedit_timeline_destroy(tl);
        return;
    }
    if (strstr(cmd, "/tmp/out.mp4") == NULL) {
        FAIL("no output path");
        free(cmd); wedit_timeline_destroy(tl);
        return;
    }
    if (strstr(cmd, "libx264") == NULL) {
        FAIL("no libx264");
        free(cmd); wedit_timeline_destroy(tl);
        return;
    }
    /* With transitions, xfade should be in the filter_complex */
    if (strstr(cmd, "xfade") == NULL) {
        FAIL("no xfade transition in filter_complex");
        printf("    cmd: %s\n", cmd);
        free(cmd); wedit_timeline_destroy(tl);
        return;
    }
    /* Should also have format=yuv420p for xfade output */
    if (strstr(cmd, "format=yuv420p") == NULL) {
        FAIL("no format=yuv420p for xfade");
        free(cmd); wedit_timeline_destroy(tl);
        return;
    }

    free(cmd);
    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Test: Build render command (concat, no transitions) ───────────────── */

static void test_build_render_command_concat(void)
{
    TEST("build_render_command_concat");
    wedit_timeline_t* tl = wedit_timeline_create(2, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    /* Two video clips WITHOUT transitions */
    wedit_clip_spec_t clip1 = {
        .track_index = 0, .start_time = 0.0, .duration = 5.0,
        .source_path = "/tmp/clip1.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };
    wedit_clip_spec_t clip2 = {
        .track_index = 0, .start_time = 5.0, .duration = 5.0,
        .source_path = "/tmp/clip2.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };

    wedit_timeline_add_clip(tl, &clip1);
    wedit_timeline_add_clip(tl, &clip2);

    char* cmd = wedit_timeline_build_render_command(tl, 0, -1, "/tmp/out.mp4", 720, 1280);
    if (!cmd) { FAIL("build_render_command returned NULL"); wedit_timeline_destroy(tl); return; }

    /* Without transitions, should use concat instead of xfade */
    if (strstr(cmd, "concat") == NULL) {
        FAIL("no concat in filter_complex");
        printf("    cmd: %s\n", cmd);
        free(cmd); wedit_timeline_destroy(tl);
        return;
    }
    /* Should NOT have xfade when no transitions */
    if (strstr(cmd, "xfade") != NULL) {
        FAIL("xfade should not appear when no transitions");
        free(cmd); wedit_timeline_destroy(tl);
        return;
    }

    free(cmd);
    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Test: Invalid add ─────────────────────────────────────────────────── */

static void test_invalid_add(void)
{
    TEST("invalid_add");
    wedit_timeline_t* tl = wedit_timeline_create(1, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    /* Invalid track_index */
    wedit_clip_spec_t clip = {
        .track_index = 5, .start_time = 0.0, .duration = 5.0,
        .source_path = "/tmp/test.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_NONE, .transition_duration = 0.0,
    };
    if (wedit_timeline_add_clip(tl, &clip) != -1) {
        FAIL("should reject invalid track_index");
        wedit_timeline_destroy(tl);
        return;
    }

    /* Zero duration */
    clip.track_index = 0;
    clip.duration = 0.0;
    if (wedit_timeline_add_clip(tl, &clip) != -1) {
        FAIL("should reject zero duration");
        wedit_timeline_destroy(tl);
        return;
    }

    /* Empty source path */
    clip.duration = 5.0;
    clip.source_path[0] = '\0';
    if (wedit_timeline_add_clip(tl, &clip) != -1) {
        FAIL("should reject empty source_path");
        wedit_timeline_destroy(tl);
        return;
    }

    wedit_timeline_destroy(tl);
    PASS();
}

/* ── Main ────────────────────────────────────────────────────────────────── */


static void test_project_save_load(void)
{
    TEST("project_save_load");
    wedit_timeline_t* tl = wedit_timeline_create(2, 30.0);
    if (!tl) { FAIL("create failed"); return; }

    wedit_clip_spec_t clip = {
        .track_index = 0, .start_time = 0.0, .duration = 5.0,
        .source_path = "/tmp/test_clip.mp4", .source_in = 0.0, .source_out = 5.0,
        .transition = WEDIT_TRANSITION_FADE, .transition_duration = 0.5,
    };
    wedit_timeline_add_clip(tl, &clip);

    /* Save project — wrap timeline in a project */
    wedit_project_t* proj = wedit_project_from_timeline(tl);
    if (!proj) {
        FAIL("project_from_timeline failed");
        wedit_timeline_destroy(tl);
        return;
    }
    if (wedit_project_save(proj, "/tmp/test_project.wed") != 0) {
        FAIL("project_save failed");
        wedit_project_destroy(proj);
        return;
    }
    /* Note: proj takes ownership of tl, so don't destroy tl separately */

    /* Load project */
    wedit_timeline_t* loaded = wedit_project_load("/tmp/test_project.wed");
    if (!loaded) {
        FAIL("project_load returned NULL");
        wedit_timeline_destroy(tl);
        return;
    }

    /* Verify loaded timeline matches */
    if (wedit_timeline_track_count(loaded) != 2) {
        FAIL("loaded track_count != 2");
        wedit_project_destroy(proj);
        wedit_timeline_destroy(loaded);
        return;
    }
    if (wedit_timeline_clip_count(loaded, 0) != 1) {
        FAIL("loaded clip_count != 1");
        wedit_project_destroy(proj);
        wedit_timeline_destroy(loaded);
        return;
    }

    wedit_clip_spec_t clips[1];
    wedit_timeline_clips_on_track(loaded, 0, clips, 1);
    if (clips[0].transition != WEDIT_TRANSITION_FADE ||
        clips[0].transition_duration != 0.5) {
        FAIL("transition fields not preserved");
        printf("    transition=%d, dur=%f\n",
               clips[0].transition, clips[0].transition_duration);
        wedit_project_destroy(proj);
        wedit_timeline_destroy(loaded);
        return;
    }

    wedit_project_destroy(proj);  /* destroys timeline inside */
    wedit_timeline_destroy(loaded);

    /* Clean up */
    remove("/tmp/test_project.wed");
    PASS();
}


static void test_effects_pipeline(void)
{
    TEST("effects_pipeline");

    wedit_effect_list_t list;
    if (wedit_effects_init(&list) != 0) {
        FAIL("effects_init failed");
        return;
    }

    /* Add a color grade */
    wedit_effects_add_color_grade(&list, 1.2, 1.1, 0.95, 0.0);

    /* Add a LUT3D */
    wedit_effects_add_lut3d(&list, "/tmp/grading.cube");

    /* Add text overlay */
    wedit_effects_add_text(&list, "Hello World", "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "white", 48, 0.5, 0.8, 2.0, 8.0);

    /* Add video overlay (picture-in-picture) */
    wedit_effects_add_overlay(&list, "/tmp/pip.mp4", 0.1, 0.1, 0.3, 0.3, 0.0, 5.0);

    /* Add audio ducking */
    wedit_effects_add_audio_duck(&list, 0.3, 1.0, 2.0);

    /* Build video filter */
    char* vfilter = wedit_effects_build_video_filter(&list, NULL);
    if (!vfilter) {
        FAIL("video filter build returned NULL");
        wedit_effects_free(&list);
        return;
    }

    /* Check that eq filter is present */
    if (strstr(vfilter, "eq=") == NULL) {
        FAIL("video filter missing eq (color grade)");
        printf("    filter: %s\n", vfilter);
        free(vfilter);
        wedit_effects_free(&list);
        return;
    }

    /* Check that lut3d is present */
    if (strstr(vfilter, "lut3d=") == NULL) {
        FAIL("video filter missing lut3d");
        free(vfilter);
        wedit_effects_free(&list);
        return;
    }

    /* Check that drawtext is present */
    if (strstr(vfilter, "drawtext=") == NULL) {
        FAIL("video filter missing drawtext");
        free(vfilter);
        wedit_effects_free(&list);
        return;
    }

    /* Check that overlay is present */
    if (strstr(vfilter, "overlay=") == NULL) {
        FAIL("video filter missing overlay");
        free(vfilter);
        wedit_effects_free(&list);
        return;
    }

    free(vfilter);

    /* Build audio filter */
    char* afilter = wedit_effects_build_audio_filter(&list);
    if (!afilter) {
        FAIL("audio filter build returned NULL");
        wedit_effects_free(&list);
        return;
    }

    /* Check that volume ducking is present */
    if (strstr(afilter, "volume=") == NULL) {
        FAIL("audio filter missing volume ducking");
        printf("    filter: %s\n", afilter);
        free(afilter);
        wedit_effects_free(&list);
        return;
    }

    free(afilter);
    wedit_effects_free(&list);
    PASS();
}

int main(void)
{
    printf("=== WubuEdit Timeline Model Tests ===\n\n");

    test_create_destroy();
    test_create_invalid();
    test_add_remove_clip();
    test_move_clip();
    test_duration();
    test_clip_at_time();
    test_clips_on_track_sorted();
    test_build_render_command_with_xfade();
    test_build_render_command_concat();
    test_invalid_add();
    test_project_save_load();
    test_effects_pipeline();

    printf("\n=== Results ===\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
