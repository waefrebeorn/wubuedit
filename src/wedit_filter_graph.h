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
 * wedit_filter_graph.h — libavfilter graph wrapper public API.
 *
 * Wraps FFmpeg's AVFilterGraph behind an opaque struct.
 * Provides a clean API for building filter chains:
 *   buffer → filter1 → filter2 → ... → buffersink
 */
#ifndef WEDIT_FILTER_GRAPH_H
#define WEDIT_FILTER_GRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* Opaque filter graph handle */
typedef struct wedit_filter_graph wedit_filter_graph_t;

/* Video frame buffer info (for the buffer source) */
typedef struct {
    int width;
    int height;
    int pixel_format;   /* AVPixelFormat enum value */
    int sample_rate;
    int64_t channel_layout;
    int sample_fmt;
    double time_base_num;
    double time_base_den;
} wedit_buffer_info_t;

/* Audio frame buffer info */
typedef struct {
    int sample_rate;
    int channel_layout;
    int sample_fmt;
    int channels;
    double time_base_num;
    double time_base_den;
} wedit_audio_buffer_info_t;

/*
 * Create a new filter graph.
 * Returns opaque handle, or NULL on failure.
 */
wedit_filter_graph_t* wedit_filter_graph_create(void);

/*
 * Destroy a filter graph. Safe with NULL.
 */
void wedit_filter_graph_destroy(wedit_filter_graph_t* graph);

/*
 * Build a filter graph from a description string.
 * e.g. "scale=1920:1080,eq=contrast=1.1,setsar=1"
 * Returns 0 on success, -1 on failure.
 * On failure, error_message is filled with a description.
 */
int wedit_filter_graph_build(wedit_filter_graph_t* graph,
                              const char* filter_desc,
                              char* error_message,
                              size_t error_size);

/*
 * Configure the filter graph for video input.
 * Adds a buffer source and a buffersink.
 * Returns 0 on success, -1 on failure.
 */
int wedit_filter_graph_config_video(wedit_filter_graph_t* graph,
                                     const wedit_buffer_info_t* info);

/*
 * Configure the filter graph for audio input.
 * Returns 0 on success, -1 on failure.
 */
int wedit_filter_graph_config_audio(wedit_filter_graph_t* graph,
                                     const wedit_audio_buffer_info_t* info);

/*
 * Run the filter graph and get the result as a single output
 * frame. Caller must free the returned buffer (or it's internal — TBD).
 *
 * In WubuEdit, this is used for testing filter chains without
 * the full encode pipeline. The real render path uses the
 * filtergraph description string passed to ffmpeg CLI or
 * libavfilter directly in the muxer.
 *
 * Returns 0 on success, -1 on failure.
 */
int wedit_filter_graph_run(wedit_filter_graph_t* graph);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_FILTER_GRAPH_H */
