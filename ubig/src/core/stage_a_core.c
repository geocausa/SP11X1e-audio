#include "stage_a_core.h"
#include "stage_a_analyzer.h"
#include "stage_a_compressor.h"
#include "stage_a_core_helpers.h"
#include "stage_a_export.h"
#include "stage_a_fft320_sp11.h"
#include "stage_a_filterbank_sp11.h"
#include "stage_a_math.h"
#include "stage_a_synth.h"
#include <string.h>

static int valid_cfg(const UbigStageACoreConfig *c)
{
    if(!c||!c->base_rows[0]||!c->base_rows[1])return 0;
    if(c->compressor_enable && (!c->compressor_config||!c->compressor_distribution||
       !c->side_a||!c->side_b||!c->mask||!c->runtime5||!c->channel_mix))return 0;
    return 1;
}

int ubig_stage_a_core_init(UbigStageACoreState *s,const UbigStageACoreConfig *cfg)
{
    if(!s||!valid_cfg(cfg))return -1;
    memset(s,0,sizeof(*s));
    for(unsigned ch=0;ch<2u;ch++){s->channel[ch].phase_index=1u;for(unsigned i=0;i<40u;i++)s->channel[ch].gains[i]=1.0f;}
    ubig_stage_a_limiter_init(&s->limiter);
    s->limiter_feedback=1.0f;
    s->startup_blend_active=1u;
    if(cfg->compressor_enable){
        s->compressor_state=ubig_stage_a_compressor_init(cfg->compressor_config,
                                                         cfg->compressor_distribution,
                                                         s->compressor_storage);
        memcpy(s->compressor_state,&cfg->compressor_mode,4);
    }
    return 0;
}

int ubig_stage_a_core_process_256(UbigStageACoreState *s,const UbigStageACoreConfig *cfg,
                                  const float *left_in,const float *right_in,
                                  float *left_out,float *right_out)
{
    if(!s||!valid_cfg(cfg)||!left_in||!right_in||!left_out||!right_out)return -1;
    const UbigStageAAnalyzerDesc *ad=ubig_stage_a_sp11_analyzer_desc();
    const UbigStageASynthDesc *sd=ubig_stage_a_sp11_synth_desc();
    memcpy(s->analysis_input[0],left_in,UBIG_STAGE_A_CORE_FRAMES*sizeof(float));
    memcpy(s->analysis_input[1],right_in,UBIG_STAGE_A_CORE_FRAMES*sizeof(float));
    if(s->startup_blend_active){
        ubig_stage_a_startup_blend_self(s->analysis_input[0],s->startup_blend_index);
        ubig_stage_a_startup_blend_self(s->analysis_input[1],s->startup_blend_index);
        s->startup_blend_index++;
        if(s->startup_blend_index*UBIG_STAGE_A_CORE_FRAMES==1536u){
            s->startup_blend_index=0u;
            s->startup_blend_active=0u;
        }
    }
    const float *input[2]={s->analysis_input[0],s->analysis_input[1]};
    for(unsigned ch=0;ch<2u;ch++){
        UbigStageAAnalyzerState as={s->channel[ch].phase_index,s->channel[ch].history,s->channel[ch].band_data};
        int rc=ubig_stage_a_analyzer_process(cfg->input_scale,&as,ad,input[ch],s->analyzed[ch],
                                             s->scratch,4u*UBIG_STAGE_A_CORE_TRANSFORM,
                                             ubig_stage_a_fft320_sp11_norm320,0);
        if(rc)return -2;
        s->channel[ch].phase_index=as.phase_index;
    }

    float *ap[2]={s->analyzed[0],s->analyzed[1]};
    float *bp[2]={(float*)cfg->base_rows[0],(float*)cfg->base_rows[1]};
    float *mp[2]={s->main_rows[0],s->main_rows[1]};
    float *cp[2]={s->companion_rows[0],s->companion_rows[1]};
    struct ubig_float_rows ar={2u,20u,ap},br={2u,20u,bp},mr={2u,20u,mp},cr={2u,20u,cp};
    ubig_stage_a_build_rows(2u,cfg->analysis_enable,&ar,&br,&mr,&cr);

    if(cfg->compressor_enable){
        const float common=ubig_stage_a_log2_approx(s->limiter_feedback)*0x1.7b63f2p-5f;
        int rc=ubig_stage_a_compressor_process(s->compressor_state,cfg->side_a,cfg->side_b,
                                               cfg->mask,cfg->runtime5,&mr,&cr,cfg->native_count,
                                               cfg->drive_state,common,cfg->controller_drive,
                                               cfg->channel_mix,s->band_gain_info,s->matrix_info,
                                               &s->matrix_rows);
        if(rc)return -3;
    }

    ubig_stage_a_export(s->export_state,&s->main_rows[0][0],2u,s->export_state,s->export_raw);

    float *output[2]={left_out,right_out};
    for(unsigned ch=0;ch<2u;ch++){
        UbigStageAChannelState *cs=&s->channel[ch];
        ubig_stage_a_store_phase_gains(cs->gains,cs->phase_index,s->companion_rows[ch]);
        UbigStageASynthState ss={cs->phase_index,cs->band_data,cs->overlap,cs->gains};
        int rc=ubig_stage_a_synth_process(&ss,sd,output[ch],s->scratch,
                                          4u*UBIG_STAGE_A_CORE_TRANSFORM,
                                          ubig_stage_a_fft320_sp11,0);
        if(rc)return -4;
    }
    s->limiter_feedback=ubig_stage_a_limiter_process_256_feedback(&s->limiter,cfg->limiter_ceiling,
                                                                   left_out,right_out);
    return 0;
}
