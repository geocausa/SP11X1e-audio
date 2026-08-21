#include "ubig/ubig.h"
#include "adapter256.h"
#include "profiles_internal.h"
#include "stage_a_core.h"
#include "stage_a_sp11_tuning.h"
#include <stdlib.h>
#include <string.h>

struct ubig_engine {
    ubig_profile profile;
    int32_t custom_eq[UBIG_EQ_BANDS];
    ubig_adapter256 stage_a_adapter;
    UbigStageACoreConfig stage_a_config;
    UbigStageACoreState stage_a_core;
    float stage_a_l[UBIG_INTERNAL_BLOCK];
    float stage_a_r[UBIG_INTERNAL_BLOCK];
    int process_status;
};

static void stage_a_exact(void *opaque,
                          const float in[UBIG_INTERNAL_BLOCK * UBIG_CHANNELS],
                          float out[UBIG_INTERNAL_BLOCK * UBIG_CHANNELS])
{
    ubig_engine *e=(ubig_engine*)opaque;
    if(e->process_status!=UBIG_OK){memset(out,0,UBIG_INTERNAL_BLOCK*UBIG_CHANNELS*sizeof(float));return;}
    for(unsigned i=0;i<UBIG_INTERNAL_BLOCK;i++){
        e->stage_a_l[i]=in[2u*i];
        e->stage_a_r[i]=in[2u*i+1u];
    }
    if(ubig_stage_a_core_process_256(&e->stage_a_core,&e->stage_a_config,
                                     e->stage_a_l,e->stage_a_r,
                                     e->stage_a_l,e->stage_a_r)!=0){
        e->process_status=UBIG_ESTATE;
        memset(out,0,UBIG_INTERNAL_BLOCK*UBIG_CHANNELS*sizeof(float));
        return;
    }
    for(unsigned i=0;i<UBIG_INTERNAL_BLOCK;i++){
        out[2u*i]=e->stage_a_l[i];
        out[2u*i+1u]=e->stage_a_r[i];
    }
}

ubig_engine *ubig_engine_create(const ubig_engine_config *cfg)
{
    if (!cfg || cfg->abi_version != UBIG_ABI_VERSION ||
        cfg->sample_rate != UBIG_SAMPLE_RATE || cfg->channels != UBIG_CHANNELS ||
        cfg->initial_profile < 0 || cfg->initial_profile >= UBIG_PROFILE_COUNT ||
        ubig_profile_uses_alternate_first_stage(cfg->initial_profile))
        return NULL;

    ubig_engine *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->profile = cfg->initial_profile;
    e->process_status=UBIG_OK;
    ubig_adapter256_reset(&e->stage_a_adapter);
    ubig_stage_a_sp11_dynamic_config(&e->stage_a_config);
    if(ubig_stage_a_core_init(&e->stage_a_core,&e->stage_a_config)!=0){free(e);return NULL;}
    return e;
}

void ubig_engine_destroy(ubig_engine *e) { free(e); }

int ubig_engine_process(ubig_engine *e,
                        const float *in_l, const float *in_r,
                        float *out_l, float *out_r, size_t frames)
{
    if (!e || !in_l || !in_r || !out_l || !out_r) return UBIG_EINVAL;
    e->process_status=UBIG_OK;
    ubig_adapter256_process(&e->stage_a_adapter, stage_a_exact, e,
                            in_l, in_r, out_l, out_r, frames);
    return e->process_status;
}

int ubig_engine_set_profile(ubig_engine *e, ubig_profile p)
{
    if (!e || p < 0 || p >= UBIG_PROFILE_COUNT) return UBIG_EINVAL;
    if(ubig_profile_uses_alternate_first_stage(p))return UBIG_EUNSUPPORTED;
    /* Same first-stage family: retune is downstream and must not reset Stage A. */
    e->profile = p;
    return UBIG_OK;
}

int ubig_engine_set_custom_eq(ubig_engine *e, const int32_t v[UBIG_EQ_BANDS])
{
    if (!e || !v) return UBIG_EINVAL;
    for (unsigned i = 0; i < UBIG_EQ_BANDS; ++i)
        if (v[i] < -192 || v[i] > 192) return UBIG_EINVAL;
    memcpy(e->custom_eq, v, sizeof(e->custom_eq));
    return UBIG_OK;
}

ubig_profile ubig_engine_profile(const ubig_engine *e)
{
    return e ? e->profile : UBIG_PROFILE_COUNT;
}
