#include "stage_b_leveler.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static float f32_bits(uint32_t u){float f;memcpy(&f,&u,4);return f;}

_Static_assert(sizeof(UbigStageBLevelerRecord)==16,"Stage-B Leveler record size");
_Static_assert(offsetof(UbigStageBLevelerState,history)==0x20,"Stage-B Leveler history offset");
_Static_assert(sizeof(UbigStageBLevelerState)==0x608,"Stage-B Leveler state size");

void ubig_stage_b_leveler_reset(UbigStageBLevelerState *s,
                                uint32_t record_count,
                                uint32_t width)
{
    if(!s)return;
    const float almost_one=f32_bits(0x3f7ffffeu);
    const float negative_almost_one=f32_bits(0xbf7ffffeu);
    s->base=almost_one;
    s->hold_count=0u;
    s->adaptive_state=0.0f;
    if(s->secondary){
        for(uint32_t r=0;r<record_count;r++){
            s->secondary[r].scalar=negative_almost_one;
            if(s->secondary[r].values)
                for(uint32_t i=0;i<width;i++)
                    s->secondary[r].values[i]=negative_almost_one;
        }
    }
    ubig_stage_b_leveler_history_init(&s->history);
}

static float history_interp(const UbigStageBLevelerHistory *h,float value)
{
    float x=(value-f32_bits(0x3f11a2f0u))*f32_bits(0x3f0c0000u);
    x*=f32_bits(0x43800000u);
    const float fl=floorf(x);
    int32_t bin=(int32_t)fl;
    float sn,cs;
    if(bin<0){bin=0;sn=0.0f;cs=1.0f;}
    else if(bin>49){bin=49;sn=1.0f;cs=0.0f;}
    else{
        const float q=(x-fl)*f32_bits(0x3ec90fdbu);
        const float angle=q*4.0f;
        sn=sinf(angle);cs=cosf(angle);
    }
    const float next=h->bins[(uint32_t)bin+1u]*sn;
    return fmaf(h->bins[(uint32_t)bin],cs,next);
}

static float select_coeff(uint32_t rising,float old,float target,
                          float rise_coeff,float fall_coeff,float zero_coeff)
{
    if(rising)return old<target?rise_coeff:fall_coeff;
    return target<=old?rise_coeff:zero_coeff;
}

static float blend_value(float old,float target,float coeff)
{
    const float almost_one=f32_bits(0x3f7ffffeu);
    const float add=(almost_one-coeff)*target;
    return fmaf(old,coeff,add);
}

void ubig_stage_b_leveler_update(UbigStageBLevelerState *s,
                                 const UbigStageBLevelerConfig *c,
                                 uint32_t index,
                                 uint32_t width,
                                 float control_mix,
                                 float direct_control,
                                 float secondary_scale,
                                 const UbigStageBLevelerRecord *observed)
{
    if(!s||!c||!s->primary||!s->secondary||!observed)return;
    const float almost_one=f32_bits(0x3f7ffffeu);
    const float activity_threshold=f32_bits(0x3f2b2b71u);
    const float control_threshold=f32_bits(0x3dcccccdu);
    const uint32_t related=index+(index>1u);
    const float target=observed[index].scalar;
    const uint32_t rising=s->primary[index].scalar<target;

    float control=direct_control;
    if(rising){
        if(s->hold_count>=c->hold_limit)s->base=c->base_decay*s->base;
        else s->hold_count++;
        const float remainder=almost_one-s->base;
        control=fmaf(s->base,control_mix,remainder);
    }else{
        s->hold_count=0u;
        s->base=almost_one;
    }

    if(activity_threshold<target && control_threshold<control)
        ubig_stage_b_leveler_history_update(&s->history,c->history_step,target,control);

    float adaptive_mix,drive;
    if(s->history.count>=5u){
        const float remainder=almost_one-c->adaptive_smooth;
        adaptive_mix=fmaf(c->adaptive_smooth,s->adaptive_state,remainder);
        s->adaptive_state=adaptive_mix;
        if(s->history.total==0.0f)drive=0.0f;
        else{
            const float value=history_interp(&s->history,s->primary[index].scalar);
            drive=value==s->history.total
                ? 1.0f
                : (float)((double)value/(double)s->history.total);
        }
        const float max_ratio=f32_bits(0x3d99999au);
        if(max_ratio<drive)drive=max_ratio;
        drive*=f32_bits(0x3f555554u);
        drive*=16.0f;
        if(almost_one<drive)drive=almost_one;
    }else{
        s->adaptive_state=0.0f;
        drive=almost_one;
        adaptive_mix=almost_one;
    }

    float rise_coeff,fall_coeff,zero_coeff;
    if(control<control_threshold){
        rise_coeff=fall_coeff=zero_coeff=almost_one;
    }else{
        /* Reference output-pointer ordering maps x2/x3/x4 to
         * fall/zero/rise coefficient registers in the parent. */
        ubig_stage_b_leveler_coeff_triplet(rising,(const float*)c,
                                           control,adaptive_mix,drive,
                                           &fall_coeff,&zero_coeff,&rise_coeff);
    }

    float coeff=activity_threshold<target?rise_coeff:almost_one;
    s->primary[index].scalar=blend_value(s->primary[index].scalar,target,coeff);
    float secondary_coeff=coeff*secondary_scale;
    s->secondary[index].scalar=blend_value(s->secondary[index].scalar,target,secondary_coeff);

    for(uint32_t j=0;j<index;j++){
        const float t=observed[j].scalar;
        coeff=select_coeff(rising,s->primary[j].scalar,t,rise_coeff,fall_coeff,zero_coeff);
        s->primary[j].scalar=blend_value(s->primary[j].scalar,t,coeff);
        secondary_coeff=coeff*secondary_scale;
        s->secondary[j].scalar=blend_value(s->secondary[j].scalar,t,secondary_coeff);
    }

    for(uint32_t j=0;j<related;j++){
        for(uint32_t k=0;k<width;k++){
            const float t=observed[j].values[k];
            coeff=select_coeff(rising,s->primary[j].values[k],t,rise_coeff,fall_coeff,zero_coeff);
            s->primary[j].values[k]=blend_value(s->primary[j].values[k],t,coeff);
            secondary_coeff=coeff*secondary_scale;
            s->secondary[j].values[k]=blend_value(s->secondary[j].values[k],t,secondary_coeff);
        }
    }
}
