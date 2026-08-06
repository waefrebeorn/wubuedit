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
 * wedit_main.c — CLI entry point for wubuedit.
 *
 * Usage:
 *   wubuedit init <project.wed>       Create empty project
 *   wubuedit render <project.wed>     Render timeline to video
 *   wubuedit info <project.wed>       Show timeline info
 *
 * The timeline model (wedit_timeline) is pure C11 with zero FFmpeg deps.
 * FFmpeg/libav provides codec/container plumbing (linked as system library).
 */
#include "wedit_timeline.h"
#include "wedit_render.h"
#include "wedit_proxy.h"
#include "wedit_edl.h"
#include "wedit_preview.h"
#ifdef WEDIT_HAS_FFMPEG
#include "wedit_demux.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char* prog)
{
    (void)prog;
    fprintf(stderr,
        "wubuedit — SLERM ground-up video editor (C11)\n"
        "Usage: wubuedit <command> [args...]\n"
        "\n"
        "Commands:\n"
        "  render <output.mp4>        Render the demo timeline to video\n"
        "  info                       Show timeline info\n"
        "  preview <n_frames>         Preview N frames via GPU renderer\n"
        "  proxy <source.mp4>         Generate low-res proxy for scrubbing\n"
        "  edl <output.edl>           Export timeline to CMX 3600 EDL\n"
        "  help                       Show this help\n"
        "\n"
        "The timeline model is pure C11. FFmpeg/libav provides codec muxing.\n"
        "This is a SLERM reimplementation — no forks, all original code.\n"
    );
}

/* Build a demo timeline that demonstrates the Cleveland workflow */
static wedit_timeline_t* build_demo_timeline(void)
{
    wedit_timeline_t* tl = wedit_timeline_create(2, 30.0);
    if (!tl) {
        return NULL;
    }

    /* Track 0 = video clips (Cleveland-style classified footage) */
    wedit_clip_spec_t vclip = {
        .track_index = 0,
        .start_time  = 0.0,
        .duration    = 5.0,
        .source_path = "/home/wubu/cleveland_video/tracks/track08/clips/m_0000.mp4",
        .source_in   = 0.0,
        .source_out  = 5.0,
        .transition  = WEDIT_TRANSITION_FADE,
        .transition_duration = 0.5,
    };
    wedit_timeline_add_clip(tl, &vclip);

    vclip.start_time = 5.0;
    vclip.transition = WEDIT_TRANSITION_NONE;
    vclip.transition_duration = 0.0;
    wedit_timeline_add_clip(tl, &vclip);

    /* Track 1 = audio (song.ogg) */
    wedit_clip_spec_t aclip = {
        .track_index = 1,
        .start_time  = 0.0,
        .duration    = 10.0,
        .source_path = "/home/wubu/cleveland_video/tracks/track08/song.ogg",
        .source_in   = 0.0,
        .source_out  = 10.0,
        .transition  = WEDIT_TRANSITION_NONE,
        .transition_duration = 0.0,
    };
    wedit_timeline_add_clip(tl, &aclip);

    return tl;
}

int main(int argc, char** argv)
{
    /* Initialize FFmpeg libraries */
#ifdef WEDIT_HAS_FFMPEG
    wedit_media_init();
#endif

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0) {
        usage(argv[0]);
        return 0;
    }

    if (strcmp(cmd, "info") == 0) {
        wedit_timeline_t* tl = build_demo_timeline();
        if (!tl) {
            fprintf(stderr, "Failed to create timeline\n");
            return 1;
        }
        printf("WubuEdit Timeline Info:\n");
        printf("  Tracks: %d\n", wedit_timeline_track_count(tl));
        printf("  FPS: %.2f\n", wedit_timeline_fps(tl));
        printf("  Duration: %.2f seconds\n", wedit_timeline_duration(tl));
        printf("  Video clips (track 0): %d\n",
               wedit_timeline_clip_count(tl, 0));
        printf("  Audio clips (track 1): %d\n",
               wedit_timeline_clip_count(tl, 1));
        wedit_timeline_destroy(tl);
        return 0;
    }

    if (strcmp(cmd, "render") == 0) {
        const char* output = argc > 2 ? argv[2] : "output.mp4";
        wedit_timeline_t* tl = build_demo_timeline();
        if (!tl) {
            fprintf(stderr, "Failed to create timeline\n");
            return 1;
        }

        wedit_render_opts_t opts;
        wedit_render_default_opts(&opts);
        opts.width = 1280;
        opts.height = 720;
        opts.fps = 30.0;
        opts.crf = 23;
        opts.preset = "fast";
        opts.gpu_encode = 1;  /* Use NVIDIA NVENC if available */

        printf("Rendering timeline to %s...\n", output);
        int ret = wedit_render_timeline(tl, output, &opts);
        if (ret == 0) {
            printf("Render complete: %s\n", output);
        } else {
            fprintf(stderr, "Render failed (code %d)\n", ret);
        }

        wedit_timeline_destroy(tl);
        return ret;
    }

    if (strcmp(cmd, "proxy") == 0) {
        const char* source = argc > 2 ? argv[2] : NULL;
        if (!source) {
            fprintf(stderr, "Usage: wubuedit proxy <source.mp4>\n");
            return 1;
        }
        wedit_proxy_opts_t opts;
        wedit_proxy_default_opts(&opts);
        char proxy_path[1024];
        enum wedit_proxy_result result = wedit_proxy_generate(
            source, proxy_path, sizeof(proxy_path), &opts);
        if (result == WEDIT_PROXY_OK) {
            printf("Proxy generated: %s\n", proxy_path);
            return 0;
        } else if (result == WEDIT_PROXY_EXISTS) {
            printf("Proxy already exists: %s\n", proxy_path);
            return 0;
        } else {
            fprintf(stderr, "Failed to generate proxy for %s (code %d)\n",
                    source, (int)result);
            return 1;
        }
    }

    if (strcmp(cmd, "preview") == 0) {
        int n_frames = argc > 2 ? atoi(argv[2]) : 60;
        wedit_timeline_t* tl = build_demo_timeline();
        if (!tl) {
            fprintf(stderr, "Failed to create timeline\n");
            return 1;
        }
        wedit_render_opts_t opts;
        wedit_render_default_opts(&opts);
        opts.width = 1280;
        opts.height = 720;
        opts.fps = 30.0;

        wedit_preview_t* pv = wedit_preview_create(tl, &opts);
        if (!pv) {
            fprintf(stderr, "Failed to create preview\n");
            wedit_timeline_destroy(tl);
            return 1;
        }

        printf("Previewing %d frames...\n", n_frames);
        for (int i = 0; i < n_frames; i++) {
            double t = (double)i / opts.fps;
            if (wedit_preview_render_frame(pv, t) != 0) {
                fprintf(stderr, "Preview frame %d failed\n", i);
                wedit_preview_destroy(pv);
                wedit_timeline_destroy(tl);
                return 1;
            }
        }
        printf("Preview complete: %d frames rendered\n", n_frames);

        wedit_preview_destroy(pv);
        wedit_timeline_destroy(tl);
        return 0;
    }

    if (strcmp(cmd, "edl") == 0) {
        const char* output = argc > 2 ? argv[2] : "timeline.edl";
        wedit_timeline_t* tl = build_demo_timeline();
        if (!tl) {
            fprintf(stderr, "Failed to create timeline\n");
            return 1;
        }
        int ret = wedit_edl_export(tl, output, 0, 1, "Cleveland Demo");
        if (ret == 0) {
            printf("EDL exported: %s\n", output);
        } else {
            fprintf(stderr, "Failed to export EDL\n");
        }
        wedit_timeline_destroy(tl);
        return ret == 0 ? 0 : 1;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    usage(argv[0]);
    return 1;
}
