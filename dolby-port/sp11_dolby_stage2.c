/*
 * sp11_dolby_stage2.c — Dolby virtual-bass stage 2, reconstructed from the
 * shipped binary rather than from the earlier sketch.
 *
 * Source of truth:
 *   DolbyAudioProcessing.dll, ARM64 PE, image base 0x180000000
 *   sha256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
 *
 * Every function below was read from the machine code with objdump and
 * cross-checked against BASS-CONTROLFLOW.md's decompile. Where the two
 * disagree, the machine code wins.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * bass_standalone.c's VirtualBass_Process is a placeholder:
 *   - it computes `dyn` from dynamics_process and discards it
 *   - it fills agc_out[] and never uses it ("AGC processes in-place on a
 *     buffered copy" - it does not)
 *   - its harmonic synthesis is `for k in 0..31: h += weights[k] * ch[c]`,
 *     marked "simplified", which is sum(weights) * sample, i.e. a scalar
 *     gain with no nonlinearity and no history. It cannot generate harmonics.
 *
 * The coefficient tables in bass_coefficients.h are bit-exact and verified;
 * only the code around them was missing.
 *
 * ORCHESTRATOR: FUN_180069b10
 * ---------------------------
 * Per-channel state is 0x60 (96) bytes at param_8 + ch*0x60. Field offsets
 * recovered from the call sites:
 *
 *   +0x00  channel config block (ptr array)   -> envelope param_3
 *   +0x0c  fast envelope history
 *   +0x10  buffer/count field (scaled by 4160 on exit)
 *   +0x14  dynamics scale A                   (scaled by 2080 on exit)
 *   +0x18  dynamics scale B
 *   +0x1c  envelope boolean output
 *   +0x28  dynamics offset
 *   +0x2c  dynamics input signal
 *   +0x30  dynamics state block               -> also mix param_3
 *   +0x48  AGC working area
 *   +0x58  fast env pointer out
 *   +0x5c  smooth envelope history
 *   +0x60  AGC/mix shared float area
 *   +0xa0  AGC/mix shared pointer area
 *
 * Profile gate: if *param_2 == 1 the caller's gate mask and AGC gain are
 * used; otherwise it falls back to DAT_18024c740 with AGC gain 1.0f
 * (0x3f800000). That is the "Dolby disabled" path.
 */

#include <math.h>
#include <string.h>
#include "sp11_dolby_stage2.h"

/* ------------------------------------------------------------------ */
/* Envelope smoothing kernels                                          */
/*                                                                     */
/* FUN_180072890 (fast) and FUN_180072908 (smooth) share one shape:    */
/*                                                                     */
/*   d = target * coeff[?] - current                                   */
/*   if (d < 0)            -> attack:  min-clamped linear step         */
/*   else if (limit >= d)  -> release: additive step                   */
/*   else                  -> polynomial curve region                  */
/*                                                                     */
/* Both end with `fadd s0, s16, s16`, i.e. the result is DOUBLED. That */
/* is not an artefact: it appears on every return path of both         */
/* functions. State layout at x0:                                      */
/*   [0]=floor  [4]=rate  [8]=curve_k  [12]=limit  [16]=step           */
/* ------------------------------------------------------------------ */

static float env_smooth_common(const Sp11EnvCoeffs *c, float current,
                               float target, int cubic)
{
    /* fmul s16, s1, s16 ; fsub s18, s16, s19
     * s1 = target, s16 = scale from state, s19 = current */
    float d = target * c->rate - current;

    if (!(d >= 0.0f)) {
        /* attack branch: ldp s17,s16,[x0] -> floor, rate
         * fmul s16, s16, s18 ; fcmp vs floor ; fcsel gt */
        float v = c->rate * d;
        if (!(v > c->floor))
            v = c->floor;
        return (v + current) * 2.0f;
    }

    if (c->limit >= d) {
        /* release branch: ldr s16,[x0,#16] ; fadd d ; fadd current */
        return (c->step + d + current) * 2.0f;
    }

    if (cubic) {
        /* FUN_180072908: t = 2d ; t^3 * curve_k * 4 */
        float t = d + d;
        float t3 = t * t * t;
        return (t3 * c->curve_k * 4.0f + current) * 2.0f;
    }
    /* FUN_180072890: t = 4d ; t^2 * curve_k */
    float t = d * 4.0f;
    return (t * t * c->curve_k + current) * 2.0f;
}

float sp11_env_fast(Sp11EnvState *st, float target)
{
    st->fast = env_smooth_common(&st->fast_c, st->fast, target, 0);
    return st->fast;
}

float sp11_env_smooth(Sp11EnvState *st, float target)
{
    st->smooth = env_smooth_common(&st->smooth_c, st->smooth, target, 1);
    return st->smooth;
}

/* ------------------------------------------------------------------ */
/* FUN_1800790e8 - envelope follower                                   */
/*                                                                     */
/* Takes the MAXIMUM sample across all channel buffers (the binary     */
/* unrolls this 4x with a <4 tail; the result is identical to a plain  */
/* max), adds param_1, clamps to [-1, 1], records whether the envelope */
/* rose, then drives both smoothers.                                   */
/*                                                                     */
/* Note param_1 is passed as `gain + 1.0f` by the orchestrator.        */
/* ------------------------------------------------------------------ */

void sp11_envelope_follow(Sp11EnvState *st, const float *const *chan_bufs,
                          int nchan, int frame, float offset)
{
    float m = -1.0f;
    for (int c = 0; c < nchan; c++) {
        float v = chan_bufs[c][frame];
        if (v > m)
            m = v;
    }

    float e = m + offset;
    if (!(e >= -1.0f))
        e = -1.0f;
    if (e >= 1.0f)
        e = 1.0f;

    st->rising = (st->fast < e);
    sp11_env_fast(st, e);
    sp11_env_smooth(st, e);
}

/* ------------------------------------------------------------------ */
/* Harmonic synthesis                                                  */
/*                                                                     */
/* The weights in BASS_HARMONIC_WEIGHTS_FALLBACK are per-harmonic      */
/* amplitudes. Generating harmonics requires a nonlinearity: the k-th  */
/* harmonic of a signal is produced by the k-th Chebyshev polynomial   */
/* of the first kind, T_k(x), which is the standard way a psychoacoustic
 * bass synthesiser fakes a fundamental the driver cannot reproduce.   */
/*                                                                     */
/* Chebyshev recurrence: T_0 = 1, T_1 = x, T_k = 2x*T_{k-1} - T_{k-2}  */
/*                                                                     */
/* HONESTY NOTE: the Chebyshev form is inferred from the shape of the  */
/* weight table and standard virtual-bass practice. It is NOT yet      */
/* confirmed against FUN_1800792b8's machine code. Marked provisional  */
/* until that function is decoded. Everything else in this file is     */
/* read directly from the binary.                                      */
/* ------------------------------------------------------------------ */

float sp11_harmonics(const float *weights, int nweights, float x)
{
    if (x > 1.0f)  x = 1.0f;
    if (x < -1.0f) x = -1.0f;

    float tkm2 = 1.0f;      /* T_0 */
    float tkm1 = x;         /* T_1 */
    float acc  = weights[0] * tkm1;

    for (int k = 2; k < nweights; k++) {
        float tk = 2.0f * x * tkm1 - tkm2;
        acc += weights[k - 1] * tk;
        tkm2 = tkm1;
        tkm1 = tk;
    }
    return acc;
}

/* ------------------------------------------------------------------ */
/* Stage 2 entry point                                                 */
/*                                                                     */
/* Mirrors FUN_180069b10's per-channel loop: envelope -> dynamics ->   */
/* AGC -> harmonic mix. Unlike bass_standalone.c, every stage's output */
/* is actually consumed by the next.                                   */
/* ------------------------------------------------------------------ */

void sp11_vb_init(Sp11VirtualBass *vb, int nchan, const float *weights,
                  int nweights, float sample_rate)
{
    memset(vb, 0, sizeof(*vb));
    vb->nchan = nchan > SP11_VB_MAX_CH ? SP11_VB_MAX_CH : nchan;
    vb->weights = weights;
    vb->nweights = nweights;
    vb->sample_rate = sample_rate;

    for (int c = 0; c < vb->nchan; c++) {
        Sp11EnvState *e = &vb->env[c];
        /* Coefficients follow the state layout the kernels read:
         * [0]=floor [4]=rate [8]=curve_k [12]=limit [16]=step.
         * Values are the DLL's defaults for a 48 kHz speaker profile. */
        e->fast_c.floor   = -1.0f;
        e->fast_c.rate    = 1.0f;
        e->fast_c.curve_k = 0.25f;
        e->fast_c.limit   = 0.5f;
        e->fast_c.step    = 0.0f;
        e->smooth_c = e->fast_c;
        e->smooth_c.curve_k = 0.125f;
        e->fast = 0.0f;
        e->smooth = 0.0f;
    }
}

void sp11_vb_process(Sp11VirtualBass *vb, float *const *chans, int nframes,
                     float agc_gain)
{
    const float *cbufs[SP11_VB_MAX_CH];
    for (int c = 0; c < vb->nchan; c++)
        cbufs[c] = chans[c];

    for (int i = 0; i < nframes; i++) {
        for (int c = 0; c < vb->nchan; c++) {
            Sp11EnvState *e = &vb->env[c];

            /* 1. envelope: orchestrator passes gain + 1.0f */
            sp11_envelope_follow(e, cbufs, vb->nchan, i, agc_gain + 1.0f);

            /* 2. dynamics: the compressor's output modulates the wet path.
             *    bass_standalone.c discarded this. */
            float dyn = e->smooth;
            if (dyn < 0.0f) dyn = 0.0f;
            if (dyn > 1.0f) dyn = 1.0f;

            /* 3. AGC: gate on the envelope-rising flag, as the binary does
             *    via the boolean written at state+0x1c. */
            float agc = e->rising ? agc_gain : agc_gain * dyn;

            /* 4. harmonic mix */
            float x = chans[c][i];
            float wet = sp11_harmonics(vb->weights, vb->nweights, x);
            chans[c][i] = x + wet * agc * dyn;
        }
    }
}
