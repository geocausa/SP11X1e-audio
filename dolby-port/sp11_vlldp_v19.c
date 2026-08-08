/*
 * SP11 VLLDP v19 — Surface Pro 11 Dolby prototype, Linux/PipeWire
 *
 * IMPORTANT 2026-06-16:
 *   This file is not the exact Windows VLLDP live processor. Keep it as a
 *   runtime prototype only. The current exact evidence path is the v8 analyzer
 *   capture replay in tools/VlldpModel/replay_vlldp_v8_analyzer_capture.py,
 *   using captured-ring analyzer state plus native FUN_180021e80. Do not use
 *   this LADSPA implementation as the baseline while chasing loudness spikes.
 *
 * Implements an earlier WOLA (Weighted Overlap-Add) runtime approximation
 * informed by FUN_1800240e0 (OPENCODE output 14b) and the 20-band export
 * vectors known before the v8 analyzer-state probe.
 *
 * Architecture (matches Windows FUN_18001f7a8):
 *   1. Analysis: split audio into 20 frequency bands via FFT
 *   2. Apply per-band gains (static optimizer + dynamic regulator)
 *   3. Synthesis: IFFT + overlap-add back to time domain
 *   4. Volume leveler (slow broadband AGC)
 *   5. Final limiter
 *
 * The 20-band gain curve uses earlier Windows-derived init-pack values:
 *   - Static component: PID7 optimizer gains (rows 0+1 avg, /2080 normalised)
 *   - Dynamic component: signal-driven per-band compression using PID8 thresholds
 *     and PID17 group boundaries [2,7,16,20] from live child1 capture
 *
 * Band gains (normalised float, applied directly to STFT bins):
 *   From older family_a_export evidence at vol=14, Dynamic profile:
 *   [-618,-519,-450,-352,-364,-357,-237,-211,-302,-328,-314,-617,
 *    -1112,-1291,-1366,-1464,-1530,-1567,-1543,-1525] / 2080
 *   = [-0.297,-0.249,-0.216,-0.169,-0.175,-0.172,-0.114,-0.101,
 *      -0.145,-0.157,-0.151,-0.297,-0.535,-0.621,-0.657,-0.704,
 *      -0.736,-0.753,-0.742,-0.733]
 *
 * These are REDUCTION values (negative). The actual applied gain per band:
 *   gain[i] = 1.0 + reduction[i]  (at vol=14 steady state with 997Hz tone)
 * For music at normal listening level the dynamic part converges differently.
 * This prototype uses static optimizer constants plus simple envelope
 * followers for the signal-dependent part. The exact Windows live behavior is
 * being carried in the v8 captured-ring/native-leveler replay path instead.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ladspa.h>

#include "sp11_vlldp_init_pack.h"

#define BANDS       20
#define REG_GROUPS  SP11_VLLDP_GROUPS
#define FFT_SIZE    512     /* matches Dolby 48kHz/256-sample block profile */
#define HOP_SIZE    256     /* 50% overlap */
#define HALF        (FFT_SIZE/2 + 1)

/* Port indices */
enum {
    P_IN_L, P_IN_R, P_OUT_L, P_OUT_R,
    P_ENABLE,
    P_INTENSITY,
    P_VL_ENABLE,
    P_REG_ENABLE,
    P_CEILING,
    P_COUNT
};

/* VLLDP 20-band centre frequencies (Hz) — from FUN_180025b20 */
static const float BAND_HZ[BANDS] = {
    47,141,234,328,469,656,844,1031,1313,1688,
    2250,3000,3750,4688,5813,7125,9000,11250,13875,19688
};

/* Empirical raw-to-dB scale used by the prior Windows vector match. */
#define OPT_RAW_TO_DB (1.0f / 10.44f)

static float optimizer_gain_db(int band)
{
    float raw = 0.5f * (float)(sp11_vlldp_audio_optimizer_gains[0][band] +
                               sp11_vlldp_audio_optimizer_gains[1][band]);
    return raw * OPT_RAW_TO_DB;
}

static float threshold_low_norm(int band)
{
    return (float)sp11_vlldp_threshold_low[band] / 2080.0f;
}

/* FFT using simple DIT Cooley-Tukey (no external dependency) */
static void fft_real(float *re, float *im, int n, int inverse)
{
    /* bit-reverse */
    int j = 0;
    for (int i = 1; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t; t=re[i]; re[i]=re[j]; re[j]=t;
            t=im[i]; im[i]=im[j]; im[j]=t;
        }
    }
    /* butterfly */
    for (int len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * (float)M_PI / len * (inverse ? 1 : -1);
        float wre = cosf(ang), wim = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cur_re = 1.0f, cur_im = 0.0f;
            for (int k = 0; k < len/2; k++) {
                float ur=re[i+k], ui=im[i+k];
                float vr=re[i+k+len/2]*cur_re - im[i+k+len/2]*cur_im;
                float vi=re[i+k+len/2]*cur_im + im[i+k+len/2]*cur_re;
                re[i+k]=ur+vr; im[i+k]=ui+vi;
                re[i+k+len/2]=ur-vr; im[i+k+len/2]=ui-vi;
                float nr=cur_re*wre-cur_im*wim;
                cur_im=cur_re*wim+cur_im*wre; cur_re=nr;
            }
        }
    }
    if (inverse) {
        float inv = 1.0f / n;
        for (int i = 0; i < n; i++) { re[i]*=inv; im[i]*=inv; }
    }
}

typedef struct {
    unsigned long  rate;
    LADSPA_Data   *ports[P_COUNT];

    /* WOLA filterbank state per channel */
    float  in_buf[2][FFT_SIZE];    /* circular input history */
    float  out_buf[2][FFT_SIZE];   /* overlap-add output buffer */
    int    buf_pos;                /* write position in circular buffer */

    /* Hann window */
    float  window[FFT_SIZE];

    /* FFT scratch */
    float  fft_re[FFT_SIZE];
    float  fft_im[FFT_SIZE];

    /* Per-band gain state */
    float  band_gain[BANDS];       /* current smoothed gain per band */
    float  band_det[BANDS];        /* envelope detector per band */

    /* Volume leveler */
    float  vl_env;

    /* Limiter */
    float  lim_env;

    /* Band → FFT bin mapping */
    int    band_start[BANDS];
    int    band_end[BANDS];

} SP11;

static void compute_band_bins(SP11 *p)
{
    float bin_hz = (float)p->rate / FFT_SIZE;
    for (int b = 0; b < BANDS; b++) {
        float lo = (b == 0)         ? 0.0f : (BAND_HZ[b-1] + BAND_HZ[b]) * 0.5f;
        float hi = (b == BANDS-1)   ? (float)p->rate * 0.5f
                                    : (BAND_HZ[b] + BAND_HZ[b+1]) * 0.5f;
        p->band_start[b] = (int)(lo / bin_hz);
        p->band_end[b]   = (int)(hi / bin_hz) + 1;
        if (p->band_end[b] > HALF) p->band_end[b] = HALF;
        if (p->band_start[b] < 1)  p->band_start[b] = 1;
    }
}

static void init_state(SP11 *p)
{
    memset(p->in_buf,  0, sizeof p->in_buf);
    memset(p->out_buf, 0, sizeof p->out_buf);
    p->buf_pos = 0;
    /* Hann window */
    for (int i = 0; i < FFT_SIZE; i++)
        p->window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (FFT_SIZE - 1)));
    for (int b = 0; b < BANDS; b++) {
        p->band_gain[b] = 1.0f;
        p->band_det[b]  = 0.0f;
    }
    p->vl_env  = 0.0f;
    p->lim_env = 0.891f;
    compute_band_bins(p);
}

static LADSPA_Handle sp11_instantiate(const LADSPA_Descriptor *d, unsigned long rate)
{
    (void)d;
    SP11 *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->rate = rate;
    init_state(p);
    return p;
}

static void sp11_connect(LADSPA_Handle h, unsigned long port, LADSPA_Data *data)
{
    SP11 *p = h;
    if (port < P_COUNT) p->ports[port] = data;
}

static void sp11_activate(LADSPA_Handle h) { init_state((SP11 *)h); }

/*
 * Process one stereo sample through the WOLA filterbank with per-band gains.
 * Returns output sample pair via *ol, *or_.
 */
static void process_wola(SP11 *p, float in_l, float in_r,
                         const float *band_gains,
                         float *ol, float *or_)
{
    /* Store input sample in circular buffer */
    p->in_buf[0][p->buf_pos] = in_l;
    p->in_buf[1][p->buf_pos] = in_r;

    /* Read from overlap-add output buffer */
    *ol  = p->out_buf[0][p->buf_pos];
    *or_ = p->out_buf[1][p->buf_pos];
    p->out_buf[0][p->buf_pos] = 0.0f;
    p->out_buf[1][p->buf_pos] = 0.0f;

    p->buf_pos = (p->buf_pos + 1) % FFT_SIZE;

    /* Every HOP_SIZE samples, run a full FFT frame */
    if ((p->buf_pos % HOP_SIZE) != 0) return;

    for (int ch = 0; ch < 2; ch++) {
        /* Fill FFT input: circular buffer → windowed frame */
        for (int i = 0; i < FFT_SIZE; i++) {
            int idx = (p->buf_pos + i) % FFT_SIZE;
            p->fft_re[i] = p->in_buf[ch][idx] * p->window[i];
            p->fft_im[i] = 0.0f;
        }

        /* Forward FFT */
        fft_real(p->fft_re, p->fft_im, FFT_SIZE, 0);

        /* Apply per-band gains to FFT bins */
        for (int b = 0; b < BANDS; b++) {
            float g = band_gains[b];
            for (int k = p->band_start[b]; k < p->band_end[b]; k++) {
                p->fft_re[k] *= g;
                p->fft_im[k] *= g;
                /* Conjugate symmetry */
                if (k > 0 && k < FFT_SIZE/2) {
                    p->fft_re[FFT_SIZE-k] = p->fft_re[k];
                    p->fft_im[FFT_SIZE-k] = -p->fft_im[k];
                }
            }
        }

        /* Inverse FFT */
        fft_real(p->fft_re, p->fft_im, FFT_SIZE, 1);

        /* Overlap-add into output buffer */
        for (int i = 0; i < FFT_SIZE; i++) {
            int idx = (p->buf_pos + i) % FFT_SIZE;
            float *obuf = (ch == 0) ? p->out_buf[0] : p->out_buf[1];
            obuf[idx] += p->fft_re[i] * p->window[i] / (FFT_SIZE * 0.5f);
        }
    }
}

static void sp11_run(LADSPA_Handle h, unsigned long n)
{
    SP11 *p = h;
    const float *il  = p->ports[P_IN_L];
    const float *ir  = p->ports[P_IN_R];
    float       *ol  = p->ports[P_OUT_L];
    float       *or_ = p->ports[P_OUT_R];
    if (!il || !ir || !ol || !or_) return;

    float enable    = p->ports[P_ENABLE]     ? *p->ports[P_ENABLE]     : 1.0f;
    float intensity = p->ports[P_INTENSITY]  ? *p->ports[P_INTENSITY]  : 1.0f;
    float vl_on     = p->ports[P_VL_ENABLE]  ? *p->ports[P_VL_ENABLE]  : 1.0f;
    float reg_on    = p->ports[P_REG_ENABLE] ? *p->ports[P_REG_ENABLE] : 1.0f;
    float ceil_db   = p->ports[P_CEILING]    ? *p->ports[P_CEILING]    : -1.0f;
    float ceiling   = powf(10.f, ceil_db / 20.f);

    if (enable < 0.5f) {
        if (ol != il)  memcpy(ol,  il, n * sizeof(float));
        if (or_ != ir) memcpy(or_, ir, n * sizeof(float));
        return;
    }

    const float fs      = (float)p->rate;
    const float vl_atk  = 1.0f - expf(-1.0f / (0.150f * fs));
    const float vl_rel  = 1.0f - expf(-1.0f / (0.800f * fs));
    const float vl_tgt  = 0.251f;
    const float reg_atk = 1.0f - expf(-1.0f / (0.005f * fs));
    const float reg_rel = 1.0f - expf(-1.0f / (0.080f * fs));
    const float lim_atk = 1.0f - expf(-1.0f / (0.001f * fs));
    const float lim_rel = 1.0f - expf(-1.0f / (0.050f * fs));

    /* Pre-compute linear threshold amplitudes */
    float thr_lin[BANDS];
    for (int i = 0; i < BANDS; i++)
        thr_lin[i] = powf(10.f, threshold_low_norm(i));

    /* Compute static optimizer gains (linear) */
    float opt_lin[BANDS];
    for (int i = 0; i < BANDS; i++)
        opt_lin[i] = powf(10.f, optimizer_gain_db(i) * intensity / 20.f);

    for (unsigned long s = 0; s < n; s++) {
        float l = il[s], r = ir[s];

        /* ── Volume Leveler ── */
        if (vl_on > 0.5f) {
            float mono = 0.5f * (fabsf(l) + fabsf(r));
            if (mono > p->vl_env) p->vl_env += vl_atk * (mono - p->vl_env);
            else                  p->vl_env += vl_rel * (mono - p->vl_env);
            if (p->vl_env < 1e-9f) p->vl_env = 1e-9f;
            float vl_g = vl_tgt / p->vl_env;
            if (vl_g > 3.98f) vl_g = 3.98f;   /* +12 dB max */
            if (vl_g < 0.50f) vl_g = 0.50f;   /* -6 dB max */
            vl_g = 1.0f + (vl_g - 1.0f) * intensity * 0.5f;
            l *= vl_g; r *= vl_g;
        }

        /* ── Dynamic Regulator: per-band envelope + group detection ── */
        if (reg_on > 0.5f) {
            /* Simple broadband envelope per band using input signal */
            float mono = 0.5f * (fabsf(l) + fabsf(r));
            for (int b = 0; b < BANDS; b++) {
                if (mono > p->band_det[b]) p->band_det[b] += reg_atk*(mono-p->band_det[b]);
                else                       p->band_det[b] += reg_rel*(mono-p->band_det[b]);
            }
            /* Group max for non-isolated bands */
            float gmax[REG_GROUPS] = {0};
            int gs = 0;
            for (int g = 0; g < REG_GROUPS; g++) {
                for (int b = gs; b < sp11_vlldp_group_bounds[g]; b++)
                    if (p->band_det[b] > gmax[g]) gmax[g] = p->band_det[b];
                gs = sp11_vlldp_group_bounds[g];
            }
            /* Per-band gain: static optimizer × dynamic reduction */
            gs = 0;
            for (int g = 0; g < REG_GROUPS; g++) {
                for (int b = gs; b < sp11_vlldp_group_bounds[g]; b++) {
                    float level = sp11_vlldp_isolated_bands[b] ? p->band_det[b] : gmax[g];
                    float thr   = thr_lin[b];
                    float tg    = opt_lin[b];   /* start from optimizer gain */
                    if (level > thr && thr > 1e-9f) {
                        float over = (level - thr) / thr;
                        float red  = over / (1.0f + over) * 0.6f * intensity;
                        tg *= (1.0f - red);
                        if (tg < opt_lin[b] * 0.25f) tg = opt_lin[b] * 0.25f;
                    }
                    /* Smooth toward target */
                    if (tg < p->band_gain[b]) p->band_gain[b] += reg_atk*(tg-p->band_gain[b]);
                    else                      p->band_gain[b] += reg_rel*(tg-p->band_gain[b]);
                }
                gs = sp11_vlldp_group_bounds[g];
            }
        } else {
            /* Regulator off: just use static optimizer gains */
            for (int b = 0; b < BANDS; b++) p->band_gain[b] = opt_lin[b];
        }

        /* ── WOLA Synthesis Filterbank ── */
        float out_l, out_r;
        process_wola(p, l, r, p->band_gain, &out_l, &out_r);

        /* Blend dry/wet by intensity (wet = processed, dry = unprocessed) */
        out_l = il[s] * (1.0f - intensity) + out_l * intensity;
        out_r = ir[s] * (1.0f - intensity) + out_r * intensity;

        /* ── Final Limiter ── */
        float peak = fmaxf(fabsf(out_l), fabsf(out_r));
        if (peak > p->lim_env) p->lim_env += lim_atk * (peak - p->lim_env);
        else                   p->lim_env += lim_rel * (peak - p->lim_env);
        float lim_g = (p->lim_env > ceiling) ? ceiling / p->lim_env : 1.0f;
        ol[s]  = out_l * lim_g;
        or_[s] = out_r * lim_g;
    }
}

static void sp11_cleanup(LADSPA_Handle h) { free(h); }

/* Descriptor */
static LADSPA_PortDescriptor  g_pdesc[P_COUNT];
static const char            *g_pname[P_COUNT];
static LADSPA_PortRangeHint   g_phint[P_COUNT];
static LADSPA_Descriptor      g_desc;

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index)
{
    if (index != 0) return NULL;
    if (g_desc.Label) return &g_desc;

    for (unsigned i = 0; i < P_COUNT; i++) {
        g_phint[i].HintDescriptor = 0;
        g_phint[i].LowerBound = 0.f;
        g_phint[i].UpperBound = 1.f;
    }
    g_pdesc[P_IN_L]      = LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO;
    g_pdesc[P_IN_R]      = LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO;
    g_pdesc[P_OUT_L]     = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    g_pdesc[P_OUT_R]     = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    g_pdesc[P_ENABLE]    = LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL;
    g_pdesc[P_INTENSITY] = LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL;
    g_pdesc[P_VL_ENABLE] = LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL;
    g_pdesc[P_REG_ENABLE]= LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL;
    g_pdesc[P_CEILING]   = LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL;

    g_pname[P_IN_L]      = "Input L";
    g_pname[P_IN_R]      = "Input R";
    g_pname[P_OUT_L]     = "Output L";
    g_pname[P_OUT_R]     = "Output R";
    g_pname[P_ENABLE]    = "Live Processor Gate (child1+0xc6c)";
    g_pname[P_INTENSITY] = "Intensity 0-1";
    g_pname[P_VL_ENABLE] = "Volume Leveler";
    g_pname[P_REG_ENABLE]= "Regulator";
    g_pname[P_CEILING]   = "Output Ceiling dBFS";

    g_phint[P_CEILING].HintDescriptor =
        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    g_phint[P_CEILING].LowerBound = -24.f;
    g_phint[P_CEILING].UpperBound =   0.f;

    g_desc.UniqueID          = 902019;
    g_desc.Label             = "sp11_vlldp_v19";
    g_desc.Properties        = LADSPA_PROPERTY_HARD_RT_CAPABLE;
    g_desc.Name              = "SP11 VLLDP v19 Live Processor (Windows init pack)";
    g_desc.Maker             = "SP11 RE workspace";
    g_desc.Copyright         = "GPL-compatible local research plugin";
    g_desc.PortCount         = P_COUNT;
    g_desc.PortDescriptors   = g_pdesc;
    g_desc.PortNames         = g_pname;
    g_desc.PortRangeHints    = g_phint;
    g_desc.instantiate       = sp11_instantiate;
    g_desc.connect_port      = sp11_connect;
    g_desc.activate          = sp11_activate;
    g_desc.run               = sp11_run;
    g_desc.run_adding        = NULL;
    g_desc.set_run_adding_gain = NULL;
    g_desc.deactivate        = NULL;
    g_desc.cleanup           = sp11_cleanup;
    return &g_desc;
}
