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
 * wedit_audio.c — libswresample audio resampling implementation.
 *
 * Clean-room: uses libswresample as system library (audio oracle),
 * not copied source. Resamples audio frames between sample rates/formats.
 */
#include "wedit_audio.h"
#include "wedit_internal.h"

#ifdef WEDIT_HAS_FFMPEG

#include <stdlib.h>
#include <string.h>

struct wedit_resampler {
    SwrContext* swr_ctx;
};

wedit_resampler_t* wedit_resampler_create(
    int in_sample_rate, int in_channels, int in_sample_fmt,
    int out_sample_rate, int out_channels, int out_sample_fmt)
{
    struct wedit_resampler* r = calloc(1, sizeof(*r));
    if (!r) return NULL;

    r->swr_ctx = swr_alloc();
    if (!r->swr_ctx) {
        free(r);
        return NULL;
    }

    /* Set options */
    av_opt_set_int(r->swr_ctx, "in_sample_rate", in_sample_rate, 0);
    av_opt_set_int(r->swr_ctx, "out_sample_rate", out_sample_rate, 0);
    av_opt_set_int(r->swr_ctx, "in_channels", in_channels, 0);
    av_opt_set_int(r->swr_ctx, "out_channels", out_channels, 0);
    av_opt_set_int(r->swr_ctx, "in_sample_fmt", in_sample_fmt, 0);
    av_opt_set_int(r->swr_ctx, "out_sample_fmt", out_sample_fmt, 0);

    /* Set channel layouts */
    AVChannelLayout in_layout;
    av_channel_layout_default(&in_layout, in_channels);
    av_opt_set_chlayout(r->swr_ctx, "in_chlayout", &in_layout, 0);

    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, out_channels);
    av_opt_set_chlayout(r->swr_ctx, "out_chlayout", &out_layout, 0);

    if (swr_init(r->swr_ctx) < 0) {
        swr_free(&r->swr_ctx);
        free(r);
        return NULL;
    }

    return r;
}

int wedit_resampler_process(struct wedit_resampler* r,
    const uint8_t* const in_data[], int in_samples,
    uint8_t* const out_data[], int out_samples)
{
    if (!r || !r->swr_ctx) return -1;
    /* swr_convert: out = uint8_t**, in = const uint8_t** */
    uint8_t** out_ptrs = (uint8_t**)out_data;
    const uint8_t** in_ptrs = (const uint8_t**)in_data;
    return swr_convert(r->swr_ctx, out_ptrs, out_samples, in_ptrs, in_samples);
}

int wedit_resampler_flush(struct wedit_resampler* r,
    uint8_t* const out_data[], int out_samples)
{
    if (!r || !r->swr_ctx) return -1;
    uint8_t** out_ptrs = (uint8_t**)out_data;
    return swr_convert(r->swr_ctx, out_ptrs, out_samples, NULL, 0);
}

void wedit_resampler_destroy(struct wedit_resampler* r)
{
    if (!r) return;
    if (r->swr_ctx) swr_free(&r->swr_ctx);
    free(r);
}

#else /* !WEDIT_HAS_FFMPEG */

wedit_resampler_t* wedit_resampler_create(
    int in_sr, int in_ch, int in_fmt,
    int out_sr, int out_ch, int out_fmt)
{
    (void)in_sr; (void)in_ch; (void)in_fmt;
    (void)out_sr; (void)out_ch; (void)out_fmt;
    return NULL; /* FFmpeg required for audio resampling */
}
int wedit_resampler_process(wedit_resampler_t* r,
    uint8_t* const in[], int ins, uint8_t* const out[], int outs)
{ (void)r; (void)in; (void)ins; (void)out; (void)outs; return -1; }
int wedit_resampler_flush(wedit_resampler_t* r, uint8_t* const out[], int outs)
{ (void)r; (void)out; (void)outs; return -1; }
void wedit_resampler_destroy(wedit_resampler_t* r) { (void)r; }

#endif /* WEDIT_HAS_FFMPEG */