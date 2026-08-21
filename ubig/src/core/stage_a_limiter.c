#include "stage_a_limiter.h"
#include <math.h>
#include <string.h>

/* DECODED Stage-A limiter contract. Numeric constants are recorded by exact
   float32 bit pattern so this implementation does not depend on decimal
   parser rounding. */
static float f32_bits(uint32_t u){float f;memcpy(&f,&u,sizeof(f));return f;}

static const uint32_t predictor_weight_bits[UBIG_A_LIMITER_HISTORY]={
    0x00000000u,0x3c9d6830u,0x3d1a61e8u,0x3d6020ebu,
    0x3d8ea17bu,0x3da7b750u,0x3dba5b2au,0x3dc5d5a7u,
    0x3dc9b5dcu,0x3dc5d5a7u,0x3dba5b2au,0x3da7b750u,
    0x3d8ea17bu,0x3d6020ebu,0x3d1a61e8u,0x3c9d6830u
};

#define COEFF_PRIMARY_RISE_BITS 0x3f7f4a35u
#define COEFF_PRIMARY_FALL_BITS 0x3f7ff259u
#define COEFF_SECONDARY_BITS    0x3f7de023u

void ubig_stage_a_limiter_init(ubig_stage_a_limiter *s)
{
    if(!s)return;
    memset(s,0,sizeof(*s));
    s->current_gain=1.0f;
    s->previous_gain=1.0f;
    s->target_gain=1.0f;
}

static float linked_peak(float l,float r)
{
    float al=fabsf(l),ar=fabsf(r);
    return al<ar?ar:al;
}

static float ring_max16(const float *v)
{
    float m=0.0f;
    for(unsigned i=0;i<UBIG_A_LIMITER_HISTORY;i++) if(m<v[i])m=v[i];
    return m;
}

static float predictor(const ubig_stage_a_limiter *s)
{
    float acc=0.0f;
    uint32_t idx=s->history_pos+1u;
    for(unsigned i=0;i<UBIG_A_LIMITER_HISTORY;i++,idx++)
        acc=fmaf(s->predictor_history[idx&(UBIG_A_LIMITER_HISTORY-1u)],
                 f32_bits(predictor_weight_bits[i]),acc);
    return acc;
}

void ubig_stage_a_limiter_process_256(ubig_stage_a_limiter *s,float ceiling,float *left,float *right)
{
    if(!s||!left||!right)return;
    const float c_rise=f32_bits(COEFF_PRIMARY_RISE_BITS);
    const float c_fall=f32_bits(COEFF_PRIMARY_FALL_BITS);
    const float c_secondary=f32_bits(COEFF_SECONDARY_BITS);

    for(unsigned group=0;group<4u;group++){
        for(unsigned j=0;j<UBIG_A_LIMITER_DELAY;j++){
            unsigned frame=group*UBIG_A_LIMITER_DELAY+j;
            float peak=linked_peak(left[frame],right[frame]);

            float dl=s->delay[0][s->delay_pos];
            float dr=s->delay[1][s->delay_pos];
            s->delay[0][s->delay_pos]=left[frame];
            s->delay[1][s->delay_pos]=right[frame];
            left[frame]=s->current_gain*dl;
            right[frame]=s->current_gain*dr;

            if(s->peak_history[s->history_pos]<peak)
                s->peak_history[s->history_pos]=peak;

            /* The reference ramps previous->target over four samples using
               phase 1/4,2/4,3/4,4/4. */
            unsigned phase=(s->delay_pos&(UBIG_A_LIMITER_RAMP-1u))+1u;
            float t=(float)phase*0.25f;
            s->current_gain=fmaf(t,s->target_gain-s->previous_gain,s->previous_gain);

            s->delay_pos=(s->delay_pos+1u)&(UBIG_A_LIMITER_DELAY-1u);
            if(s->delay_pos&(UBIG_A_LIMITER_RAMP-1u))continue;

            float p=ring_max16(s->peak_history);
            float old=s->envelope_primary;
            float coeff=old<p?c_rise:c_fall;
            /* Reference uses separate multiply + add for primary envelope. */
            float delta=old-p;
            float prod=coeff*delta;
            float e1=p+prod;
            s->envelope_primary=e1;
            float m1=p<e1?e1:p;

            float old2=s->envelope_secondary;
            float e2=fmaf(old2-p,c_secondary,p);
            s->envelope_secondary=e2;
            float m2=p<e2?e2:p;
            float smoothed=m1<m2?m2:m1;
            s->predictor_history[s->history_pos]=smoothed;

            float predicted=predictor(s);
            s->previous_gain=s->target_gain;
            if(ceiling<predicted){
                double q=(double)ceiling/(double)predicted;
                s->target_gain=(float)q;
            }else{
                s->target_gain=1.0f;
            }

            s->history_pos=(s->history_pos+1u)&(UBIG_A_LIMITER_HISTORY-1u);
            s->peak_history[s->history_pos]=0.0f;
        }
    }
}
