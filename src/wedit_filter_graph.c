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
 * wedit_filter_graph.c — libavfilter graph wrapper implementation.
 *
 * Uses libavfilter (system library) — not vendored source.
 * Opaque struct pattern: AVFilterGraph is hidden behind our struct.
 */
#include "wedit_filter_graph.h"
#include "wedit_internal.h"

#include <stdlib.h>
#include <string.h>

#ifdef WEDIT_HAS_FFMPEG
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#endif

struct wedit_filter_graph {
#ifdef WEDIT_HAS_FFMPEG
    AVFilterGraph* graph;
    AVFilterContext* buffersrc;
    AVFilterContext* buffersink;
    int configured;
#else
    int dummy;
#endif
};

wedit_filter_graph_t* wedit_filter_graph_create(void)
{
#ifdef WEDIT_HAS_FFMPEG
    wedit_filter_graph_t* g = calloc(1, sizeof(wedit_filter_graph_t));
    if (!g) return NULL;
    g->graph = avfilter_graph_alloc();
    if (!g->graph) {
        free(g);
        return NULL;
    }
    return g;
#else
    return NULL;
#endif
}

void wedit_filter_graph_destroy(wedit_filter_graph_t* graph)
{
    if (!graph) return;
#ifdef WEDIT_HAS_FFMPEG
    if (graph->graph) {
        avfilter_graph_free(&graph->graph);
    }
#endif
    free(graph);
}

int wedit_filter_graph_build(wedit_filter_graph_t* graph,
                              const char* filter_desc,
                              char* error_message,
                              size_t error_size)
{
    if (!graph || !filter_desc) return -1;

#ifdef WEDIT_HAS_FFMPEG
    char errbuf[256] = {0};
    int ret = avfilter_graph_parse_ptr(
        graph->graph, filter_desc,
        NULL, NULL,  /* inputs/outputs — auto-create */
        errbuf);

    if (ret < 0) {
        if (error_message && error_size > 0) {
            snprintf(error_message, error_size,
                "avfilter_graph_parse_ptr failed: %s (%s)",
                errbuf[0] ? errbuf : "unknown error", filter_desc);
        }
        return -1;
    }

    ret = avfilter_graph_config(graph->graph, NULL);
    if (ret < 0) {
        if (error_message && error_size > 0) {
            snprintf(error_message, error_size,
                "avfilter_graph_config failed for: %s", filter_desc);
        }
        return -1;
    }

    graph->configured = 1;
    return 0;
#else
    if (error_message && error_size > 0) {
        snprintf(error_message, error_size,
            "FFmpeg not available (WEDIT_HAS_FFMPEG not defined)");
    }
    return -1;
#endif
}

int wedit_filter_graph_config_video(wedit_filter_graph_t* graph,
                                     const wedit_buffer_info_t* info)
{
    if (!graph || !info) return -1;

#ifdef WEDIT_HAS_FFMPEG
    char args[256];
    snprintf(args, sizeof(args),
        "buffer_i%d=%d:video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
        /* Note: the proper buffer source args for video */
        0, 0, info->width, info->height, info->pixel_format,
        (int)info->time_base_num, (int)info->time_base_den);

    /* Use the buffer source filter properly */
    int ret = avfilter_graph_create_filter(
        &graph->buffersrc,
        avfilter_get_by_name("buffer"),
        "in",
        args,
        NULL,
        graph->graph);

    if (ret < 0) return -1;

    ret = avfilter_graph_create_filter(
        &graph->buffersink,
        avfilter_get_by_name("buffersink"),
        "out",
        NULL,
        NULL,
        graph->graph);

    if (ret < 0) return -1;

    graph->configured = 1;
    return 0;
#else
    return -1;
#endif
}

int wedit_filter_graph_config_audio(wedit_filter_graph_t* graph,
                                     const wedit_audio_buffer_info_t* info)
{
    if (!graph || !info) return -1;

#ifdef WEDIT_HAS_FFMPEG
    char args[256];
    snprintf(args, sizeof(args),
        "sample_%d:rate=%d:channels=%d:channel_layout=%.0f:time_base=%d/%d"
        ":sample_fmt=%d",
        0, info->sample_rate, info->channels,
        (double)info->channel_layout,
        (int)info->time_base_num, (int)info->time_base_den,
        info->sample_fmt);

    int ret = avfilter_graph_create_filter(
        &graph->buffersrc,
        avfilter_get_by_name("abuffer"),
        "in",
        args,
        NULL,
        graph->graph);

    if (ret < 0) return -1;

    ret = avfilter_graph_create_filter(
        &graph->buffersink,
        avfilter_get_by_name("abuffersink"),
        "out",
        NULL,
        NULL,
        graph->graph);

    if (ret < 0) return -1;

    graph->configured = 1;
    return 0;
#else
    return -1;
#endif
}

int wedit_filter_graph_run(wedit_filter_graph_t* graph)
{
    if (!graph) return -1;
#ifdef WEDIT_HAS_FFMPEG
    if (!graph->configured || !graph->graph) return -1;
    /* In a full implementation, this would:
     * 1. Push frames via av_buffersrc_add_frame()
     * 2. Pull frames via av_buffersink_get_frame()
     * 3. Process until EOF
     * For now, just validate the graph is configured.
     */
    return 0;
#else
    return -1;
#endif
}
