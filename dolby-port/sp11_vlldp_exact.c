/* sp11_vlldp_exact.c - Exact Windows VLLDP dynamic processor as a LADSPA plugin.
 *
 * Not an approximation. Every DSP stage is a direct call into the real Windows
 * ARM64 DolbyAPOvlldp150.dll, mapped by the verified PE loader. Per-block
 * orchestration is lifted verbatim from the proven sp11_full_chain_core_test.c
 * (outputs/267,268,270,281).
 *
 * Per 256-sample block, through one persistent 0x40 state object per channel:
 *   FUN_180023DB0 analyzer  -> 20-band energy (writes band ring, advances tap@0x10)
 *   phase6 base/B-array add
 *   FUN_180021E80 leveler   -> immediate 20-band feedback/export vector (b60)
 *   FUN_18001DE90 smoother  -> bbc/c0c feedback/export state (exact C helper)
 *   FUN_180023D20 gain convert -> B/local_2e0 synthesis gains for current phase
 *   FUN_1800240E0 synthesis -> output samples
 *
 * outputs/281: leveler forgets init state and adapts to the signal; cold and
 * seeded init converge to the same steady state. So cold-init via FUN_180021DA8
 * is correct for a live plugin. Phase 11 uses the B array, not the smoother
 * output, for synthesis gains. MSIIR stays neutral. No virtual bass, no tuning.
 *
 * Build:
 *   gcc -O2 -march=native -shared -fPIC -o ~/.local/lib/ladspa/sp11_vlldp_exact.so \
 *       sp11_vlldp_exact.c -lm
 */
#include "sp11_vlldp_pe_loader.h"
#include "sp11_vlldp_init_pack.h"
#include "sp11_vlldp_v8_runtime_contract.h"
#include "sp11_vlldp_fun18001de90.h"
#include <ladspa.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ANALYZER_VA    0x180023DB0ULL
#define LEVELER_VA     0x180021E80ULL
#define SYNTHESIS_VA   0x1800240E0ULL
#define STATE_INIT_VA  0x180021DA8ULL
#define TABLE_48K_256  0x180116C40ULL
#define SYNTH_DESC_VA  0x180116A20ULL
#define ANALYZER_CB_VA 0x18014D390ULL

#define CHANNELS 2
#define FRAME 256
#define BANDS 20
#define PHASE_COUNT 2
#define MATRIX_SPAN 640
#define WINDOW_FLOATS 128
#define BAND_FLOATS (PHASE_COUNT * MATRIX_SPAN)
#define GAIN_FLOATS 128
#define OVERLAP_FLOATS MATRIX_SPAN
#define OPT_STATE_SIZE 0x900
#define ANA_SCRATCH 8192
#define SYN_SCRATCH 4096

#define DLL_PATH "/home/geoca/Documents/SP11-PROJECT/01-audio/dolby-port/dll/DolbyAPOvlldp150.dll"

typedef void (*AnalyzerFn)(float, void*, void*, void*, void*, void*);
typedef void (*LevelerFn)(void*, void*, void*, void*, void*, void*, void*,
                          uint32_t, float, float, float, void*, void*, void*, void*);
typedef uint64_t (*SynthFn)(void*, void*, void*, void*, void*);
typedef void* (*InitFn)(void*, void*, void*);

enum { P_IN_L, P_IN_R, P_OUT_L, P_OUT_R, P_BYPASS, P_COUNT };

typedef struct {
    uint8_t state[0x40];
    float window[WINDOW_FLOATS];
    float band[BAND_FLOATS];
    float overlap[OVERLAP_FLOATS];
    float gains[GAIN_FLOATS];
    float analyzed[BANDS];
    float input[FRAME];
    float output[FRAME];
} Channel;

typedef struct {
    LADSPA_Data *ports[P_COUNT];
    unsigned long rate;
    Sp11PeImage img;
    int img_ok;
    AnalyzerFn analyzer;
    LevelerFn leveler;
    SynthFn synth;
    void *analyzer_desc;
    void *synth_desc;
    uint8_t opt_state[OPT_STATE_SIZE];
    Channel ch[CHANNELS];
    float side_a[BANDS], side_b[BANDS], runtime[5], p12[BANDS];
    int32_t mask[BANDS];
    float previous_bbc[BANDS];
    float base0[BANDS], base1[BANDS];
    float analyzer_scratch[ANA_SCRATCH];
    float synth_scratch[SYN_SCRATCH];
    /* per-frame input staging (process_frame reads these) */
    float in_l[FRAME], in_r[FRAME];
    /* input accumulator (one frame) */
    float acc_l[FRAME], acc_r[FRAME];
    int   acc_n;
    /* output FIFO (holds up to 2 frames) */
    float fifo_l[2*FRAME], fifo_r[2*FRAME];
    int   fifo_head, fifo_tail;   /* [head,tail) valid; tail-head = count */
} SP11;

static inline void wr_u64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline uint32_t f2u(float v){ uint32_t u; memcpy(&u,&v,4); return u; }
static inline float u2f(uint32_t u){ float v; memcpy(&v,&u,4); return v; }

static void load_captured_phase6_b_rows(float *left, float *right)
{
    for (int b = 0; b < BANDS; b++) {
        left[b] = sp11_vlldp_v8_phase6_output_ch0[b];
        right[b] = sp11_vlldp_v8_phase6_output_ch1[b];
    }
}

/* exact FUN_180023D20 gain conversion (verbatim from sp11_full_chain_core_test.c) */
static float gain_convert_one(float value)
{
    const float c3 = u2f(0x3D714000U);
    const float c2 = u2f(0x3E827800U);
    const float c1 = u2f(0x3F2FB000U);
    const float scale = 21.5927734375f;
    float scaled = value * scale;
    int truncated = (int)scaled;
    float truncated_f32 = (float)truncated;
    float correction = truncated_f32 > scaled ? 1.0f : 0.0f;
    float floored = truncated_f32 - correction;
    float fraction = scaled - floored;
    int32_t fixed_bits = (int32_t)((int)floored << 23);
    float fraction2 = fraction * fraction;
    float fraction3 = fraction2 * fraction;
    float polynomial = 1.0f + fraction * c1 + fraction2 * c2 + fraction3 * c3;
    return u2f((f2u(polynomial) + (uint32_t)fixed_bits) & 0xFFFFFFFFU);
}

static void channel_bind_state(Channel *c, const Sp11PeImage *img)
{
    memset(c->state, 0, sizeof c->state);
    wr_u64(c->state + 0x00, (uint64_t)(uintptr_t)c->window);
    wr_u64(c->state + 0x08, (uint64_t)(uintptr_t)sp11_pe_ptr_for_va(img, ANALYZER_CB_VA));
    wr_u64(c->state + 0x18, (uint64_t)(uintptr_t)c->band);
    wr_u64(c->state + 0x20, (uint64_t)(uintptr_t)c->overlap);
    wr_u64(c->state + 0x28, (uint64_t)(uintptr_t)c->gains);
}

static void reset_runtime(SP11 *p)
{
    for (int c = 0; c < CHANNELS; c++) {
        memset(p->ch[c].window, 0, sizeof p->ch[c].window);
        memset(p->ch[c].band, 0, sizeof p->ch[c].band);
        memset(p->ch[c].overlap, 0, sizeof p->ch[c].overlap);
        for (int i = 0; i < GAIN_FLOATS; i++)
            p->ch[c].gains[i] = 1.0f;
        memset(p->ch[c].analyzed, 0, sizeof p->ch[c].analyzed);
        channel_bind_state(&p->ch[c], &p->img);
    }
    for (int b = 0; b < BANDS; b++)
        p->previous_bbc[b] = sp11_vlldp_v8_child1_bbc_previous[b];
    p->acc_n = 0;
    p->fifo_head = 0;
    p->fifo_tail = 0;
    if (p->img_ok) {
        uint64_t tbl[8];
        for (int i = 0; i < 8; i++)
            tbl[i] = sp11_pe_read_u64_va(&p->img, TABLE_48K_256 + (uint64_t)i * 8);
        InitFn init = (InitFn)sp11_pe_ptr_for_va(&p->img, STATE_INIT_VA);
        init((void *)(uintptr_t)tbl[2], (void *)(uintptr_t)tbl[6], p->opt_state);
    }
}

static LADSPA_Handle sp11_instantiate(const LADSPA_Descriptor *d, unsigned long rate)
{
    (void)d;
    SP11 *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->rate = rate;
    int load_rc = sp11_pe_load(&p->img, DLL_PATH);
    fprintf(stderr, "sp11_vlldp_exact instantiate rate=%lu dll=%s load_rc=%d\n",
            rate, DLL_PATH, load_rc);
    fflush(stderr);
    if (load_rc == 0) {
        p->img_ok = 1;
        p->analyzer = (AnalyzerFn)sp11_pe_ptr_for_va(&p->img, ANALYZER_VA);
        p->leveler  = (LevelerFn)sp11_pe_ptr_for_va(&p->img, LEVELER_VA);
        p->synth    = (SynthFn)sp11_pe_ptr_for_va(&p->img, SYNTHESIS_VA);
        p->analyzer_desc = (void *)(uintptr_t)sp11_pe_read_u64_va(&p->img, TABLE_48K_256);
        p->synth_desc = sp11_pe_ptr_for_va(&p->img, SYNTH_DESC_VA);
        fprintf(stderr, "sp11_vlldp_exact mapped_base=%p image_size=%zu\n",
                (void *)p->img.base, p->img.size);
        fflush(stderr);
    }
    for (int b = 0; b < BANDS; b++) {
        p->side_a[b] = (float)sp11_vlldp_threshold_low[b] / 2080.0f;
        p->side_b[b] = (float)sp11_vlldp_threshold_high[b] / 2080.0f;
        p->mask[b]   = sp11_vlldp_isolated_bands[b];
        p->p12[b]    = (b < SP11_VLLDP_STRESS_VALUES) ? (float)sp11_vlldp_stress_amount[b] / 2080.0f : 0.0f;
    }
    load_captured_phase6_b_rows(p->base0, p->base1);
    p->runtime[0] = 0.75f;
    p->runtime[1] = -0.26201921701431274f;
    p->runtime[2] = 0.0f; p->runtime[3] = 0.0f; p->runtime[4] = 0.0f;
    reset_runtime(p);
    return p;
}

static void sp11_connect(LADSPA_Handle h, unsigned long port, LADSPA_Data *data)
{
    SP11 *p = h;
    if (port < P_COUNT) p->ports[port] = data;
}

static void sp11_activate(LADSPA_Handle h) { reset_runtime((SP11 *)h); }

/* process one full stereo frame: in_l/in_r -> out_l/out_r */
static void process_frame(SP11 *p)
{
    if (!p->img_ok) {
        for (int i = 0; i < FRAME; i++) {
            p->fifo_l[p->fifo_tail] = p->in_l[i];
            p->fifo_r[p->fifo_tail] = p->in_r[i];
            p->fifo_tail++;
        }
        return;
    }
    for (int i = 0; i < FRAME; i++) {
        p->ch[0].input[i] = p->in_l[i];
        p->ch[1].input[i] = p->in_r[i];
    }
    for (int c = 0; c < CHANNELS; c++) {
        memset(p->analyzer_scratch, 0, sizeof p->analyzer_scratch);
        p->analyzer(1.1111111640930176f, p->ch[c].state, p->analyzer_desc,
                    p->ch[c].input, p->ch[c].analyzed, p->analyzer_scratch);
    }
    float phase6_in0[BANDS], phase6_in1[BANDS], phase6_b0[BANDS], phase6_b1[BANDS];
    for (int b = 0; b < BANDS; b++) {
        phase6_in0[b] = p->ch[0].analyzed[b] + p->base0[b];
        phase6_in1[b] = p->ch[1].analyzed[b] + p->base1[b];
        phase6_b0[b] = p->base0[b];
        phase6_b1[b] = p->base1[b];
    }
    uint64_t in_ptrs[CHANNELS]  = { (uint64_t)(uintptr_t)phase6_in0,  (uint64_t)(uintptr_t)phase6_in1 };
    uint64_t out_ptrs[CHANNELS] = { (uint64_t)(uintptr_t)phase6_b0, (uint64_t)(uintptr_t)phase6_b1 };
    struct { uint32_t count; uint32_t pad; uint64_t ptrs; } in_desc = { CHANNELS, 0, (uint64_t)(uintptr_t)in_ptrs };
    struct { uint32_t count; uint32_t pad; uint64_t ptrs; } out_desc = { CHANNELS, 0, (uint64_t)(uintptr_t)out_ptrs };
    int32_t b60[BANDS] = {0};
    int32_t side_scratch[8 * BANDS] = {0};
    int32_t export_count[1] = {0};
    p->leveler(p->opt_state, p->side_a, p->side_b, p->mask, p->runtime,
               &in_desc, &out_desc, CHANNELS,
               0.0f, p->runtime[1], 0.0f, p->p12,
               b60, side_scratch, export_count);
    float b60_source[BANDS], bbc[BANDS]; int32_t c0c[BANDS];
    for (int b = 0; b < BANDS; b++)
        b60_source[b] = (float)b60[b] / 2080.0f;
    sp11_vlldp_fun18001de90_export(p->previous_bbc, b60_source, b60_source, bbc, c0c);
    for (int b = 0; b < BANDS; b++) p->previous_bbc[b] = bbc[b];

    /* Phase 11 converts local_2e0 (the Phase-6 B array) to synthesis gains.
       Use the immutable captured B rows from the runtime contract; the leveler
       call may write through the descriptor scratch, so phase6_b* is not reused. */
    for (int c = 0; c < CHANNELS; c++) {
        const float *bvec = c == 0 ? p->base0 : p->base1;
        int phase = *(int32_t *)(p->ch[c].state + 0x10) % PHASE_COUNT;
        for (int b = 0; b < BANDS; b++)
            p->ch[c].gains[phase * BANDS + b] = gain_convert_one(bvec[b]);
        memset(p->synth_scratch, 0, sizeof p->synth_scratch);
        p->synth(p->ch[c].state, p->synth_desc, NULL, p->ch[c].output, p->synth_scratch);
    }
    for (int i = 0; i < FRAME; i++) {
        float l = p->ch[0].output[i], r = p->ch[1].output[i];
        p->fifo_l[p->fifo_tail] = isfinite(l) ? l : 0.0f;
        p->fifo_r[p->fifo_tail] = isfinite(r) ? r : 0.0f;
        p->fifo_tail++;
    }
}

static void sp11_run(LADSPA_Handle h, unsigned long n)
{
    SP11 *p = h;
    const LADSPA_Data *in_l = p->ports[P_IN_L];
    const LADSPA_Data *in_r = p->ports[P_IN_R];
    LADSPA_Data *out_l = p->ports[P_OUT_L];
    LADSPA_Data *out_r = p->ports[P_OUT_R];
    if (!in_l || !in_r || !out_l || !out_r) return;

    int bypass = (p->ports[P_BYPASS] && *p->ports[P_BYPASS] > 0.5f) || !p->img_ok;
    if (bypass) {
        for (unsigned long i = 0; i < n; i++) { out_l[i] = in_l[i]; out_r[i] = in_r[i]; }
        return;
    }

    for (unsigned long i = 0; i < n; i++) {
        /* accumulate one input sample */
        p->acc_l[p->acc_n] = in_l[i];
        p->acc_r[p->acc_n] = in_r[i];
        p->acc_n++;
        if (p->acc_n == FRAME) {
            for (int k = 0; k < FRAME; k++) { p->in_l[k] = p->acc_l[k]; p->in_r[k] = p->acc_r[k]; }
            process_frame(p);          /* pushes FRAME samples onto FIFO (advances fifo_tail) */
            p->acc_n = 0;
        }
        /* emit one output sample from FIFO, or silence (one-frame startup latency) */
        if (p->fifo_head < p->fifo_tail) {
            out_l[i] = p->fifo_l[p->fifo_head];
            out_r[i] = p->fifo_r[p->fifo_head];
            p->fifo_head++;
            if (p->fifo_head == p->fifo_tail) { p->fifo_head = 0; p->fifo_tail = 0; } /* compact when empty */
        } else {
            out_l[i] = 0.0f;
            out_r[i] = 0.0f;
        }
    }
}

static void sp11_cleanup(LADSPA_Handle h)
{
    SP11 *p = h;
    if (p->img_ok) sp11_pe_unload(&p->img);
    free(p);
}

static LADSPA_PortDescriptor g_pdesc[P_COUNT];
static const char *g_pname[P_COUNT];
static LADSPA_PortRangeHint g_phint[P_COUNT];
static LADSPA_Descriptor g_desc;

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index)
{
    if (index != 0) return NULL;
    g_pdesc[P_IN_L]   = LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO;
    g_pdesc[P_IN_R]   = LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO;
    g_pdesc[P_OUT_L]  = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    g_pdesc[P_OUT_R]  = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    g_pdesc[P_BYPASS] = LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL;
    g_pname[P_IN_L]="Input L"; g_pname[P_IN_R]="Input R";
    g_pname[P_OUT_L]="Output L"; g_pname[P_OUT_R]="Output R";
    g_pname[P_BYPASS]="Bypass";
    for (int i=0;i<P_COUNT;i++) g_phint[i].HintDescriptor = 0;
    g_phint[P_BYPASS].HintDescriptor = LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_TOGGLED|LADSPA_HINT_DEFAULT_0;
    g_phint[P_BYPASS].LowerBound = 0.0f;
    g_phint[P_BYPASS].UpperBound = 1.0f;
    g_desc.UniqueID = 0x5350314CUL;
    g_desc.Label = "sp11_vlldp_exact";
    g_desc.Properties = 0;
    g_desc.Name = "SP11 VLLDP Exact (native Dolby bridge)";
    g_desc.Maker = "sp11 re project";
    g_desc.Copyright = "research use";
    g_desc.PortCount = P_COUNT;
    g_desc.PortDescriptors = g_pdesc;
    g_desc.PortNames = g_pname;
    g_desc.PortRangeHints = g_phint;
    g_desc.instantiate = sp11_instantiate;
    g_desc.connect_port = sp11_connect;
    g_desc.activate = sp11_activate;
    g_desc.run = sp11_run;
    g_desc.run_adding = NULL;
    g_desc.set_run_adding_gain = NULL;
    g_desc.deactivate = NULL;
    g_desc.cleanup = sp11_cleanup;
    return &g_desc;
}
