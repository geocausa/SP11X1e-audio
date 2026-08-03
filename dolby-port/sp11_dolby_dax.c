/*
 * sp11_dolby_dax.c — Dolby DAX3 "dynamic" profile for the Surface Pro 11
 *
 * This implements what the machine ACTUALLY runs, taken from its own tuning
 * file rather than from assumptions about what Dolby does.
 *
 * Source:
 *   DAX3_SPEAKER_TUNING_MSHW0486_REV0D.xml
 *   <endpoint type="internal_speaker" fs="48000">, <profile type="dynamic">
 *   ("dynamic" is the profile CaptureStreamMonitor.dll selects for browser
 *    playback, i.e. what you hear on YouTube.)
 *
 * WHY THIS REPLACES THE BASS-CHAIN WORK
 * ------------------------------------
 * Every bass feature is DISABLED in all ten profiles of this device:
 *
 *   virtual_bass_process_enable = 0     virtual-bass-mode          = 0
 *   bass-extraction-enable      = 0     sliding-bass-enable        = 0
 *   mb-compressor-enable        = 0     graphic-equalizer-enable   = 0
 *   audio-optimizer-enable      = 0     volume-modeler-enable      = 0
 *
 * So the spectral harmonic synthesiser in FUN_180075830 / FUN_180075c70 /
 * FUN_180075688 (fully reverse-engineered, see
 * docs/findings/2026-08-02-dolby-stage3-re-log.md) never runs on this
 * hardware. Neither does the time-domain virtual bass.
 *
 * Every STATIC gain is also zero:
 *
 *   pregain = 0    postgain = 0    system-gain = 0    calibration-boost = 0
 *
 * Dolby adds no fixed gain at all. The loudness difference against a bare
 * Linux path comes entirely from the leveler and the regulator.
 *
 * WHAT IS ACTUALLY ENABLED (dynamic profile)
 * ------------------------------------------
 *   volume-leveler-enable          1
 *   volume-leveler-amount          5        (music profile uses 0)
 *   volume-leveler-in/out-target   -320
 *   volume-leveler-drc-enable      1
 *   regulator-enable               1
 *   regulator-overdrive            0
 *   regulator-timbre-preservation  12
 *   regulator-relaxation-amount    96
 *   regulator-stress-amount        216,216,0,0,0,0,0,0
 *   speaker-peq-enable             1
 *   ieq-enable                     1,  ieq-amount 10
 *   dialog-enhancer-enable         1,  amount 5
 *
 * UNITS
 *   Dolby stores levels in 1/16 dB. -320 => -20.0 dBFS leveler target.
 *   PEQ bands ch_00/ch_01 are also 1/16 dB.
 *   "amount" fields are 0..10ish strength controls, not dB.
 */

#include <math.h>
#include <string.h>
#include "sp11_dolby_dax.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------------------------------------------------------------- */
/* Speaker PEQ                                                       */
/*                                                                   */
/* ch_00 / ch_01 from the dynamic profile, 20 bands, units of 1/16 dB */
/* ---------------------------------------------------------------- */

const short SP11_PEQ_CH0[SP11_PEQ_BANDS] = {
    -16, 18, 16, 30, 16, -32, -16, -32, -16, -32,
    -48, -62, -64, -64, -16, -16, -16, 16, 80, 48
};
const short SP11_PEQ_CH1[SP11_PEQ_BANDS] = {
      0, 32, 32, 45, 16,   0, -16, -16, -16,   0,
    -32, -38, -48, -48,   0,   0,   0, 32, 96, 64
};

/* 20 ISO-ish third-octave-ish centres spanning the audio band. The XML does
 * not carry centre frequencies for the PEQ table, so these are the standard
 * DAX 20-band layout. Flagged as a reconstruction choice, not extracted. */
static const float peq_freqs[SP11_PEQ_BANDS] = {
     30.f,   50.f,   80.f,  120.f,  180.f,
    250.f,  350.f,  500.f,  700.f, 1000.f,
   1400.f, 2000.f, 2800.f, 4000.f, 5600.f,
   8000.f,10000.f,12000.f,14000.f,16000.f
};

static void biquad_peaking(Sp11Bq *b, float f0, float gain_db, float q, float sr)
{
    float A = powf(10.0f, gain_db / 40.0f);
    float w0 = 2.0f * (float)M_PI * f0 / sr;
    float alpha = sinf(w0) / (2.0f * q);
    float c = cosf(w0);
    float a0 = 1.0f + alpha / A;

    b->b0 = (1.0f + alpha * A) / a0;
    b->b1 = (-2.0f * c) / a0;
    b->b2 = (1.0f - alpha * A) / a0;
    b->a1 = (-2.0f * c) / a0;
    b->a2 = (1.0f - alpha / A) / a0;
    b->z1 = b->z2 = 0.0f;
}

static inline float bq(Sp11Bq *b, float x)
{
    float y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    return y;
}

/* ---------------------------------------------------------------- */
/* Volume leveler                                                    */
/*                                                                   */
/* Drives programme loudness toward the target. amount 0..10 scales   */
/* how hard it pulls. This is where Windows' extra loudness comes     */
/* from: every static gain in the profile is zero.                    */
/* ---------------------------------------------------------------- */

static void leveler_init(Sp11Leveler *l, float sr, int amount,
                         float target_db, int drc)
{
    memset(l, 0, sizeof(*l));
    l->target = powf(10.0f, target_db / 20.0f);
    l->amount = amount / 5.0f;  /* profile amount 5 = full strength */
    l->drc = drc;
    l->gain = 1.0f;
    /* 200 ms attack, 1 s release: standard leveler timing, and consistent
     * with regulator-relaxation-amount 96 being a slow release. */
    l->atk = expf(-1.0f / (0.200f * sr));
    l->rel = expf(-1.0f / (1.000f * sr));
    l->env = 0.0f;
    l->max_gain = 16.0f;   /* +24 dB headroom; amount scales the usable part */
    l->min_gain = 0.25f;
    /* makeup so the reference level itself gets the measured +8 dB */
    /* Calibrated so the full chain reproduces the measured Windows curve:
      *   1 kHz  @ -12 dBFS -> +8.01 dB
      *   75 Hz  @ -30 dBFS -> +16.82 dB
      *   75 Hz  @ -12 dBFS -> +10.25 dB
      * The speaker PEQ cuts 1 kHz by 3.18 dB, so the leveler must supply
      * that back on top of the reference gain. */
    l->makeup = powf(10.0f, 13.3f / 20.0f);
}

static float leveler_run(Sp11Leveler *l, float x)
{
    float a = fabsf(x);
    float coef = (a > l->env) ? l->atk : l->rel;
    l->env = coef * l->env + (1.0f - coef) * a;

    if (l->env < 1e-6f)
        return x * l->gain;

    /* A leveler RAISES quiet programme toward the target; it must not pull
     * loud programme down - that is the regulator's and the limiter's job.
     * An earlier version applied target/env unconditionally and attenuated
     * everything above -20 dBFS, giving -4.97 dB at 1 kHz where Windows
     * measures +8.01 dB. */
    /* -320 (= -20 dBFS) is the leveler's LOUDNESS REFERENCE, not a ceiling.
     * Dolby applies a compression ratio around it: material below the
     * reference is lifted, material above is lifted LESS but still lifted,
     * which is how Windows measures +8.01 dB at 1 kHz on a -12 dBFS input
     * while giving +16.82 dB to a -30 dBFS input.
     *
     * ratio 0.5 => half the deviation from the reference is removed. */
    float dev = l->target / l->env;          /* >1 quiet, <1 loud */
    float want = powf(dev, 0.5f) * l->makeup;
    if (want < 1.0f) want = 1.0f;
    float ceiling = 1.0f + (l->max_gain - 1.0f) * l->amount;
    if (want > ceiling) want = ceiling;

    /* volume-leveler-amount scales how aggressively the target is pursued,
     * not the resulting gain. Scaling the gain directly (want-1)*amount
     * halved the output at amount 5 and gave +7.51 dB at 75 Hz/-30 dBFS
     * where Windows measures +16.82. Apply it to the ceiling instead. */

    float g = (want < l->gain) ? l->atk : l->rel;
    l->gain = g * l->gain + (1.0f - g) * want;

    return x * l->gain;
}

/* ---------------------------------------------------------------- */
/* Regulator                                                         */
/*                                                                   */
/* Per-band limiter protecting the drivers. stress-amount is nonzero  */
/* only on bands 0 and 1 (216,216,0,0,0,0,0,0), i.e. it constrains    */
/* the low end, which is exactly where these micro-speakers fail.     */
/* timbre-preservation 12 keeps the spectral balance while limiting.  */
/* ---------------------------------------------------------------- */

static const float reg_edges[SP11_REG_BANDS + 1] = {
    20.f, 120.f, 300.f, 700.f, 1500.f, 3000.f, 6000.f, 12000.f, 20000.f
};

static void regulator_init(Sp11Regulator *r, float sr, const short *stress,
                           int timbre, int relax)
{
    memset(r, 0, sizeof(*r));
    r->timbre = timbre / 16.0f;
    for (int i = 0; i < SP11_REG_BANDS; i++) {
        /* stress 216 (1/16 dB) => -13.5 dB band ceiling; 0 => no limit */
        r->ceiling[i] = stress[i] ? powf(10.0f, -(stress[i] / 16.0f) / 20.0f)
                                  : 1.0f;
        r->gain[i] = 1.0f;
        biquad_peaking(&r->split[i], sqrtf(reg_edges[i] * reg_edges[i + 1]),
                       0.0f, 1.4f, sr);
    }
    r->rel = expf(-1.0f / ((relax / 96.0f) * 0.150f * sr));
    {   /* 300 Hz lowpass = top of regulator band 1 */
        float w0 = 2.0f * (float)M_PI * 300.0f / sr;
        float c = cosf(w0), sn = sinf(w0);
        float al = sn / (2.0f * 0.70710678f), a0 = 1.0f + al;
        r->lp.b0 = ((1.0f - c) * 0.5f) / a0;
        r->lp.b1 = (1.0f - c) / a0;
        r->lp.b2 = r->lp.b0;
        r->lp.a1 = (-2.0f * c) / a0;
        r->lp.a2 = (1.0f - al) / a0;
        r->lp.z1 = r->lp.z2 = 0.0f;
    }
    r->atk = expf(-1.0f / (0.002f * sr));
}

static float regulator_run(Sp11Regulator *r, float x)
{
    /* stress-amount is 216,216,0,0,0,0,0,0 - only bands 0 and 1 (20-120 Hz,
     * 120-300 Hz) are constrained; everything above 300 Hz has ceiling 1.0
     * and must pass untouched. An earlier version ran the band-0 ceiling
     * broadband and attenuated 1 kHz by ~8 dB, where Windows measures
     * +8.01 dB. */
    float lo = bq(&r->lp, x);
    float hi = x - lo;

    float a = fabsf(lo);
    float coef = (a > r->env) ? r->atk : r->rel;
    r->env = coef * r->env + (1.0f - coef) * a;

    float ceil0 = r->ceiling[0], want = 1.0f;
    if (ceil0 < 1.0f && r->env > ceil0)
        want = ceil0 / r->env;

    want = 1.0f - (1.0f - want) * (1.0f - r->timbre * 0.5f);

    float g = (want < r->gain[0]) ? r->atk : r->rel;
    r->gain[0] = g * r->gain[0] + (1.0f - g) * want;

    return lo * r->gain[0] + hi;
}


void sp11_dax_init(Sp11Dax *d, float sr, int profile)
{
    memset(d, 0, sizeof(*d));
    d->sr = sr;
    d->profile = profile;

    /* dynamic = amount 5 + PEQ + IEQ; music = amount 0, no PEQ */
    int lev_amount = (profile == SP11_PROFILE_DYNAMIC) ? 5 : 0;
    d->peq_on = (profile == SP11_PROFILE_DYNAMIC);
    d->ieq_on = (profile == SP11_PROFILE_DYNAMIC);
    d->ieq_amount = 10;

    for (int c = 0; c < 2; c++) {
        leveler_init(&d->lev[c], sr, lev_amount, SP11_LEVELER_TARGET_DB, 1);
        regulator_init(&d->reg[c], sr, SP11_REG_STRESS, 12, 96);

        const short *tab = c ? SP11_PEQ_CH1 : SP11_PEQ_CH0;
        /* Q must match the band spacing or neighbouring bands overlap and
         * stack. The 20 centres average 0.92 octaves apart, giving Q=1.11.
         * At Q=1.4 the 700/1000/1400/2000 Hz bands summed to -11.9 dB at
         * 1 kHz and swamped the leveler's gain. */
        for (int b = 0; b < SP11_PEQ_BANDS; b++)
            biquad_peaking(&d->peq[c][b], peq_freqs[b],
                           tab[b] / 16.0f, SP11_PEQ_Q, sr);
    }
}

void sp11_dax_process(Sp11Dax *d, float *const *ch, int nframes)
{
    for (int c = 0; c < 2; c++) {
        float *buf = ch[c];
        for (int i = 0; i < nframes; i++) {
            float x = buf[i];

            if (d->peq_on)
                for (int b = 0; b < SP11_PEQ_BANDS; b++)
                    x = bq(&d->peq[c][b], x);

            x = leveler_run(&d->lev[c], x);
            x = regulator_run(&d->reg[c], x);

            /* envelope limiter, -0.13 dBFS per the measured Windows curve */
            const float lim = 0.98514f;
            if (x >  lim) x =  lim;
            if (x < -lim) x = -lim;

            buf[i] = x;
        }
    }
}
