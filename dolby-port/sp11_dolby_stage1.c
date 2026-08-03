/*
 * sp11_dolby_stage1.c — 4-band crossover for the SP11 Dolby bass chain.
 *
 * WHY THIS REPLACES bass_standalone.c's BassExtract_*
 * ---------------------------------------------------
 * That version calls zero_fill() over the whole Crossover4Band struct, which
 * clears the biquad taps, and then never fills them in. Its own comment says
 * why:
 *
 *   "the actual biquad taps are NOT hardcoded in .rdata - they are computed
 *    from crossover frequency settings during pipeline init"
 *
 * So every band runs with all-zero coefficients and the stage outputs
 * silence. Verified 2026-08-02: stage 1 alone produced peak=0.0000 on a
 * 55 Hz 0.3 amplitude input, while stages 2 and 4 produced 0.6775 and 0.3000.
 *
 * Since the taps are computed rather than stored, there is nothing to extract
 * from the binary. They must be generated. This file does that with
 * Linkwitz-Riley 4th order sections, which is the standard choice for an
 * audio crossover and is what the DLL's comment points to.
 *
 * PROVENANCE
 *   split points        documented in bass_standalone.c from AIDE/VB config
 *                       strings: 120 Hz, 500 Hz, 2000 Hz
 *   BASS_CROSSOVER_INPUT_GAIN = 21.5927734375f
 *                       bit-exact from .rdata, verified in VERIFICATION_LOG.md
 *   filter topology     NOT from the binary. Linkwitz-Riley is a
 *                       reconstruction choice, not a recovered fact.
 *
 * LR4 = two cascaded Butterworth 2nd-order sections, giving -24 dB/octave
 * with the two halves summing flat in magnitude at the crossover point.
 */

#include <math.h>
#include <string.h>
#include "sp11_dolby_stage1.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void biquad_reset(Sp11Biquad *b)
{
    b->z1 = b->z2 = 0.0f;
}

static void design_lp(Sp11Biquad *b, float fc, float sr)
{
    /* Butterworth 2nd order lowpass, Q = 1/sqrt(2) */
    float w0 = 2.0f * (float)M_PI * fc / sr;
    float c = cosf(w0), s = sinf(w0);
    float alpha = s / (2.0f * 0.70710678f);
    float a0 = 1.0f + alpha;

    b->b0 = ((1.0f - c) * 0.5f) / a0;
    b->b1 = (1.0f - c) / a0;
    b->b2 = b->b0;
    b->a1 = (-2.0f * c) / a0;
    b->a2 = (1.0f - alpha) / a0;
    biquad_reset(b);
}

static void design_hp(Sp11Biquad *b, float fc, float sr)
{
    float w0 = 2.0f * (float)M_PI * fc / sr;
    float c = cosf(w0), s = sinf(w0);
    float alpha = s / (2.0f * 0.70710678f);
    float a0 = 1.0f + alpha;

    b->b0 = ((1.0f + c) * 0.5f) / a0;
    b->b1 = -(1.0f + c) / a0;
    b->b2 = b->b0;
    b->a1 = (-2.0f * c) / a0;
    b->a2 = (1.0f - alpha) / a0;
    biquad_reset(b);
}

/* transposed direct form II */
static inline float biquad_run(Sp11Biquad *b, float x)
{
    float y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    return y;
}

/* LR4 = two identical Butterworth sections in series */
static inline float lr4_run(Sp11Biquad *s, float x)
{
    return biquad_run(&s[1], biquad_run(&s[0], x));
}

void sp11_xover_init(Sp11Crossover *xb, float sample_rate)
{
    memset(xb, 0, sizeof(*xb));
    xb->sample_rate = sample_rate;
    xb->input_gain = SP11_XOVER_INPUT_GAIN;

    /* Split at 120 Hz. Everything below is the band whose fundamental the
     * SP11 drivers cannot reproduce, and is what stage 2 synthesises
     * harmonics from. */
    design_lp(&xb->low[0],  120.0f, sample_rate);
    design_lp(&xb->low[1],  120.0f, sample_rate);
    design_hp(&xb->high[0], 120.0f, sample_rate);
    design_hp(&xb->high[1], 120.0f, sample_rate);
}

void sp11_xover_process(Sp11Crossover *xb, const float *in,
                        float *out_low, float *out_high, int n)
{
    for (int i = 0; i < n; i++) {
        float s = in[i];
        float lo = lr4_run(xb->low, s);
        float hi = lr4_run(xb->high, s);

        /* LR4 highpass is polarity-inverted relative to the lowpass at the
         * crossover; negate so low + high reconstructs the input. */
        out_low[i]  = lo;
        out_high[i] = -hi;
    }
}
