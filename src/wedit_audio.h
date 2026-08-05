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
 * wedit_audio.h — libswresample audio resampling public API.
 * Opaque struct + minimal interface.
 */
#ifndef WEDIT_AUDIO_H
#define WEDIT_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WEDIT_HAS_FFMPEG
#include <libswresample/swresample.h>
typedef struct SwrContext wedit_swr_ctx_t;
#else
typedef void wedit_swr_ctx_t;
#endif

/* Opaque audio resampler handle */
typedef struct wedit_resampler wedit_resampler_t;

/* Create a resampler: convert from in->out sample rate, channels, format */
wedit_resampler_t* wedit_resampler_create(
    int in_sample_rate, int in_channels, int in_sample_fmt,
    int out_sample_rate, int out_channels, int out_sample_fmt);

/* Resample one audio frame. Returns 0 on success. */
int wedit_resampler_process(wedit_resampler_t* r,
    const uint8_t* const in_data[], int in_samples,
    uint8_t* const out_data[], int out_samples);

/* Flush remaining buffered samples (call at EOF). */
int wedit_resampler_flush(wedit_resampler_t* r,
    uint8_t* const out_data[], int out_samples);

void wedit_resampler_destroy(wedit_resampler_t* r);

#ifdef __cplusplus
}
#endif

#endif /* WEDIT_AUDIO_H */