#include "stage_a_core_helpers.h"
#include "stage_a_math.h"
#include <math.h>
#include <string.h>

void ubig_stage_a_build_rows(uint32_t channels,
                             uint32_t analysis_enable,
                             const struct ubig_float_rows *analyzed,
                             const struct ubig_float_rows *base,
                             const struct ubig_float_rows *main_rows,
                             const struct ubig_float_rows *companion_rows)
{
    if(!analyzed||!base||!main_rows||!companion_rows)return;
    uint32_t n=channels;
    if(n>analyzed->count)n=analyzed->count;
    if(n>base->count)n=base->count;
    if(n>main_rows->count)n=main_rows->count;
    if(n>companion_rows->count)n=companion_rows->count;
    for(uint32_t ch=0;ch<n;ch++){
        const float *a=analyzed->rows[ch],*b=base->rows[ch];
        float *m=main_rows->rows[ch],*c=companion_rows->rows[ch];
        for(uint32_t band=0;band<20u;band++){
            const float v=b[band];
            c[band]=v;
            m[band]=analysis_enable ? a[band]+v : v;
        }
    }
}

void ubig_stage_a_store_phase_gains(float gains[40],
                                    uint32_t phase_index,
                                    const float log_gain20[20])
{
    if(!gains||!log_gain20)return;
    const uint32_t phase=phase_index&1u;
    ubig_stage_a_exp2_scaled(gains+phase*20u,log_gain20,20u,21.5927734375f);
}

void ubig_stage_a_startup_blend_self(float samples[256], uint32_t block_index)
{
    if(!samples || block_index>=6u)return;
    float k; const uint32_t kb=0x3a2ac721u; memcpy(&k,&kb,4);
    const uint32_t base=block_index*256u;
    for(uint32_t i=0;i<256u;i++){
        const float x=samples[i];
        const float t=(float)(base+i)*k;
        const float old_term=x*t;
        const float keep=1.0f-t;
        samples[i]=fmaf(keep,x,old_term);
    }
}
