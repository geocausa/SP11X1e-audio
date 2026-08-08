/*
 * sp11_vlldp_orchestrator_ladspa.c
 *
 * SP11 VLLDP150 Linux bridge using the original Windows ARM64 Dolby core.
 *
 * Proven live Windows path (2026-08-04 KDNET):
 *   480-frame host blocks -> inner 256-frame accumulator (RVA 0x33640)
 *   -> descriptor shim -> FUN_18001f7a8 orchestrator.
 *
 * This plugin calls the original FUN_18001f7a8 directly and reproduces only
 * the small, live-decoded accumulator around it.  State construction and the
 * active speaker PID 5/17/22/31 setup are also executed by the original DLL.
 */
#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <ladspa.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VLLDP_CTOR_VA   0x18001BFB0ULL
#define VLLDP_PID5_VA   0x18001BC80ULL
#define VLLDP_PID17_VA  0x18001CDD0ULL
#define VLLDP_PID22_VA  0x18001E5C8ULL
#define VLLDP_PID31_VA  0x18001EE68ULL
#define VLLDP_APPLY_VA  0x18001D280ULL
#define VLLDP_ORCH_VA   0x18001F7A8ULL

#define VLLDP_BLOCK       256U
#define VLLDP_CHANNELS      2U
#define VLLDP_RATE      48000U
#define VLLDP_ARENA_SIZE 0x20000U
#define VLLDP_AUX_OWNER_OFF 0x0CA0U
#define VLLDP_AUX_INNER_OFF 0x0008U

#ifndef SP11_VLLDP_DEFAULT_DLL
#define SP11_VLLDP_DEFAULT_DLL "/usr/lib/sp11-dolby/DolbyAPOvlldp150.dll"
#endif

typedef void *(*VlldpCtorFn)(uint32_t block, uint32_t rate, uint32_t channels,
                              uint32_t slot_count, void *arena);
typedef void (*VlldpPid5Fn)(void *state, const uint32_t *blob);
typedef void (*VlldpPid17Fn)(void *state, uint32_t count,
                              const int32_t *const *groups);
typedef void (*VlldpPid22Fn)(void *state, uint32_t count, const int32_t *values);
typedef void (*VlldpPid31Fn)(void *state, const int32_t *values);
typedef void (*VlldpApplyFn)(void *state, uint32_t channels);
typedef void (*VlldpOrchFn)(void *state, void *in_desc, void *out_desc, void *aux);

typedef struct {
    uint64_t channels;
    uint64_t stride;
    uint64_t format;
    uint64_t planes;
} VlldpAudioDesc;

enum {
    PORT_IN_L,
    PORT_IN_R,
    PORT_OUT_L,
    PORT_OUT_R,
    PORT_BYPASS,
    PORT_COUNT
};

typedef struct {
    LADSPA_Data *ports[PORT_COUNT];
    unsigned long rate;

    Sp11PeImage img;
    int img_loaded;
    int core_ready;

    void *arena;
    uint8_t *state;
    void *aux;

    VlldpCtorFn ctor;
    VlldpPid5Fn pid5;
    VlldpPid17Fn pid17;
    VlldpPid22Fn pid22;
    VlldpPid31Fn pid31;
    VlldpApplyFn apply;
    VlldpOrchFn orch;

    /* Exact FUN_180033640-equivalent persistent 256-frame adapter state. */
    uint32_t fill;
    float block_in[VLLDP_BLOCK * VLLDP_CHANNELS];
    float block_out[VLLDP_BLOCK * VLLDP_CHANNELS];
    int was_bypassed;
} Sp11Vlldp;

static uint64_t rd_u64(const void *p)
{
    uint64_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

/* Windows CRITICAL_SECTION imports used by the DLL.  The LADSPA instance is
 * single-thread-owned by the audio graph, so these are safe no-op shims for
 * this bridge.  No Dolby DSP routine is replaced. */
static void win_lock_noop(void *p) { (void)p; }
static int win_lock_true(void *p) { (void)p; return 1; }
static int win_init_ex_true(void *p, unsigned spin, unsigned flags)
{
    (void)p; (void)spin; (void)flags; return 1;
}

static void patch_iat(Sp11PeImage *img, uint64_t va, void *fn)
{
    uintptr_t *slot = (uintptr_t *)sp11_pe_ptr_for_va(img, va);
    *slot = (uintptr_t)fn;
}

static void patch_windows_runtime(Sp11PeImage *img)
{
    patch_iat(img, 0x1801070E0ULL, (void *)win_lock_noop);   /* LeaveCriticalSection */
    patch_iat(img, 0x1801070E8ULL, (void *)win_lock_noop);   /* EnterCriticalSection */
    patch_iat(img, 0x180107190ULL, (void *)win_lock_noop);   /* InitializeCriticalSection */
    patch_iat(img, 0x180107198ULL, (void *)win_lock_true);   /* TryEnterCriticalSection */
    patch_iat(img, 0x180107248ULL, (void *)win_lock_noop);   /* DeleteCriticalSection */
    patch_iat(img, 0x180107250ULL, (void *)win_init_ex_true);/* InitializeCriticalSectionEx */
}

static const char *dll_path(void)
{
    const char *p = getenv("SP11_VLLDP_DLL");
    return (p && *p) ? p : SP11_VLLDP_DEFAULT_DLL;
}

static int configure_active_speaker(Sp11Vlldp *p)
{
    /* August live-state readback exactly matches the June active-speaker
     * persisted values for these controls. */
    uint32_t empty_filter_blob[2] = { 0, 0 };
    int32_t compressor_group0[6] = { 20, 0, 32767, 10, 20, 0 };
    const int32_t *compressor_groups[1] = { compressor_group0 };
    int32_t stress[8] = { 216, 216, 0, 0, 0, 0, 0, 0 };
    int32_t bass_curve[5] = { 0, 0, 0, 0, 0 };

    p->pid5(p->state, empty_filter_blob);
    p->pid17(p->state, 1, compressor_groups);
    p->pid22(p->state, 8, stress);
    p->pid31(p->state, bass_curve);
    p->apply(p->state, VLLDP_CHANNELS);

    /* PID5 stages a small object at state+0xCA0; the live orchestrator's x3
     * points eight bytes into it.  Constructor geometry matched this exactly
     * against the 2026-08-04 fresh Windows graph. */
    uint64_t aux_owner = rd_u64(p->state + VLLDP_AUX_OWNER_OFF);
    if (!aux_owner)
        return -1;
    p->aux = (void *)(uintptr_t)(aux_owner + VLLDP_AUX_INNER_OFF);
    return 0;
}

static int reset_core(Sp11Vlldp *p)
{
    if (!p->img_loaded || !p->arena)
        return -1;

    memset(p->arena, 0, VLLDP_ARENA_SIZE);
    p->state = (uint8_t *)p->ctor(VLLDP_BLOCK, VLLDP_RATE,
                                   VLLDP_CHANNELS, 0, p->arena);
    if (!p->state)
        return -2;
    if (configure_active_speaker(p) != 0)
        return -3;

    p->fill = 0;
    memset(p->block_in, 0, sizeof(p->block_in));
    memset(p->block_out, 0, sizeof(p->block_out));
    p->core_ready = 1;
    return 0;
}

static void process_full_internal_block(Sp11Vlldp *p)
{
    uint64_t in_planes[2] = {
        (uint64_t)(uintptr_t)&p->block_in[0],
        (uint64_t)(uintptr_t)&p->block_in[1]
    };
    uint64_t out_planes[2] = {
        (uint64_t)(uintptr_t)&p->block_out[0],
        (uint64_t)(uintptr_t)&p->block_out[1]
    };
    VlldpAudioDesc in_desc = { 2, 2, 7, (uint64_t)(uintptr_t)in_planes };
    VlldpAudioDesc out_desc = { 2, 2, 7, (uint64_t)(uintptr_t)out_planes };
    p->orch(p->state, &in_desc, &out_desc, p->aux);
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor *descriptor,
                                  unsigned long sample_rate)
{
    (void)descriptor;
    if (sample_rate != VLLDP_RATE) {
        fprintf(stderr, "sp11_vlldp_orchestrator: requires 48000 Hz, got %lu\n",
                sample_rate);
        return NULL;
    }

    Sp11Vlldp *p = (Sp11Vlldp *)calloc(1, sizeof(*p));
    if (!p)
        return NULL;
    p->rate = sample_rate;

    const char *path = dll_path();
    int rc = sp11_pe_load(&p->img, path);
    if (rc != 0) {
        fprintf(stderr, "sp11_vlldp_orchestrator: DLL load failed rc=%d path=%s\n",
                rc, path);
        free(p);
        return NULL;
    }
    p->img_loaded = 1;
    patch_windows_runtime(&p->img);

    p->ctor  = (VlldpCtorFn)sp11_pe_ptr_for_va(&p->img, VLLDP_CTOR_VA);
    p->pid5  = (VlldpPid5Fn)sp11_pe_ptr_for_va(&p->img, VLLDP_PID5_VA);
    p->pid17 = (VlldpPid17Fn)sp11_pe_ptr_for_va(&p->img, VLLDP_PID17_VA);
    p->pid22 = (VlldpPid22Fn)sp11_pe_ptr_for_va(&p->img, VLLDP_PID22_VA);
    p->pid31 = (VlldpPid31Fn)sp11_pe_ptr_for_va(&p->img, VLLDP_PID31_VA);
    p->apply = (VlldpApplyFn)sp11_pe_ptr_for_va(&p->img, VLLDP_APPLY_VA);
    p->orch  = (VlldpOrchFn)sp11_pe_ptr_for_va(&p->img, VLLDP_ORCH_VA);

    if (posix_memalign(&p->arena, 64, VLLDP_ARENA_SIZE) != 0) {
        sp11_pe_unload(&p->img);
        free(p);
        return NULL;
    }

    if (reset_core(p) != 0) {
        fprintf(stderr, "sp11_vlldp_orchestrator: core initialization failed\n");
        free(p->arena);
        sp11_pe_unload(&p->img);
        free(p);
        return NULL;
    }
    return (LADSPA_Handle)p;
}

static void connect_port(LADSPA_Handle instance, unsigned long port,
                         LADSPA_Data *data)
{
    Sp11Vlldp *p = (Sp11Vlldp *)instance;
    if (port < PORT_COUNT)
        p->ports[port] = data;
}

static void activate(LADSPA_Handle instance)
{
    Sp11Vlldp *p = (Sp11Vlldp *)instance;
    if (reset_core(p) != 0)
        p->core_ready = 0;
    p->was_bypassed = 0;
}

static void run(LADSPA_Handle instance, unsigned long n)
{
    Sp11Vlldp *p = (Sp11Vlldp *)instance;
    const float *in_l = p->ports[PORT_IN_L];
    const float *in_r = p->ports[PORT_IN_R];
    float *out_l = p->ports[PORT_OUT_L];
    float *out_r = p->ports[PORT_OUT_R];
    if (!in_l || !in_r || !out_l || !out_r)
        return;

    int bypass = !p->core_ready ||
        (p->ports[PORT_BYPASS] && *p->ports[PORT_BYPASS] > 0.5f);
    if (bypass) {
        for (unsigned long i = 0; i < n; ++i) {
            out_l[i] = in_l[i];
            out_r[i] = in_r[i];
        }
        p->was_bypassed = 1;
        return;
    }

    /* Avoid replaying stale pre-bypass history when the effect is re-enabled. */
    if (p->was_bypassed) {
        if (reset_core(p) != 0) {
            p->core_ready = 0;
            for (unsigned long i = 0; i < n; ++i) {
                out_l[i] = in_l[i]; out_r[i] = in_r[i];
            }
            return;
        }
        p->was_bypassed = 0;
    }

    /* Live-decoded FUN_180033640 accumulator semantics.  A full block is
     * processed at the top of the next iteration; therefore the direct path
     * has exactly one 256-frame startup/history block of latency. */
    unsigned long pos = 0;
    while (pos < n) {
        if (p->fill >= VLLDP_BLOCK) {
            process_full_internal_block(p);
            p->fill = 0;
        }

        uint32_t space = VLLDP_BLOCK - p->fill;
        unsigned long remain = n - pos;
        uint32_t take = remain < space ? (uint32_t)remain : space;

        for (uint32_t j = 0; j < take; ++j) {
            uint32_t f = p->fill + j;
            p->block_in[2 * f]     = in_l[pos + j];
            p->block_in[2 * f + 1] = in_r[pos + j];
            out_l[pos + j] = p->block_out[2 * f];
            out_r[pos + j] = p->block_out[2 * f + 1];
        }
        p->fill += take;
        pos += take;
    }
}

static void cleanup(LADSPA_Handle instance)
{
    Sp11Vlldp *p = (Sp11Vlldp *)instance;
    if (!p) return;
    free(p->arena);
    if (p->img_loaded)
        sp11_pe_unload(&p->img);
    free(p);
}

static LADSPA_PortDescriptor port_desc[PORT_COUNT];
static const char *port_names[PORT_COUNT];
static LADSPA_PortRangeHint port_hints[PORT_COUNT];
static LADSPA_Descriptor descriptor;

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index)
{
    if (index != 0)
        return NULL;

    port_desc[PORT_IN_L]  = LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO;
    port_desc[PORT_IN_R]  = LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO;
    port_desc[PORT_OUT_L] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    port_desc[PORT_OUT_R] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    port_desc[PORT_BYPASS]= LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL;

    port_names[PORT_IN_L] = "Input L";
    port_names[PORT_IN_R] = "Input R";
    port_names[PORT_OUT_L]= "Output L";
    port_names[PORT_OUT_R]= "Output R";
    port_names[PORT_BYPASS]= "Bypass";

    memset(port_hints, 0, sizeof(port_hints));
    port_hints[PORT_BYPASS].HintDescriptor =
        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE |
        LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0;
    port_hints[PORT_BYPASS].LowerBound = 0.0f;
    port_hints[PORT_BYPASS].UpperBound = 1.0f;

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.UniqueID = 0x5350314FULL;
    descriptor.Label = "sp11_vlldp_orchestrator";
    descriptor.Name = "SP11 VLLDP Original Windows Orchestrator";
    descriptor.Maker = "sp11 re project";
    descriptor.Copyright = "research bridge; original DLL supplied separately";
    descriptor.PortCount = PORT_COUNT;
    descriptor.PortDescriptors = port_desc;
    descriptor.PortNames = port_names;
    descriptor.PortRangeHints = port_hints;
    descriptor.instantiate = instantiate;
    descriptor.connect_port = connect_port;
    descriptor.activate = activate;
    descriptor.run = run;
    descriptor.cleanup = cleanup;
    return &descriptor;
}
