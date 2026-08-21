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

void ubig_stage_b_leveler_pair_row(const UbigStageBLevelerPairCoefficients *coefficients,
                                   const float *target_b_flat,
                                   const UbigStageBLevelerPairControl *control,
                                   uint32_t count,
                                   const float *mix_values,
                                   const UbigStageBLevelerRecord *target_a,
                                   float *state_a_scalar,
                                   float *state_a_values,
                                   float *state_b_scalar,
                                   float *state_b_values,
                                   float scalar_mix)
{
    if(!coefficients||!target_b_flat||!control||!mix_values||!target_a||
       !target_a->values||!state_a_scalar||!state_a_values||!state_b_scalar||!state_b_values)return;
    ubig_stage_b_leveler_pair_smooth(coefficients,control,state_a_scalar,state_b_scalar,
                                     target_a->scalar,target_b_flat[0],scalar_mix);
    for(uint32_t i=0;i<count;i++)
        ubig_stage_b_leveler_pair_smooth(coefficients,control,&state_a_values[i],&state_b_values[i],
                                         target_a->values[i],target_b_flat[i+1u],mix_values[i]);
}

static float leveler_clamp_almost_one(float x)
{
    const float hi=f32_bits(0x3f7ffffeu);
    if(x < -hi)x=-hi;
    if(hi < x)x=hi;
    return x;
}

void ubig_stage_b_leveler_curve_rows(const float curve[17],
                                     const UbigStageBLevelerRecord *a,
                                     const UbigStageBLevelerRecord *b,
                                     uint32_t width,
                                     uint32_t index,
                                     float *out,
                                     float anchor_bias,
                                     float input_bias)
{
    if(!curve||!a||!b||!out||!a[index].values||!b[index].values)return;
    const float delta=anchor_bias-input_bias;
    const float anchor=a[index].scalar+delta;
    out[index*21u]=ubig_stage_b_leveler_piecewise(curve,b[index].scalar+delta);
    for(uint32_t k=0;k<width;k++){
        const float x=b[index].values[k]+(anchor-a[index].values[k]);
        out[index*21u+1u+k]=ubig_stage_b_leveler_piecewise(curve,leveler_clamp_almost_one(x));
    }
    for(uint32_t r=0;r<index;r++){
        float x=b[r].scalar+(anchor-a[r].scalar);
        out[r*21u]=ubig_stage_b_leveler_piecewise(curve,leveler_clamp_almost_one(x));
        for(uint32_t k=0;k<width;k++){
            x=b[r].values[k]+(anchor-a[r].values[k]);
            out[r*21u+1u+k]=ubig_stage_b_leveler_piecewise(curve,leveler_clamp_almost_one(x));
        }
    }
}

static void leveler_clamp_between(float *x,float lo,float hi)
{
    if(*x<lo)*x=lo;
    else if(hi<*x)*x=hi;
}

void ubig_stage_b_leveler_curve_bounds(const float *limits,
                                       const UbigStageBLevelerRecord *a,
                                       const UbigStageBLevelerRecord *b,
                                       const float curve[17],
                                       uint32_t width,
                                       uint32_t index,
                                       float *rows,
                                       float anchor_bias,
                                       float input_bias)
{
    if(!limits||!a||!b||!curve||!rows)return;
    const float delta=anchor_bias-input_bias;
    const float a_index=a[index].scalar;
    float base=(a_index<limits[index])?a_index:limits[index];
    const float curve_bound=ubig_stage_b_leveler_piecewise(curve,base+delta);
    float *indexed=rows+index*21u;
    float lo,hi;
    if(base<b[index].scalar){lo=indexed[0];hi=curve_bound;}
    else{lo=curve_bound;hi=indexed[0];}
    for(uint32_t k=0;k<width;k++)leveler_clamp_between(&indexed[1u+k],lo,hi);
    for(uint32_t r=0;r<index;r++){
        const float ar=a[r].scalar;
        base=(ar<limits[r])?ar:limits[r];
        const float bound=ubig_stage_b_leveler_piecewise(curve,(a_index-ar)+delta+base);
        float *row=rows+r*21u;
        leveler_clamp_between(&row[0],lo,hi);
        if(base>=b[r].scalar){
            for(uint32_t k=0;k<width;k++)leveler_clamp_between(&row[1u+k],bound,row[0]);
        }else{
            for(uint32_t k=0;k<width;k++)leveler_clamp_between(&row[1u+k],row[0],bound);
        }
    }
}

void ubig_stage_b_leveler_link_ceiling(const UbigStageBLevelerRecord *a,
                                       const UbigStageBLevelerRecord *b,
                                       const float *thresholds,
                                       uint32_t width,
                                       uint32_t index,
                                       float *rows)
{
    if(!a||!b||!thresholds||!rows)return;
    if(b[index].scalar>a[index].scalar)return;
    const float span=f32_bits(0x3e57d5ecu);
    const float scalar_gate=a[index].scalar-thresholds[index];
    const float lane_gate=a[index].scalar-span;
    float *indexed=rows+index*21u;
    float ceiling=indexed[0];
    for(uint32_t k=0;k<width;k++)
        if(lane_gate<a[index].values[k] && indexed[1u+k]<ceiling)ceiling=indexed[1u+k];
    ubig_stage_b_leveler_row_ceiling(indexed,width,ceiling);
    float global=ceiling;
    for(uint32_t r=0;r<index;r++){
        if(scalar_gate<a[r].scalar){
            float *row=rows+r*21u;
            float local=(row[0]<ceiling)?row[0]:ceiling;
            for(uint32_t k=0;k<width;k++)
                if(lane_gate<a[r].values[k] && row[1u+k]<local)local=row[1u+k];
            if(local<global)global=local;
        }
    }
    for(uint32_t r=0;r<index;r++)ubig_stage_b_leveler_row_ceiling(rows+r*21u,width,global);
}

static float leveler_curve_final_clamp(float x)
{
    const float lo=f32_bits(0xbefffffeu);
    const float hi=f32_bits(0x3cf652aau);
    if(x<lo)x=lo;
    if(hi<x)x=hi;
    return x;
}

void ubig_stage_b_leveler_curve_pipeline(const float curve[17],
                                         const UbigStageBLevelerRecord *a,
                                         const UbigStageBLevelerRecord *b,
                                         const float *limits,
                                         const float *thresholds,
                                         uint32_t width,
                                         uint32_t index,
                                         float *rows,
                                         uint32_t preserve,
                                         float anchor_bias,
                                         float input_bias)
{
    if(!curve||!a||!b||!limits||!thresholds||!rows)return;
    ubig_stage_b_leveler_curve_rows(curve,a,b,width,index,rows,anchor_bias,input_bias);
    const float almost_one=f32_bits(0x3f7ffffeu);
    const float high=f32_bits(0x3f2b2b71u);
    const float low=f32_bits(0x3e76bab1u);
    const float scale=f32_bits(0x3f15a490u);
    const float activity=b[index].scalar;
    float factor;
    if(activity>=high)factor=almost_one;
    else if(rows[index*21u]+anchor_bias<=0.0f)factor=almost_one;
    else if(activity<low)factor=0.0f;
    else{
        float t=(activity-low)*scale;
        t*=4.0f;
        const float t2=t*t;
        factor=(t2*t2)*t;
    }
    ubig_stage_b_leveler_curve_bounds(limits,a,b,curve,width,index,rows,anchor_bias,input_bias);
    ubig_stage_b_leveler_link_ceiling(a,b,thresholds,width,index,rows);
    const float half=anchor_bias*0.5f;
    for(uint32_t r=0;r<=index;r++){
        float *row=rows+r*21u;
        float v=preserve?fmaf(row[0],0.5f,half):half;
        v=leveler_curve_final_clamp(v*factor);
        row[0]=v+v;
        if(width){
            if(preserve){
                for(uint32_t k=0;k<width;k++){
                    v=fmaf(row[1u+k],0.5f,half);
                    v=leveler_curve_final_clamp(v*factor);
                    row[1u+k]=v+v;
                }
            }else{
                v=leveler_curve_final_clamp(half*factor);
                v+=v;
                for(uint32_t k=0;k<width;k++)row[1u+k]=v;
            }
        }
    }
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
