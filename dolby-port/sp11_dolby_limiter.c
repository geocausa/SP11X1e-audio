/*
 * sp11_dolby_limiter.c — Dolby envelope limiter, decoded from the binary.
 *
 * FUN_180097228 in DolbyAudioProcessing.dll
 * sha256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
 *
 * This is the stage that produces the measured -0.13 dBFS ceiling on Windows.
 * It is NOT a clipper: it is a look-ahead peak limiter with dual-envelope
 * smoothing, 16-band spectral weighting, and interpolated gain application.
 *
 * sp11_dolby_chain.c currently hard-clamps at that level, which matches the
 * ceiling but not the behaviour - a clamp distorts where this smoothly
 * reduces gain. This file replaces it.
 *
 * See docs/findings/2026-08-03-dolby-leveler-regulator-re-log.md for the
 * annotated decompilation.
 */

#include <math.h>
#include <string.h>
#include "sp11_dolby_limiter.h"

/* Envelope flush threshold, DAT_180098288. Envelopes below this are zeroed
 * so a decayed limiter does not hold a denormal forever. */
#define ENV_FLOOR 1.0e-8f

void sp11_lim_init(Sp11Limiter *l, float threshold,
                   float attack, float release, float slow_release,
                   const float *weights16, const float *ramp, int ramp_taps)
{
    memset(l, 0, sizeof(*l));
    l->threshold    = threshold;
    l->attack       = attack;        /* coeffs +0x04 */
    l->release      = release;       /* coeffs +0x08 */
    l->slow_release = slow_release;  /* coeffs +0x0c */
    l->weights      = weights16;     /* coeffs +0x18, 16 taps */
    l->ramp         = ramp;          /* coeffs +0x10 */
    l->ramp_taps    = ramp_taps;     /* 4, 8 or 16 by mode */
    l->prev_gain    = 1.0f;
}

/* Step 1-2: peak detect over a sub-block, then reduce across bands.
 * The binary does this with NEON fmax/fmaxp over 64-sample groups; a plain
 * loop is numerically identical. */
static float block_peak(const float *x, int n)
{
    float p = 0.0f;
    for (int i = 0; i < n; i++) {
        float a = fabsf(x[i]);
        if (a > p)
            p = a;
    }
    return p;
}

/* Step 3: dual-envelope smoothing.
 *
 *   coef = (peak > fast) ? attack : release
 *   fast = peak + (fast - peak) * coef
 *   slow = peak + (slow - peak) * slow_release
 *   out  = max(peak, fast, slow)
 */
static float envelope(Sp11Limiter *l, float peak)
{
    float coef = (peak > l->fast_env) ? l->attack : l->release;

    l->fast_env = peak + (l->fast_env - peak) * coef;
    l->slow_env = peak + (l->slow_env - peak) * l->slow_release;

    /* the binary flushes to zero below DAT_180098288 */
    if (fabsf(l->fast_env) < ENV_FLOOR) l->fast_env = 0.0f;
    if (fabsf(l->slow_env) < ENV_FLOOR) l->slow_env = 0.0f;

    float out = peak;
    if (l->fast_env > out) out = l->fast_env;
    if (l->slow_env > out) out = l->slow_env;
    return out;
}

/* Step 4: 16-tap spectral weighting. Fully unrolled in the binary. */
static float spectral_weight(const Sp11Limiter *l, const float *bands)
{
    if (!l->weights)
        return bands[0];
    float acc = 0.0f;
    for (int i = 0; i < SP11_LIM_BANDS; i++)
        acc += bands[i] * l->weights[i];
    return acc;
}

/* Step 5: the limiter gain itself.
 *   gain = (peak <= threshold) ? 1.0 : threshold / peak
 * Unity below the ceiling, exact reciprocal above. */
static float limiter_gain(const Sp11Limiter *l, float peak)
{
    return (peak <= l->threshold) ? 1.0f : (l->threshold / peak);
}

/* The binary computes ALL sub-block gains from the untouched input first,
 * then runs a separate application pass. Interleaving the two makes each
 * block measure audio the previous block already reduced, which turns a
 * feedforward limiter into a feedback one and under-corrects: a 1.5 input
 * settled at 1.19 instead of the 0.985 ceiling. Two passes it is. */
#define SP11_LIM_MAX_BLOCKS 256

void sp11_lim_process(Sp11Limiter *l, float *const *chans, int nchan,
                      int nframes, int sub_block)
{
    if (sub_block <= 0)
        sub_block = 64;

    int nblocks = (nframes + sub_block - 1) / sub_block;
    if (nblocks > SP11_LIM_MAX_BLOCKS)
        nblocks = SP11_LIM_MAX_BLOCKS;

    float gains[SP11_LIM_MAX_BLOCKS];

    /* pass 1: detect and compute gains from the untouched input */
    for (int b = 0; b < nblocks; b++) {
        int off = b * sub_block;
        int n = nframes - off;
        if (n > sub_block) n = sub_block;

        float peak = 0.0f;
        for (int c = 0; c < nchan; c++) {
            float p = block_peak(chans[c] + off, n);
            if (p > peak) peak = p;
        }

        float bands[SP11_LIM_BANDS];
        for (int k = 0; k < SP11_LIM_BANDS; k++)
            bands[k] = peak;

        float env = envelope(l, spectral_weight(l, bands));
        gains[b] = limiter_gain(l, env);
    }

    /* pass 2: apply, ramping between consecutive gains */
    for (int b = 0; b < nblocks; b++) {
        int off = b * sub_block;
        int n = nframes - off;
        if (n > sub_block) n = sub_block;

        float d = gains[b] - l->prev_gain;
        for (int i = 0; i < n; i++) {
            float t = (l->ramp && i < l->ramp_taps)
                    ? l->ramp[i]
                    : (float)(i + 1) / (float)n;
            float g = l->prev_gain + d * t;
            for (int c = 0; c < nchan; c++)
                chans[c][off + i] *= g;
        }
        l->prev_gain = gains[b];
    }
}
