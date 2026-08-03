/*
 * sp11_dolby_modular.c — SP11 modular Dolby processing chain (LADSPA)
 *
 * Combines the reverse-engineered Dolby components into one switchable
 * plugin so the whole chain can be evaluated at once, and any single stage
 * can be isolated when something sounds wrong.
 *
 * STAGES
 *   1  BassExtract    4-band crossover                       IMPLEMENTED
 *   2  VirtualBass    harmonic synthesis + dynamics + AGC     IMPLEMENTED
 *   3  BassEnhancer   FFT spectral rotation                   DISABLED
 *   4  SlidingBass    level-dependent bass gain               IMPLEMENTED
 *   5  VLLDP          Dolby's live dynamic processor          separate plugin
 *
 * WHY STAGE 3 IS DISABLED RATHER THAN STUBBED
 * -------------------------------------------
 * bass_standalone.c's stage 3 does not match the shipped binary. Verified
 * 2026-08-02 against DolbyAudioProcessing.dll
 * (sha256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3)
 * by decompile and by objdump on the machine code:
 *
 *   - FUN_180075688 applies a 45-degree complex rotation (1/sqrt(2)) and a
 *     complex multiply against a SEPARATE coefficient array, then
 *     ACCUMULATES into its output. bass_standalone.c overwrites.
 *   - It has a second loop over the mirrored negative-frequency half at
 *     bin 0x100-k, confirming a 256-point real FFT. bass_standalone.c has
 *     no second loop.
 *   - param_6 is a gain-RAMP descriptor (count + float*, advancing one
 *     element per bin), not a per-bin table.
 *   - The dispatcher FUN_180075e80 is not a 4-way exclusive switch: modes 2
 *     and 3 call mag-shaping / combined AND THEN FUN_180075688, which is why
 *     accumulation matters. It also calls FUN_180077098 every block and
 *     zeroes a 2048-byte (256 complex bin) accumulator each time.
 *   - Per-mode gain comes from a runtime global at [x19,#1872], not from the
 *     BE_MODE*_GAINS tables.
 *   - BE_MODE0_GAINS[32] is documented in bass_coefficients.h as 12 gain
 *     floats followed by 20 FFT TWIDDLE factors. freq_domain_process() reads
 *     the whole array as [re,im] gain pairs with a bound of i<64, so it
 *     treats twiddles as audio gains and reads 96 floats out of bounds.
 *
 * A stage that silently processes garbage and reads out of bounds is worse
 * than an absent one, so it is compiled out unless SP11_ENABLE_STAGE3 is
 * defined. See docs/findings/2026-08-02-dolby-bass-stage3-fft-analysis.md
 *
 * CONTROLS (LADSPA ports)
 *   stage_mask   bitfield: 1=crossover 2=virtualbass 4=enhancer 8=slidingbass
 *   vb_mode      VirtualBass mode 0..3
 *   dry_wet      0.0 dry .. 1.0 fully processed
 *   out_gain     linear output trim
 *
 * Default stage_mask is 11 (1|2|8): every implemented stage on, enhancer off.
 */

#include <ladspa.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Pull in the extracted Dolby bass implementation. bass_standalone.c has no
 * main(), so including it gives us the stage functions and their bit-exact
 * coefficient tables directly. */
#define SP11_MODULAR_BUILD 1
#include "bass_standalone.c"
#include "sp11_dolby_stage1.h"
#include "sp11_dolby_stage2.h"

#define SP11_MAX_BLOCK   4096

/* stage bits */
#define ST_CROSSOVER     0x1
#define ST_VIRTUALBASS   0x2
#define ST_ENHANCER      0x4      /* not implemented, see header comment */
#define ST_SLIDINGBASS   0x8
#define ST_DEFAULT       (ST_CROSSOVER | ST_VIRTUALBASS | ST_SLIDINGBASS)

enum {
    PORT_IN_L = 0, PORT_IN_R,
    PORT_OUT_L, PORT_OUT_R,
    PORT_STAGE_MASK, PORT_VB_MODE, PORT_DRY_WET, PORT_OUT_GAIN,
    PORT_COUNT
};

typedef struct {
    const LADSPA_Data *in[2];
    LADSPA_Data       *out[2];
    LADSPA_Data       *ctl_stage_mask;
    LADSPA_Data       *ctl_vb_mode;
    LADSPA_Data       *ctl_dry_wet;
    LADSPA_Data       *ctl_out_gain;

    float sample_rate;
    int   vb_mode_active;
    int   warned_stage3;

    /* one processing chain per channel */
    Sp11Crossover     xover[2];
    Sp11VirtualBass   vb2;
    SlidingBassState  sliding_bass[2];

    float low[SP11_MAX_BLOCK];
    float high[SP11_MAX_BLOCK];
    float vb[SP11_MAX_BLOCK];
    float gains[SP11_MAX_BLOCK];
    float scratch[SP11_MAX_BLOCK];
} Sp11Dolby;

static void chain_init(Sp11Dolby *s, int vb_mode)
{
    for (int c = 0; c < 2; c++) {
        sp11_xover_init(&s->xover[c], s->sample_rate);
        SlidingBass_Init(&s->sliding_bass[c], 1);
    }
    sp11_vb_init(&s->vb2, 1, BASS_HARMONIC_WEIGHTS_FALLBACK,
                 (int)(sizeof(BASS_HARMONIC_WEIGHTS_FALLBACK)/sizeof(float)),
                 s->sample_rate);
    s->vb_mode_active = vb_mode;
}

static LADSPA_Handle sp11_instantiate(const LADSPA_Descriptor *d,
                                      unsigned long sample_rate)
{
    Sp11Dolby *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->sample_rate = (float)sample_rate;
    chain_init(s, 0);
    return s;
}

static void sp11_connect(LADSPA_Handle h, unsigned long port, LADSPA_Data *p)
{
    Sp11Dolby *s = h;
    switch (port) {
    case PORT_IN_L:        s->in[0] = p; break;
    case PORT_IN_R:        s->in[1] = p; break;
    case PORT_OUT_L:       s->out[0] = p; break;
    case PORT_OUT_R:       s->out[1] = p; break;
    case PORT_STAGE_MASK:  s->ctl_stage_mask = p; break;
    case PORT_VB_MODE:     s->ctl_vb_mode = p; break;
    case PORT_DRY_WET:     s->ctl_dry_wet = p; break;
    case PORT_OUT_GAIN:    s->ctl_out_gain = p; break;
    default: break;
    }
}

static float ctl(const LADSPA_Data *p, float dflt)
{
    return p ? (float)*p : dflt;
}

static int finite_buf(const float *b, unsigned long n)
{
    for (unsigned long i = 0; i < n; i++)
        if (!isfinite(b[i]))
            return 0;
    return 1;
}

static void sp11_run(LADSPA_Handle h, unsigned long nframes)
{
    Sp11Dolby *s = h;

    if (nframes > SP11_MAX_BLOCK)
        nframes = SP11_MAX_BLOCK;

    int mask    = (int)ctl(s->ctl_stage_mask, (float)ST_DEFAULT);
    int vb_mode = (int)ctl(s->ctl_vb_mode, 0.0f);
    float mix   = ctl(s->ctl_dry_wet, 1.0f);
    float gain  = ctl(s->ctl_out_gain, 1.0f);

    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    if (vb_mode < 0) vb_mode = 0;
    if (vb_mode > 3) vb_mode = 3;

    if (vb_mode != s->vb_mode_active)
        chain_init(s, vb_mode);

    if ((mask & ST_ENHANCER) && !s->warned_stage3) {
        fprintf(stderr,
                "sp11-dolby: stage 3 (BassEnhancer) requested but is not "
                "implemented; see docs/findings/2026-08-02-dolby-bass-stage3-"
                "fft-analysis.md. Ignoring.\n");
        s->warned_stage3 = 1;
    }

    for (int c = 0; c < 2; c++) {
        const float *in  = s->in[c];
        float       *out = s->out[c];
        if (!in || !out)
            continue;

        /* dry copy first: every stage is optional, and a bypassed chain must
         * be bit-identical to the input. */
        memcpy(s->scratch, in, nframes * sizeof(float));

        const float *lowp  = s->scratch;
        const float *highp = s->scratch;

        if (mask & ST_CROSSOVER) {
            sp11_xover_process(&s->xover[c], s->scratch,
                               s->low, s->high, (int)nframes);
            lowp  = s->low;
            highp = s->high;
        }

        if (mask & ST_VIRTUALBASS) {
            memcpy(s->vb, lowp, nframes * sizeof(float));
            { float *chp[1] = { s->vb };
              sp11_vb_process(&s->vb2, chp, (int)nframes, 0.5f); }
        } else {
            memcpy(s->vb, highp, nframes * sizeof(float));
        }

        /* stage 3 deliberately absent */

        if (mask & ST_SLIDINGBASS) {
            SlidingBass_Process(&s->sliding_bass[c], s->scratch,
                                (int)nframes, s->gains);
        } else {
            for (unsigned long i = 0; i < nframes; i++)
                s->gains[i] = 1.0f;
        }

        /* Mix weights follow bass_standalone.c's BassPipeline_Process.
         * These are NOT extracted constants - the shipped final mix has not
         * been recovered. Treat as provisional. */
        for (unsigned long i = 0; i < nframes; i++) {
            float wet;
            if (mask & ST_CROSSOVER) {
                /* Reconstruct the full band: the synthesised low band
                 * REPLACES the original low band, and the high band passes
                 * through unchanged. bass_standalone.c used 0.5/0.5/0.25,
                 * which are not extracted values and which attenuate and
                 * partially cancel. */
                wet = s->vb[i] + highp[i];
            } else {
                wet = s->vb[i];
            }
            wet *= s->gains[i];
            out[i] = (s->scratch[i] * (1.0f - mix) + wet * mix) * gain;
        }

        /* Never hand a non-finite buffer to the audio graph. An earlier
         * VLLDP build produced ~1e38 values; that must fail safe, not
         * reach the speakers. */
        if (!finite_buf(out, nframes)) {
            memcpy(out, in, nframes * sizeof(float));
            chain_init(s, vb_mode);
        }
    }
}

static void sp11_cleanup(LADSPA_Handle h)
{
    free(h);
}

static LADSPA_Descriptor *g_desc;

static const char *g_port_names[PORT_COUNT] = {
    "Input L", "Input R", "Output L", "Output R",
    "Stage mask", "Virtual bass mode", "Dry/wet", "Output gain"
};

static LADSPA_PortDescriptor g_ports[PORT_COUNT] = {
    LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
    LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
};

static LADSPA_PortRangeHint g_hints[PORT_COUNT] = {
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    { LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_INTEGER | LADSPA_HINT_DEFAULT_MAXIMUM, 0.0f, 15.0f },
    { LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_INTEGER | LADSPA_HINT_DEFAULT_MINIMUM, 0.0f, 3.0f },
    { LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_DEFAULT_MAXIMUM, 0.0f, 1.0f },
    { LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_DEFAULT_1, 0.0f, 4.0f },
};

__attribute__((constructor))
static void sp11_init(void)
{
    g_desc = calloc(1, sizeof(*g_desc));
    if (!g_desc)
        return;
    g_desc->UniqueID   = 0x5350B1D0;
    g_desc->Label      = strdup("sp11_dolby_modular");
    g_desc->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
    g_desc->Name       = strdup("SP11 Dolby modular bass chain");
    g_desc->Maker      = strdup("SP11 audio reconstruction");
    g_desc->Copyright  = strdup("None");
    g_desc->PortCount  = PORT_COUNT;
    g_desc->PortDescriptors = g_ports;
    g_desc->PortNames  = (const char * const *)g_port_names;
    g_desc->PortRangeHints = g_hints;
    g_desc->instantiate = sp11_instantiate;
    g_desc->connect_port = sp11_connect;
    g_desc->activate = NULL;
    g_desc->run = sp11_run;
    g_desc->run_adding = NULL;
    g_desc->set_run_adding_gain = NULL;
    g_desc->deactivate = NULL;
    g_desc->cleanup = sp11_cleanup;
}

__attribute__((destructor))
static void sp11_fini(void)
{
    if (!g_desc)
        return;
    free((char *)g_desc->Label);
    free((char *)g_desc->Name);
    free((char *)g_desc->Maker);
    free((char *)g_desc->Copyright);
    free(g_desc);
}

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index)
{
    return index == 0 ? g_desc : NULL;
}
