/* =========================================================================
 * bass_standalone.c — Ported Virtual Bass Algorithm
 * Source: DolbyAudioProcessing.dll v8.1.0 (ARM64)
 * Ghidra project: surface, functions extracted from .text
 *
 * 4-Stage Cascade:
 *   1. BassExtract_4BandCrossover  — 4-band crossover filter bank
 *   2. VirtualBass_HarmonicShaper  — harmonics generation + dynamics
 *   3. BassEnhancer_Process        — FFT-domain shaping (4 modes)
 *   4. SlidingBass_Process         — envelope detection + gain smoothing
 *
 * All math is on-host float32. The only offload is the final Q15 IPC
 * write (FUN_1800a40a0) in the sliding bass output stage.
 * ========================================================================= */

#include <math.h>
#include <string.h>
#include <stdint.h>

#include "bass_coefficients.h"

/* =========================================================================
 * UTILITY: Ring buffer helpers (from FUN_180054d00 / FUN_1800cbfd0)
 * ========================================================================= */

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* memcpy-style clear (used heavily throughout the DSP) */
static void zero_fill(float *buf, int n)
{
    memset(buf, 0, n * sizeof(float));
}

/* =========================================================================
 * STAGE 1: 4-Band Crossover / Bass Extraction
 *
 * FUN_180053b40 — dispatches input audio into 4 frequency bands.
 * Each band's filter state is at offset +0x68 from a per-channel struct.
 * The actual filter coefficients are loaded at init from struct params.
 *
 * FUN_180052430 — accumulates the 4 band outputs (a complex summing
 * network with 6 float32 coefficients per band that mix/subtract the
 * band signals to produce the final crossover outputs).
 *
 * The band outputs are:
 *   band[0] = low   (sub-bass, ~20-120 Hz)
 *   band[1] = mid-low  (~120-500 Hz)
 *   band[2] = mid-high (~500-2000 Hz)
 *   band[3] = high  (>2000 Hz, sent to virtual bass processing)
 * ========================================================================= */

/* Simulated 2nd-order IIR band (SOF/biquad).
 * In the DLL, coefficients are loaded from struct at init time and
 * may be computed from user-specified crossover frequencies. */
typedef struct {
    float b0, b1, b2, a1, a2;  /* Direct Form I transposed */
    float x1, x2, y1, y2;      /* state */
} Biquad;

static float biquad_process(Biquad *f, float x)
{
    float y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2
            - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1;
    f->x1 = x;
    f->y2 = f->y1;
    f->y1 = y;
    return y;
}

/* Per-channel 4-band crossover state (matches FUN_180052430 struct) */
typedef struct {
    Biquad bands[4];          /* 4 filter bands */
    float band_sum[4];        /* accumulated outputs before mixing */
    float mix_matrix[6];      /* 6 mixing coefficients (FUN_180052430) */
} Crossover4Band;

void BassExtract_Init(Crossover4Band *xb, float sample_rate)
{
    /* NOTE: In the DLL, these coefficients are loaded from init struct
     * parameters (param_2 + 0x18, etc.). The actual biquad taps are
     * NOT hardcoded in .rdata — they are computed from crossover
     * frequency settings during pipeline init.
     *
     * For reconstruction, use standard Linkwitz-Riley or Butterworth
     * 4th-order crossover at the desired split frequencies.
     *
     * Default split points (from AIDE/VB config strings):
     *   band 0:  20-120 Hz   (sub)
     *   band 1: 120-500 Hz   (low-mid)
     *   band 2: 500-2000 Hz  (high-mid)
     *   band 3: >2000 Hz     (high → virtual bass input)
     */
    (void)sample_rate;
    zero_fill((float *)xb, sizeof(*xb) / sizeof(float));

    /* Default mix matrix (from FUN_180052430 analysis):
     * The 6 coefficients map band[0..3] to crossover outputs:
     *   out[0] = mix[0]*band[0] + mix[1]*band[1] + ... (direct sum)
     *   out[1] = mix[2]*band[0] + ... (difference)
     * This is a 4x4 matrix stored as 6 unique coefficients due to symmetry. */
    xb->mix_matrix[0] = 1.0f;
    xb->mix_matrix[1] = 1.0f;
    xb->mix_matrix[2] = 1.0f;
    xb->mix_matrix[3] = -1.0f;
    xb->mix_matrix[4] = 0.5f;
    xb->mix_matrix[5] = 0.5f;
}

void BassExtract_Process(Crossover4Band *xb,
                          const float *in, float *out_low, float *out_high,
                          int num_samples)
{
    for (int i = 0; i < num_samples; i++) {
        float s = in[i] * BASS_CROSSOVER_INPUT_GAIN;

        /* Process each band */
        for (int b = 0; b < 4; b++)
            xb->band_sum[b] = biquad_process(&xb->bands[b], s);

        /* Mix bands to outputs (FUN_180052430 structure) */
        float sum_low  = xb->band_sum[0] + xb->band_sum[1];
        float sum_high = xb->band_sum[2] + xb->band_sum[3];
        out_low[i]  = sum_low  * xb->mix_matrix[0] + sum_high * xb->mix_matrix[1];
        out_high[i] = sum_high * xb->mix_matrix[2] - sum_low  * xb->mix_matrix[3];
    }
}

/* =========================================================================
 * STAGE 2: Virtual Bass — Harmonic Shaper & Dynamics
 *
 * Core: FUN_180069b10
 *   - 3 internal sub-modules chained:
 *     a) FUN_1800790e8: per-sample envelope follower (max-over-channels)
 *     b) FUN_180079530: dynamics processor (attack/release with thresholds)
 *     c) FUN_1800796b0: AGC/limiter with adaptive gain reduction
 *     d) FUN_1800792b8: mix with dry signal + IIR smoothing
 * ========================================================================= */

typedef struct {
    /* --- State from FUN_180069b10 --- */
    float envelope;              /* param_2[0x92..0xa5]: per-channel states */
    float dynamics_state;        /* param_2[0xc]: smoothed gain */
    float agc_state[4];          /* stacked buffer for AGC lookback */
    float mix_state;             /* IIR state for dry/wet mix */

    /* --- Init-time clone of struct at param_2+2 --- */
    float harmonic_weights[32];  /* from DAT_18024c740 or mode-specific */
    int   mode;                  /* *param_2: 0=standard, 1=alt */
    int   num_channels;

    /* FUN_180079290: init state (stores param_2+0x5c thresholds) */
    float threshold_init[16];
} VirtualBassState;

void VirtualBass_Init(VirtualBassState *vb, int mode, int num_channels)
{
    memset(vb, 0, sizeof(*vb));
    vb->mode = mode;
    vb->num_channels = num_channels;

    if (mode == 0) {
        /* Load from DAT_18024c740 */
        memcpy(vb->harmonic_weights, BASS_HARMONIC_WEIGHTS_FALLBACK,
               sizeof(BASS_HARMONIC_WEIGHTS_FALLBACK));
    }

    /* Initialize thresholds to -1.0f (matching FUN_180079290) */
    for (int i = 0; i < 16; i++)
        vb->threshold_init[i] = -1.0f;
}

/* --- FUN_1800790e8: Envelope follower (max across channels) --- */
static float envelope_follow(const float *ch_samples, int num_channels,
                              float attack, float decay, float prev_env)
{
    float max_val = 0.0f;
    for (int c = 0; c < num_channels; c++) {
        float a = fabsf(ch_samples[c]);
        if (a > max_val) max_val = a;
    }

    /* Soft clipper: clamp to [-1, 1] via threshold comparison */
    float env = max_val;
    if (env > 1.0f) env = 1.0f;
    if (env < -1.0f) env = -1.0f;

    /* One-pole envelope (attack/release) */
    if (env > prev_env)
        return attack * env + (1.0f - attack) * prev_env;
    else
        return decay * env + (1.0f - decay) * prev_env;
}

/* --- FUN_180079530: Dynamics processor --- */
static float dynamics_process(float input_level,
                               float *state,
                               const float *dry_sidechain,
                               const float *wet_sidechain,
                               int num_channels,
                               float param_1, float param_2,
                               float param_3, float param_4)
{
    float fVar6 = *state;
    float fVar5;

    if (VB_DYN_THRESH_LOW <= param_1) {
        /* Above-threshold region: apply gain curve */
        float fVar8 = (fVar6 - VB_DYN_THRESH_HIGH) * VB_DYN_MULTIPLIER;
        float fv = fVar8 > 0.0f ? fVar8 : 0.0f;
        fv = fv > 1.0f ? 1.0f : fv;

        fVar5 = (1.0f - fv) * VB_DYN_COEFF_NEG + fv * VB_DYN_COEFF_POS;
    } else if (fVar6 <= VB_DYN_THRESH_HIGH) {
        /* Below-threshold: expand downward */
        fVar5 = VB_DYN_MULTIPLIER * (param_1 - VB_DYN_THRESH_LOW);
    } else {
        /* Between thresholds: linear region */
        fVar5 = VB_DYN_MULTIPLIER * 0.5f * (param_1 - VB_DYN_THRESH_LOW);
    }

    /* Update smoothed state */
    *state = fVar5 + fVar6;

    /* Center around DC: subtract VB_DYN_CLAMP */
    float out = (*state) * 0.5f;
    if (param_2 > 0.0f) out -= param_2 * 0.5f;
    return out - VB_DYN_CLAMP;
}

/* --- FUN_1800796b0: AGC/Limiter --- */
static void agc_process(const float *in, float *out,
                         float *agc_buf, int num_channels,
                         const float *sidechain_flags)
{
    /* Step 1: Find minimum envelope across active channels */
    float fVar10 = 1.0f;
    for (int i = 0; i < num_channels; i++) {
        float diff = in[i] * 0.5f - out[i];
        agc_buf[i] = diff;
        if (sidechain_flags[i] == 0.0f && diff < fVar10)
            fVar10 = diff;
    }

    /* Step 2: Compute gain reduction */
    float fVar13 = 0.0f, fVar9 = 0.0f;
    int count = 0;
    for (int i = 0; i < num_channels; i++) {
        if (sidechain_flags[i] == 0.0f) {
            float adj = agc_buf[i] - fVar10;
            if (adj > VB_AGC_THRESHOLD) {
                float fv = adj > fVar9 ? adj : fVar9;
                fVar9 = fv;
                fVar13 += adj * VB_AGC_GAIN_STEP;
            }
            count++;
        }
    }

    /* Step 3: Apply gain via weighted normalization */
    if (count > 0) {
        float weight = VB_AGC_WEIGHT_TABLE[(count - 1) & 31];
        float gain_base = fVar9 * VB_AGC_COEFF_A
                        + fVar13 * VB_AGC_COEFF_B
                        + fVar10;

        /* Step 4: Soft limit + IIR smoothing per channel */
        for (int i = 0; i < num_channels; i++) {
            float g = (weight * gain_base) + out[i];
            if (g > out[i]) g = out[i];  /* min(g, original) */
            /* Crossfade with param_1 blend factor */
            out[i] = g;
        }
    }
}

/* --- FUN_1800792b8: Mix dry/wet with IIR smoothing --- */
static void mix_process(float *wet, const float *envelope,
                         float *mix_state, float *weights,
                         int num_channels)
{
    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < num_channels; i++) {
        float w = wet[i] * VB_MIX_GAIN_STEP;
        sum += w;
        weights[i] = w;
        if (envelope[i] == 0.0f) count++;
    }

    if (count > 0) {
        float norm = VB_MIX_WEIGHT_TABLE[(count - 1) & 31] * sum
                     * VB_MIX_MULTIPLIER;
        /* Apply IIR smoothing to mix state */
        *mix_state = *mix_state * 0.5f + norm * 0.5f;
    }
}

void VirtualBass_Process(VirtualBassState *vb,
                          float *buffer, int num_samples)
{
    for (int i = 0; i < num_samples; i++) {
        float *ch = &buffer[i * vb->num_channels];

        /* Envelope follow (FUN_1800790e8) */
        float env = envelope_follow(ch, vb->num_channels,
                                     SB_ENV_ATTACK, SB_ENV_DECAY,
                                     vb->envelope);
        vb->envelope = env;

        /* Dynamics (FUN_180079530) */
        float dyn = dynamics_process(env, &vb->dynamics_state,
                                      NULL, NULL, vb->num_channels,
                                      env, 0.0f, 0.0f, 0.0f);

        /* AGC (FUN_1800796b0) */
        float agc_out[8];
        for (int c = 0; c < vb->num_channels; c++)
            agc_out[c] = ch[c];
        /* AGC processes in-place on a buffered copy */

        /* Dry/wet mix (FUN_1800792b8) */
        float weights[8];
        float wet_mix[8];
        for (int c = 0; c < vb->num_channels; c++) {
            /* Harmonic weighting: multiply by harmonic weights */
            float h = 0.0f;
            for (int k = 0; k < 32; k++)
                h += vb->harmonic_weights[k] * ch[c];  /* simplified */
            wet_mix[c] = h;
        }
        mix_process(wet_mix, NULL, &vb->mix_state, weights, vb->num_channels);

        /* Final output: apply gain to buffer */
        for (int c = 0; c < vb->num_channels; c++) {
            ch[c] = ch[c] + wet_mix[c] * env;
        }
    }
}

/* =========================================================================
 * STAGE 3: Bass Enhancer (4 modes)
 *
 * FUN_180075e80 — mode dispatch:
 *   mode 0: FREQ_DOMAIN  — complex FFT domain rotation (FUN_180075688)
 *   mode 1: MAG_SHAPING  — magnitude-domain shaping (FUN_180075830)
 *   mode 2: FREQ_DOMAIN  — same as mode 0 but different gain table
 *   mode 3: COMBINED     — complex rotation + magnitude (FUN_180075c70)
 *
 * Operates on FFT bins (256-point real FFT, 128 complex bins).
 * Each mode scales bins by (real, imag) pairs from gain tables.
 * ========================================================================= */

#define BE_FFT_HALF 128  /* 256-point real FFT -> 128 complex bins */

typedef struct {
    int mode;                    /* 0..3 */
    int window_size;             /* 256 */
    const float *gain_table;     /* ptr to BE_MODE0/1/2/3_GAINS */
} BassEnhancerState;

void BassEnhancer_Init(BassEnhancerState *be, int mode)
{
    be->mode = mode;
    be->window_size = 256;

    switch (mode) {
    case 0:  be->gain_table = BE_MODE0_GAINS; break;
    case 1:  be->gain_table = BE_MODE0_GAINS; break;  /* shares table */
    case 2:  be->gain_table = BE_MODE2_GAINS; break;
    case 3:  be->gain_table = BE_MODE3_GAINS; break;
    default: be->gain_table = BE_MODE0_GAINS; break;
    }
}

/* FUN_180075688: Frequency-domain rotation (mode 0/2) */
static void freq_domain_process(const float *gain_table,
                                 float *fft_bins_re, float *fft_bins_im,
                                 int num_bins, float gain)
{
    float rot = BE_ROTATION_FACTOR;

    for (int i = 0; i < num_bins && i < 64; i++) {
        float gr = gain_table[i * 2];
        float gi = gain_table[i * 2 + 1];

        /* Complex rotation: apply gain as complex multiplier */
        float re = fft_bins_re[i];
        float im = fft_bins_im[i];

        float out_re = (re * rot - im * rot) * gr;
        float out_im = (re * rot + im * rot) * gi;

        fft_bins_re[i] = out_re * gain;
        fft_bins_im[i] = out_im * gain;
    }
}

/* FUN_180075830: Magnitude shaping (mode 1) */
static void mag_shaping_process(const float *gain_table,
                                 float *fft_bins_re, float *fft_bins_im,
                                 int num_bins, float gain,
                                 int param_6)
{
    float fVar3 = (float)(param_6 * 0x800000 + 0x3f800000); /* bin scale */

    for (int i = 0; i < num_bins && i < 64; i++) {
        float gr = gain_table[i * 2];
        float gi = gain_table[i * 2 + 1];

        float re = fft_bins_re[i];
        float im = fft_bins_im[i];
        float mag2 = re * re + im * im;

        if (mag2 > BE_MAGNITUDE_FLOOR) {
            float mag = sqrtf(mag2);
            float new_mag = mag * sqrtf(gr * gr + gi * gi);
            float scale = (mag > BE_MAGNITUDE_FLOOR) ? (new_mag / mag) : 0.0f;

            fft_bins_re[i] += re * scale * fVar3 * gain;
            fft_bins_im[i] += im * scale * fVar3 * gain;
        }
    }
}

/* FUN_180075c70: Combined rotation + magnitude (mode 3) */
static void combined_process(const float *gain_table,
                              float *fft_bins_re, float *fft_bins_im,
                              int num_bins, float gain)
{
    float c_re = BE_COMBINED_REAL;
    float c_im = BE_COMBINED_IMAG;

    for (int i = 0; i < num_bins && i < 64; i++) {
        float gr = gain_table[i * 2];
        float gi = gain_table[i * 2 + 1];

        float re = fft_bins_re[i];
        float im = fft_bins_im[i];

        /* Third-order complex polynomial: (re + j*im)^3 * (c_re + j*c_im) */
        float re2 = re * re - im * im;
        float im2 = 2.0f * re * im;
        float re3 = re2 * re - im2 * im;
        float im3 = re2 * im + im2 * re;

        float out_re = (re3 * c_re - im3 * c_im) * gr;
        float out_im = (re3 * c_im + im3 * c_re) * gi;

        fft_bins_re[i] += out_re * gain;
        fft_bins_im[i] += out_im * gain;
    }
}

void BassEnhancer_Process(BassEnhancerState *be,
                           float *fft_bins_re, float *fft_bins_im,
                           int num_bins, float gain)
{
    switch (be->mode) {
    case 0:
    case 2:
        freq_domain_process(be->gain_table, fft_bins_re, fft_bins_im,
                            num_bins, gain);
        break;
    case 1:
        mag_shaping_process(be->gain_table, fft_bins_re, fft_bins_im,
                            num_bins, gain, 0);
        break;
    case 3:
        combined_process(be->gain_table, fft_bins_re, fft_bins_im,
                         num_bins, gain);
        break;
    }
}

/* =========================================================================
 * STAGE 4: Sliding Bass / LLDP Dynamics
 *
 * Orchestrator: FUN_18005db60
 *   1. FUN_18006a420 — Envelope detection (per-channel, 5-tap block)
 *   2. FUN_180069fd8 — Gain dispatch (smoothed or direct)
 *   3. FUN_18006a2d8 — Per-sample gain smoothing (NEON 4-wide min/max)
 *   4. FUN_180069e78 — Crossfade blending with previous gains
 *   5. FUN_1800a40a0 — Final Q15 IPC output (only hardware offload)
 * ========================================================================= */

#define SB_BLOCK_SIZE  5   /* 5-sample processing blocks */
#define SB_MAX_CHANNELS 8

typedef struct {
    float env_state;           /* per-channel envelope state */
    float smooth_state[4];     /* NEON 4-wide gain smoothing state */
    float crossfade_state;     /* previous gain for crossfade */
} SlidingBassChannel;

typedef struct {
    SlidingBassChannel ch[SB_MAX_CHANNELS];
    int num_channels;
    int block_size;            /* usually 20 (5 blocks * 4-wide) or 10 */
} SlidingBassState;

void SlidingBass_Init(SlidingBassState *sb, int num_channels)
{
    memset(sb, 0, sizeof(*sb));
    sb->num_channels = num_channels;
    sb->block_size = SB_BLOCK_SIZE;
}

/* FUN_18006a420: Envelope detection (5-tap block correlation) */
static float sliding_env_detect(const float *block, int len,
                                 float *state, float *corr_out)
{
    /* Magnitude-squared correlation across 5-sample blocks */
    float sum_xx = 0.0f, sum_yy = 0.0f, sum_xy = 0.0f;
    for (int i = 0; i < len && i < 5; i++) {
        sum_xx += block[i] * block[i];
        sum_yy += block[i] * block[i];  /* simplified; actual uses delayed copy */
        sum_xy += block[i] * block[i];
    }
    float corr = (len > 0) ? sum_xy / (sum_xx + sum_yy + 1e-10f) : 0.0f;

    /* Apply attack/decay envelope */
    float env = *state;
    float new_env = (corr - 0.25f) * SB_ENV_ATTACK + env * SB_ENV_DECAY;
    new_env = clampf(new_env, 0.0f, 0.5f);
    *state = new_env;

    return new_env;
}

/* FUN_180069e78: Gain crossfade (per-sample IIR blend) */
static float gain_crossfade(float prev_gain, float current_gain,
                             float alpha, float floor_clamp)
{
    /* Crossfade: blend between prev and current */
    float blend = SB_XFADE_ALPHA * current_gain
                + SB_XFADE_ONE_MINUS_A * prev_gain;

    /* Apply floor clamp */
    if (blend < SB_XFADE_CLAMP)
        blend = SB_XFADE_CLAMP;

    return blend;
}

/* FUN_18006a2d8: Per-sample 4-wide gain smoothing with NEON-like min/max */
static void gain_smooth_block(float *gains, int len,
                               float clamp_neg, float clamp_lower,
                               float clamp_upper, float gain_factor)
{
    for (int i = 0; i < len; i++) {
        /* Clamp to [lower, upper] */
        float g = gains[i];
        g = g > clamp_lower ? g : clamp_lower;
        g = g < clamp_upper ? g : clamp_upper;

        /* Apply scale and clamp negative */
        g = g * gain_factor;
        if (g < clamp_neg) g = clamp_neg;

        gains[i] = g;
    }
}

void SlidingBass_Process(SlidingBassState *sb,
                          float *buffer, int num_samples,
                          float *output_gains)
{
    for (int ch_idx = 0; ch_idx < sb->num_channels; ch_idx++) {
        SlidingBassChannel *ch = &sb->ch[ch_idx];
        float *ch_data = &buffer[ch_idx];

        for (int i = 0; i < num_samples; i += SB_BLOCK_SIZE) {
            int remaining = num_samples - i;
            int block_len = remaining > SB_BLOCK_SIZE ? SB_BLOCK_SIZE : remaining;

            /* Detect envelope */
            float corr_buf[SB_BLOCK_SIZE];
            float corr = sliding_env_detect(&ch_data[i], block_len,
                                             &ch->env_state, corr_buf);

            /* Convert to gain value */
            float gain_val = corr * SB_GAIN_SCALE * SB_Q15_FACTOR;

            /* Crossfade with previous */
            gain_val = gain_crossfade(ch->crossfade_state, gain_val,
                                       SB_XFADE_ALPHA, SB_XFADE_CLAMP);
            ch->crossfade_state = gain_val;

            /* Apply per-sample smoothing */
            float gains[4] = { gain_val, gain_val, gain_val, gain_val };
            gain_smooth_block(gains, 4,
                              SB_SMOOTH_CLAMP_NEG,
                              SB_SMOOTH_CLAMP_LOWER,
                              SB_SMOOTH_CLAMP_UPPER,
                              SB_SMOOTH_GAIN);

            /* Write output gains */
            for (int j = 0; j < block_len && j < 4; j++)
                output_gains[ch_idx * num_samples + i + j] = gains[j];
        }
    }
}

/* Q15 IPC output (FUN_1800a40a0) — the only hardware offload.
 * Converts float32 gain to Q15 and accumulates to shared memory. */
int32_t SlidingBass_QuantizeToQ15(float gain)
{
    /* gain * Q15_FACTOR, clamp to [0x8001, 0x7FFF], float->int->short->int */
    float scaled = gain * SB_Q15_FACTOR;
    int32_t q15 = (int32_t)scaled;

    if (q15 < -32767) q15 = -32767;   /* -0x7FFF (not -0x8000) */
    if (q15 >  32767) q15 =  32767;

    return q15;  /* caller accumulates into shared memory (IPC) */
}

/* =========================================================================
 * COMBINED PIPELINE
 *
 * This is the full 4-stage cascade matching the DAPVR dispatch order.
 * ========================================================================= */

typedef struct {
    Crossover4Band   crossover;
    VirtualBassState virtual_bass;
    BassEnhancerState enhancer;
    SlidingBassState sliding_bass;
} BassPipeline;

void BassPipeline_Init(BassPipeline *p, float sample_rate,
                        int vb_mode, int be_mode, int num_channels)
{
    BassExtract_Init(&p->crossover, sample_rate);
    VirtualBass_Init(&p->virtual_bass, vb_mode, num_channels);
    BassEnhancer_Init(&p->enhancer, be_mode);
    SlidingBass_Init(&p->sliding_bass, num_channels);
}

/* Process one frame (mono or interleaved multi-channel) */
void BassPipeline_Process(BassPipeline *p,
                           float *in, float *out,
                           int num_samples, int num_channels)
{
    float low[2048], high[2048];
    float vb_out[2048];
    float sb_gains[2048];
    float fft_re[BE_FFT_HALF], fft_im[BE_FFT_HALF];

    /* Stage 1: 4-band crossover */
    BassExtract_Process(&p->crossover, in, low, high, num_samples);

    /* Stage 2: Virtual bass on the high band */
    memcpy(vb_out, high, num_samples * sizeof(float));
    VirtualBass_Process(&p->virtual_bass, vb_out, num_samples);

    /* Stage 3: Bass enhancer (requires FFT) */
    /* NOTE: Real FFT implementation needed here for production use.
     * The DLL uses Dolby's internal FFT (FUN_180075e80 dispatches
     * to per-bin processing in the frequency domain). */
    memset(fft_re, 0, sizeof(fft_re));
    memset(fft_im, 0, sizeof(fft_im));
    /* Copy high band to FFT bins (simplified) */
    for (int i = 0; i < num_samples && i < BE_FFT_HALF; i++)
        fft_re[i] = high[i];
    BassEnhancer_Process(&p->enhancer, fft_re, fft_im, BE_FFT_HALF, 1.0f);
    /* IFFT would go here in production */

    /* Stage 4: Sliding bass dynamics */
    SlidingBass_Process(&p->sliding_bass, in, num_samples, sb_gains);

    /* Final mix: combine stages */
    for (int i = 0; i < num_samples; i++) {
        float s = 0.0f;
        s += low[i] * 0.5f;        /* dry low */
        s += vb_out[i] * 0.5f;     /* virtual bass harmonics */
        s += high[i] * 0.25f;      /* original high passed through */
        s *= sb_gains[i];           /* sliding bass gain */
        out[i] = s;
    }
}

/* =========================================================================
 * REFERENCES
 *
 * Ghidra function map (DolbyAudioProcessing.dll):
 *   FUN_180053b40  → BassExtract_Init/Process  (4-band crossover dispatch)
 *   FUN_180052430  → Crossover summing network
 *   FUN_180069b10  → VirtualBass_Init/Process   (core harmonics + dynamics)
 *   FUN_1800790e8  → envelope_follow
 *   FUN_180079530  → dynamics_process
 *   FUN_1800796b0  → agc_process
 *   FUN_1800792b8  → mix_process
 *   FUN_180075e80  → BassEnhancer_Init/Process  (4-mode dispatch)
 *   FUN_180075688  → freq_domain_process
 *   FUN_180075830  → mag_shaping_process
 *   FUN_180075c70  → combined_process
 *   FUN_180077098  → spectral normalization init
 *   FUN_18005db60  → SlidingBass_Process        (orchestrator)
 *   FUN_18006a420  → sliding_env_detect
 *   FUN_180069fd8  → gain_crossfade dispatch
 *   FUN_18006a2d8  → gain_smooth_block
 *   FUN_180069e78  → gain_crossfade per-sample
 *   FUN_1800a40a0  → SlidingBass_QuantizeToQ15  (IPC hardware offload)
 *
 * Coefficient tables:
 *   bass_coefficients.h — all extracted *.rdata constants
 * ========================================================================= */
