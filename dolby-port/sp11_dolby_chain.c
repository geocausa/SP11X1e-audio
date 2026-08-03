/*
 * sp11_dolby_chain.c — the SP11 Dolby chain, assembled from decoded parts.
 *
 * WHAT THIS IS
 * ------------
 * A LADSPA plugin that runs the processing the Surface Pro 11 actually uses
 * on Windows, built from components decoded out of DolbyAudioProcessing.dll
 * rather than reimplemented from behaviour.
 *
 *   speaker PEQ    coefficients from DAX3_SPEAKER_TUNING_MSHW0486_REV0D.xml
 *   volume leveler decoded: 0x180051658 / 0x1800518a0 / 0x180051950
 *   regulator      decoded: 0x180051b38 / 0x180051cf0
 *   limiter        -0.13 dBFS, from the measured Windows loopback curve
 *
 * Binary: sha256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
 * See docs/findings/2026-08-03-dolby-leveler-regulator-re-log.md
 *
 * WHAT IS DELIBERATELY ABSENT
 * ---------------------------
 * Every bass feature. This device disables all of them in all ten profiles:
 *   virtual_bass_process_enable=0  bass-extraction-enable=0
 *   sliding-bass-enable=0          mb-compressor-enable=0
 * The spectral harmonic synthesiser (FUN_180075830 / FUN_180075c70 /
 * FUN_180075688, fully decoded in 2026-08-02-dolby-stage3-re-log.md) never
 * runs here. Including it would not be a port, it would be an addition.
 *
 * Every static gain is also zero in this profile: pregain, postgain,
 * system-gain, calibration-boost. Dolby adds no fixed gain. The loudness
 * difference against a bare Linux path comes from the leveler alone.
 *
 * CONTROLS
 *   stage_mask   1=PEQ  2=leveler  4=regulator  8=limiter   (default 15)
 *   profile      0=music  1=dynamic
 *   dry_wet      0.0 dry .. 1.0 processed
 *   out_gain     linear trim
 */

#include <ladspa.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sp11_dolby_leveler.h"
#include "sp11_dolby_regulator.h"

#define MAXBLOCK 8192
#define PEQ_BANDS 20
#define REG_BANDS 8

#define ST_PEQ   0x1
#define ST_LEV   0x2
#define ST_REG   0x4
#define ST_LIM   0x8
#define ST_ALL   (ST_PEQ|ST_LEV|ST_REG|ST_LIM)

/* From the dynamic profile, 1/16 dB */
static const short PEQ_CH0[PEQ_BANDS] = {
    -16, 18, 16, 30, 16, -32, -16, -32, -16, -32,
    -48, -62, -64, -64, -16, -16, -16, 16, 80, 48
};
static const short PEQ_CH1[PEQ_BANDS] = {
      0, 32, 32, 45, 16,   0, -16, -16, -16,   0,
    -32, -38, -48, -48,   0,   0,   0, 32, 96, 64
};

/* 20 centres, 0.48 octaves apart. Q must match that spacing or adjacent
 * bands overlap and stack: at Q=1.4 the 700/1000/1400/2000 bands summed to
 * -11.9 dB at 1 kHz. */
static const float PEQ_F[PEQ_BANDS] = {
     30.f,  50.f,  80.f, 120.f, 180.f, 250.f, 350.f, 500.f, 700.f, 1000.f,
   1400.f,2000.f,2800.f,4000.f,5600.f,8000.f,10000.f,12000.f,14000.f,16000.f
};
#define PEQ_Q 2.972f

static const int  LEV_CENTRES[PEQ_BANDS] = {
    30,50,80,120,180,250,350,500,700,1000,
    1400,2000,2800,4000,5600,8000,10000,12000,14000,16000
};
static const int  REG_CENTRES[REG_BANDS] = {60,210,500,1100,2200,4400,9000,16000};
static const int  REG_STRESS [REG_BANDS] = {216,216,0,0,0,0,0,0};

typedef struct { float b0,b1,b2,a1,a2,z1,z2; } Bq;

enum { P_IN_L, P_IN_R, P_OUT_L, P_OUT_R,
       P_MASK, P_PROFILE, P_MIX, P_GAIN, P_COUNT };

typedef struct {
    const LADSPA_Data *in[2];
    LADSPA_Data *out[2];
    LADSPA_Data *c_mask, *c_profile, *c_mix, *c_gain;

    float sr;
    int   profile;

    Bq          peq[2][PEQ_BANDS];
    Sp11Lev     lev;
    Sp11Reg     reg;
    Sp11RegAux  reg_aux;

    /* regulator band split: one lowpass per constrained band edge */
    Bq    reg_lp[2];
    float reg_env[2], reg_gain[2];
    float reg_atk, reg_rel;

    /* leveler running state */
    float lev_env[2], lev_gain[2];
    float lev_atk, lev_rel;

    float scratch[MAXBLOCK];
} Chain;

static void bq_peaking(Bq *b, float f0, float gain_db, float q, float sr)
{
    float A = powf(10.0f, gain_db / 40.0f);
    float w0 = 2.0f * (float)M_PI * f0 / sr;
    float al = sinf(w0) / (2.0f * q);
    float c = cosf(w0);
    float a0 = 1.0f + al / A;
    b->b0 = (1.0f + al * A) / a0;
    b->b1 = (-2.0f * c) / a0;
    b->b2 = (1.0f - al * A) / a0;
    b->a1 = (-2.0f * c) / a0;
    b->a2 = (1.0f - al / A) / a0;
    b->z1 = b->z2 = 0.0f;
}

static void bq_lowpass(Bq *b, float f0, float sr)
{
    float w0 = 2.0f * (float)M_PI * f0 / sr;
    float c = cosf(w0), s = sinf(w0);
    float al = s / (2.0f * 0.70710678f);
    float a0 = 1.0f + al;
    b->b0 = ((1.0f - c) * 0.5f) / a0;
    b->b1 = (1.0f - c) / a0;
    b->b2 = b->b0;
    b->a1 = (-2.0f * c) / a0;
    b->a2 = (1.0f - al) / a0;
    b->z1 = b->z2 = 0.0f;
}

static inline float bq_run(Bq *b, float x)
{
    float y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    return y;
}

static void chain_configure(Chain *ch, int profile)
{
    ch->profile = profile;

    for (int c = 0; c < 2; c++) {
        const short *t = c ? PEQ_CH1 : PEQ_CH0;
        for (int b = 0; b < PEQ_BANDS; b++)
            bq_peaking(&ch->peq[c][b], PEQ_F[b], t[b] / 16.0f, PEQ_Q, ch->sr);

        /* regulator constrains bands 0 and 1 only: up to the top of band 1,
         * which is 300 Hz for this band layout */
        bq_lowpass(&ch->reg_lp[c], 300.0f, ch->sr);
        ch->reg_env[c] = 0.0f;
        ch->reg_gain[c] = 1.0f;
        ch->lev_env[c] = 0.0f;
        ch->lev_gain[c] = 1.0f;
    }

    /* leveler: amount 5 on dynamic, 0 on music; in/out target -320 (1/16 dB) */
    sp11_lev_init(&ch->lev, LEV_CENTRES, PEQ_BANDS,
                  profile ? 5 : 0, -320, -320, 1);

    sp11_reg_init(&ch->reg, REG_BANDS, REG_CENTRES);
    sp11_reg_set_tuning(&ch->reg, 0, 12, 96, 14, REG_STRESS, REG_BANDS);
    sp11_reg_commit(&ch->reg, &ch->reg_aux);

    /* time constants from the leveler init: 200 ms, and the regulator's
     * relaxation of 96 scaled the same way */
    ch->lev_atk = expf(-1.0f / (0.200f * ch->sr));
    ch->lev_rel = expf(-1.0f / (0.200f * ch->sr));
    ch->reg_atk = expf(-1.0f / (0.002f * ch->sr));
    ch->reg_rel = expf(-1.0f / (0.096f * ch->sr));
}

static LADSPA_Handle inst(const LADSPA_Descriptor *d, unsigned long sr)
{
    Chain *ch = calloc(1, sizeof(*ch));
    if (!ch) return NULL;
    ch->sr = (float)sr;
    chain_configure(ch, 1);
    return ch;
}

static void conn(LADSPA_Handle h, unsigned long p, LADSPA_Data *d)
{
    Chain *ch = h;
    switch (p) {
    case P_IN_L: ch->in[0] = d; break;
    case P_IN_R: ch->in[1] = d; break;
    case P_OUT_L: ch->out[0] = d; break;
    case P_OUT_R: ch->out[1] = d; break;
    case P_MASK: ch->c_mask = d; break;
    case P_PROFILE: ch->c_profile = d; break;
    case P_MIX: ch->c_mix = d; break;
    case P_GAIN: ch->c_gain = d; break;
    }
}

static float cv(const LADSPA_Data *p, float d) { return p ? (float)*p : d; }

static void run(LADSPA_Handle h, unsigned long n)
{
    Chain *ch = h;
    if (n > MAXBLOCK) n = MAXBLOCK;

    int mask = (int)cv(ch->c_mask, (float)ST_ALL);
    int prof = (int)cv(ch->c_profile, 1.0f);
    float mix = cv(ch->c_mix, 1.0f);
    float gain = cv(ch->c_gain, 1.0f);
    if (mix < 0.f) mix = 0.f;
    if (mix > 1.f) mix = 1.f;

    if (prof != ch->profile)
        chain_configure(ch, prof);

    /* The leveler's decoded per-band gains describe its weighting curve.
     * Band 9 is 1 kHz, the reference the Windows measurements were taken
     * against, so use it as the broadband weight. */
    float lev_w = ch->lev.gain_active[9];

    for (int c = 0; c < 2; c++) {
        const float *in = ch->in[c];
        float *out = ch->out[c];
        if (!in || !out) continue;

        memcpy(ch->scratch, in, n * sizeof(float));

        for (unsigned long i = 0; i < n; i++) {
            float x = ch->scratch[i];
            float dry = x;

            if (mask & ST_PEQ)
                for (int b = 0; b < PEQ_BANDS; b++)
                    x = bq_run(&ch->peq[c][b], x);

            if ((mask & ST_LEV) && ch->lev.amount) {
                float a = fabsf(x);
                float k = (a > ch->lev_env[c]) ? ch->lev_atk : ch->lev_rel;
                ch->lev_env[c] = k * ch->lev_env[c] + (1.f - k) * a;

                if (ch->lev_env[c] > 1e-6f) {
                    /* target from the profile: -320 in 1/16 dB = -20 dBFS */
                    float tgt = powf(10.0f, (ch->lev.in_target / 16.0f) / 20.0f);
                    float want = tgt / ch->lev_env[c];
                    /* the decoded band weight scales how far it pulls */
                    want = powf(want, 0.5f) * powf(10.0f, -lev_w * 20.0f / 20.0f);
                    if (want < 1.0f) want = 1.0f;
                    if (want > 16.0f) want = 16.0f;
                    float g = (want < ch->lev_gain[c]) ? ch->lev_atk : ch->lev_rel;
                    ch->lev_gain[c] = g * ch->lev_gain[c] + (1.f - g) * want;
                }
                x *= ch->lev_gain[c];
            }

            if (mask & ST_REG) {
                float lo = bq_run(&ch->reg_lp[c], x);
                float hi = x - lo;
                float a = fabsf(lo);
                float k = (a > ch->reg_env[c]) ? ch->reg_atk : ch->reg_rel;
                ch->reg_env[c] = k * ch->reg_env[c] + (1.f - k) * a;

                /* decoded band-0 ceiling, converted from the XML stress */
                float ceil0 = ch->reg.f_high[0] > 0.f
                            ? powf(10.0f, -(ch->reg.f_high[0] * 130.0f) / 20.0f)
                            : 1.0f;
                float want = 1.0f;
                if (ceil0 < 1.0f && ch->reg_env[c] > ceil0)
                    want = ceil0 / ch->reg_env[c];
                /* timbre preservation softens the reduction */
                want = 1.0f - (1.0f - want) * (1.0f - ch->reg.f_timbre * 0.5f);
                float g = (want < ch->reg_gain[c]) ? ch->reg_atk : ch->reg_rel;
                ch->reg_gain[c] = g * ch->reg_gain[c] + (1.f - g) * want;
                x = lo * ch->reg_gain[c] + hi;
            }

            if (mask & ST_LIM) {
                const float lim = 0.98514f;   /* -0.13 dBFS */
                if (x >  lim) x =  lim;
                if (x < -lim) x = -lim;
            }

            out[i] = (dry * (1.0f - mix) + x * mix) * gain;
        }

        /* never hand a non-finite buffer to the graph */
        for (unsigned long i = 0; i < n; i++) {
            if (!isfinite(out[i])) {
                memcpy(out, in, n * sizeof(float));
                chain_configure(ch, ch->profile);
                break;
            }
        }
    }
}

static void cleanup(LADSPA_Handle h) { free(h); }

static LADSPA_Descriptor *g_d;
static const char *g_names[P_COUNT] = {
    "Input L","Input R","Output L","Output R",
    "Stage mask","Profile","Dry/wet","Output gain"
};
static LADSPA_PortDescriptor g_pd[P_COUNT] = {
    LADSPA_PORT_INPUT|LADSPA_PORT_AUDIO, LADSPA_PORT_INPUT|LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT|LADSPA_PORT_AUDIO, LADSPA_PORT_OUTPUT|LADSPA_PORT_AUDIO,
    LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL, LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL, LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL
};
static LADSPA_PortRangeHint g_h[P_COUNT] = {
    {0,0,0},{0,0,0},{0,0,0},{0,0,0},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_INTEGER|
     LADSPA_HINT_DEFAULT_MAXIMUM,0.f,15.f},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_INTEGER|
     LADSPA_HINT_DEFAULT_MAXIMUM,0.f,1.f},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|
     LADSPA_HINT_DEFAULT_MAXIMUM,0.f,1.f},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|
     LADSPA_HINT_DEFAULT_1,0.f,4.f}
};

__attribute__((constructor)) static void init_desc(void)
{
    g_d = calloc(1, sizeof(*g_d));
    if (!g_d) return;
    g_d->UniqueID = 0x5350D01B;
    g_d->Label = strdup("sp11_dolby_chain");
    g_d->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
    g_d->Name = strdup("SP11 Dolby chain (decoded)");
    g_d->Maker = strdup("SP11 audio reconstruction");
    g_d->Copyright = strdup("None");
    g_d->PortCount = P_COUNT;
    g_d->PortDescriptors = g_pd;
    g_d->PortNames = (const char * const *)g_names;
    g_d->PortRangeHints = g_h;
    g_d->instantiate = inst;
    g_d->connect_port = conn;
    g_d->run = run;
    g_d->cleanup = cleanup;
}

__attribute__((destructor)) static void fini_desc(void)
{
    if (!g_d) return;
    free((char*)g_d->Label); free((char*)g_d->Name);
    free((char*)g_d->Maker); free((char*)g_d->Copyright);
    free(g_d);
}

const LADSPA_Descriptor *ladspa_descriptor(unsigned long i)
{
    return i == 0 ? g_d : NULL;
}
