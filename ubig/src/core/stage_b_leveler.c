#include "stage_b_leveler.h"
#include "stage_a_math.h"
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

_Static_assert(sizeof(UbigStageBLevelerProducerState)==0x2a8,"Leveler producer-state size");
_Static_assert(offsetof(UbigStageBLevelerProducerState,state_b_scalar)==0x140,"Leveler producer b scalar");
_Static_assert(offsetof(UbigStageBLevelerProducerState,state_a_values)==0x150,"Leveler producer a values");
_Static_assert(offsetof(UbigStageBLevelerProducerState,state_a_scalar)==0x290,"Leveler producer a scalar");
_Static_assert(offsetof(UbigStageBLevelerProducerState,negative_mode)==0x2a0,"Leveler producer mode");
_Static_assert(sizeof(UbigStageBLevelerProducerConfig)==0x28,"Leveler producer-config size");

static float leveler_producer_exp2(float x)
{
    const float fl=floorf(x);
    const float frac=x-fl;
    const int32_t exponent=(int32_t)fl;
    float p=fmaf(frac,f32_bits(0x3d714000u),f32_bits(0x3e827800u));
    p=fmaf(p,frac,f32_bits(0x3f2fb000u));
    p=fmaf(p,frac,1.0f);
    return p*f32_bits((uint32_t)(exponent+127)<<23);
}

static float leveler_producer_error_mix(float delta)
{
    const float almost_one=f32_bits(0x3f7ffffeu);
    const float threshold=f32_bits(0x3d2ff1e5u);
    if(threshold<=delta)return almost_one;
    if(delta>0.0f){
        const float t=delta*f32_bits(0x41ba3d77u);
        return t*t;
    }
    return 0.0f;
}

static float leveler_abs_select(float x)
{
    return (-x>x)?-x:x;
}

static float leveler_adaptive_exp2(float x)
{
    const float c0=f32_bits(0x3d714000u);
    const float c1=f32_bits(0x3e827800u);
    const float c2=f32_bits(0x3f2fb000u);
    int32_t exponent=(int32_t)x;
    if((float)exponent>x)exponent--;
    const float frac=x-(float)exponent;
    float p=fmaf(frac,c0,c1);
    p=fmaf(p,frac,c2);
    p=fmaf(p,frac,1.0f);
    uint32_t bits;
    memcpy(&bits,&p,sizeof bits);
    bits+=(uint32_t)exponent<<23;
    memcpy(&p,&bits,sizeof p);
    return p;
}

static float leveler_adaptive_scratch(float source,float base)
{
    float x=fmaf(source,0.25f,-f32_bits(0x3dc0e094u));
    x+=base;
    const float low=f32_bits(0xbeaff1e7u);
    const float high=f32_bits(0x3e2a28f4u);
    if(x<low)x=low;
    if(high<x)x=high;
    return x-high;
}

void ubig_stage_b_leveler_adaptive_filter_process(
        UbigStageBLevelerAdaptiveState *state,
        const UbigStageBLevelerAdaptiveControl *control,
        const UbigStageBLevelerSymmetricFilter *filter,
        const UbigStageBLevelerSourceGate *source_gate,
        const float *reference_source,
        const float *band_weights,
        uint32_t emit,
        uint32_t reset,
        uint32_t direct_update,
        UbigStageBLevelerProducerRows *output,
        int32_t *telemetry,
        float reference_bias,
        float output_scale,
        float slow_mix,
        float rise_modulation,
        float target_scale)
{
    if(!state||!state->fast||!state->slow||!control||!control->rise_mix||!control->blend||
       !filter||!source_gate||!source_gate->source||!reference_source||!band_weights||
       !output||!output->row_ptrs||!output->records||
       !output->row_ptrs[UBIG_STAGE_B_LEVELER_ADAPTIVE_INDEX]||
       !output->records[UBIG_STAGE_B_LEVELER_ADAPTIVE_INDEX].values)return;
    const uint32_t width=UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH;
    const uint32_t index=UBIG_STAGE_B_LEVELER_ADAPTIVE_INDEX;
    if(reset){
        memset(state->fast,0,width*sizeof(float));
        memset(state->slow,0,width*sizeof(float));
        direct_update=1u;
    }

    float scratch_a[UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH];
    float scratch_b[UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH];
    const float *record=output->records[index].values;
    const float *indexed=output->row_ptrs[index];
    for(uint32_t i=0;i<width;i++){
        scratch_a[i]=leveler_adaptive_scratch(record[i],indexed[i]);
        scratch_b[i]=leveler_adaptive_scratch(reference_source[i],reference_bias*0.25f);
    }

    float target_a[UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH];
    float target_b[UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH];
    const float exp_scale=f32_bits(0x42ba3d77u);
    for(uint32_t i=0;i<width;i++){
        target_a[i]=leveler_adaptive_exp2(scratch_a[i]*exp_scale);
        target_b[i]=leveler_adaptive_exp2(scratch_b[i]*exp_scale);
    }

    uint32_t rising_mask[UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH];
    const float mix=control->mix;
    const float keep=1.0f-mix;
    for(uint32_t i=0;i<width;i++){
        float previous=state->fast[i];
        rising_mask[i]=(previous<=target_a[i]);
        if(rising_mask[i]){
            const float lane_mix=control->blend[i];
            const float left=(1.0f-lane_mix)*target_a[i];
            previous=fmaf(previous,lane_mix,left);
        }else{
            const float floor=target_b[i];
            if(floor<=previous){
                const float left=keep*target_a[i];
                previous=fmaf(previous,mix,left);
                if(floor>previous)previous=floor;
            }else{
                previous=floor;
            }
        }
        state->fast[i]=previous;
    }

    float snapshot[UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH];
    memcpy(snapshot,state->fast,sizeof snapshot);
    float cubic_sum=0.0f;
    float weighted_sum=0.0f;
    for(uint32_t i=0;i<width;i++){
        const float x=snapshot[i];
        const float x2=x*x;
        cubic_sum=fmaf(x,x2,cubic_sum);
        weighted_sum=fmaf(band_weights[i],x2,weighted_sum);
    }
    float ratio=0.0f;
    if(cubic_sum>0.0f && weighted_sum>0.0f)
        ratio=(float)((double)cubic_sum/(double)weighted_sum);

    float normalized[UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH];
    float total=0.0f;
    for(uint32_t i=0;i<width;i++){
        float floor=band_weights[i]*0.125f;
        floor*=ratio;
        float value=snapshot[i]*0.25f;
        if(value<floor)value=floor;
        normalized[i]=value;
        total=fmaf(value,f32_bits(0x3d800000u),total);
    }
    if(total<=0.0f||source_gate->gate<=0.0f)return;

    for(uint32_t i=0;i<width;i++){
        const float reciprocal=(float)(1.0/(double)normalized[i]);
        float x=source_gate->source[i]*total;
        x=reciprocal*x;
        float target=ubig_stage_a_log2_approx(x);
        target*=f32_bits(0x3d2ff1e7u);
        target*=target_scale;
        target*=0.25f;
        if(direct_update){
            state->slow[i]=target;
            continue;
        }
        const float previous=state->slow[i];
        float coefficient;
        if(previous<=target||rising_mask[i]==0u){
            coefficient=slow_mix;
            if(previous<target&&rising_mask[i]==0u){
                float adjusted=1.0f-rise_modulation;
                adjusted=fmaf(control->rise_mix[i],rise_modulation,adjusted);
                coefficient=adjusted*slow_mix;
            }
        }else{
            coefficient=0.0f;
        }
        const float left=(1.0f-coefficient)*target;
        state->slow[i]=fmaf(previous,coefficient,left);
    }

    if(!emit)return;
    float filtered[UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH];
    ubig_stage_b_leveler_symmetric_filter(filter,state->slow,width,filtered);
    for(uint32_t i=0;i<width;i++){
        filtered[i]+=f32_bits(0x3acb1168u);
        filtered[i]*=output_scale;
    }
    for(uint32_t row=0;row<=index;row++){
        if(!output->row_ptrs[row])continue;
        for(uint32_t i=0;i<width;i++)output->row_ptrs[row][i]+=filtered[i];
    }
    if(telemetry){
        for(uint32_t i=0;i<width;i++){
            float value=filtered[i]*f32_bits(0x3f68cccdu);
            value*=f32_bits(0x46800000u);
            telemetry[i]+=(int32_t)floorf(value);
        }
    }
}

void ubig_stage_b_leveler_producer_process(UbigStageBLevelerProducerState *s,
                                           const UbigStageBLevelerProducerConfig *c,
                                           const UbigStageBLevelerRecord *input,
                                           const UbigStageBLevelerRecord *anchors,
                                           uint32_t update_mode,
                                           uint32_t width,
                                           uint32_t index,
                                           const float curve[17],
                                           float control0,
                                           float control1,
                                           float curve_bias,
                                           float input_bias,
                                           const UbigStageBLevelerPairCoefficients *override_coefficients,
                                           uint32_t reset,
                                           float *error_rows,
                                           UbigStageBLevelerProducerRows *out,
                                           uint32_t preserve_rows,
                                           const float *log_thresholds)
{
    if(!s||!c||!c->filter||!input||!anchors||!curve||!out||!out->records||!log_thresholds)return;
    if(index>=UBIG_STAGE_B_LEVELER_PRODUCER_ROWS||width>UBIG_STAGE_B_LEVELER_PRODUCER_WIDTH)return;
    const float almost_one=f32_bits(0x3f7ffffeu);
    if(reset){
        const uint32_t count=index+(index>1u);
        for(uint32_t r=0;r<count;r++){
            s->state_b_scalar[r]=0.0f;
            s->state_a_scalar[r]=-almost_one;
            for(uint32_t k=0;k<width;k++){
                s->state_b_values[r][k]=0.0f;
                s->state_a_values[r][k]=-almost_one;
            }
        }
    }

    float pipeline[UBIG_STAGE_B_LEVELER_PRODUCER_ROWS*21u]={0};
    ubig_stage_b_leveler_curve_pipeline(curve,anchors,out->records,s->state_a_scalar,
                                        log_thresholds,width,index,pipeline,preserve_rows,
                                        curve_bias,input_bias);

    const UbigStageBLevelerPairCoefficients *pair=override_coefficients?override_coefficients:&c->pair;
    float remainder=almost_one-control0;
    float denominator=remainder*0.5f;
    denominator=fmaf(control0,f32_bits(0x3c23d70au),denominator);
    const float ratio=(float)((double)c->exp_drive/(double)denominator);
    UbigStageBLevelerPairControl control;
    remainder=almost_one-control1;
    control.base=fmaf(pair->neutral_primary,control1,remainder);
    control.negative=fmaf(pair->negative_primary,control1,remainder);
    control.alternate=leveler_producer_exp2(ratio);

    if(update_mode){
        float delta=out->records[index].scalar*0.5f;
        delta=fmaf(-s->state_a_scalar[index],0.5f,delta);
        s->negative_mode=(f32_bits(0xbcaff1e7u)>=delta);
    }
    control.negative_mode=s->negative_mode;
    const float active=f32_bits(0x3f2b2b71u);
    if(out->records[index].scalar<active){
        uint32_t hold=s->hold_count+1u;
        if(hold>c->hold_limit)hold=c->hold_limit;
        s->hold_count=hold;
    }else{
        s->hold_count=0u;
    }
    control.compare_enable=(s->hold_count>=c->hold_limit);
    control.use_alternate=(s->state_a_scalar[index]<=out->records[index].scalar &&
                           s->state_b_scalar[index]<=pipeline[index*21u] &&
                           s->state_a_scalar[index]<active);

    float mix[UBIG_STAGE_B_LEVELER_PRODUCER_WIDTH];
    float indexed_filtered[UBIG_STAGE_B_LEVELER_PRODUCER_WIDTH];
    float filtered[UBIG_STAGE_B_LEVELER_PRODUCER_WIDTH];
    for(int32_t rr=(int32_t)index;rr>=0;--rr){
        const uint32_t r=(uint32_t)rr;
        const float scalar_mix=leveler_producer_error_mix(s->state_a_scalar[r]-input[r].scalar);
        for(uint32_t k=0;k<width;k++){
            const float lane_mix=leveler_producer_error_mix(s->state_a_values[r][k]-input[r].values[k]);
            mix[k]=(scalar_mix<lane_mix)?scalar_mix:lane_mix;
        }
        ubig_stage_b_leveler_pair_row(pair,pipeline+r*21u,&control,width,mix,&out->records[r],
                                      &s->state_a_scalar[r],s->state_a_values[r],
                                      &s->state_b_scalar[r],s->state_b_values[r],scalar_mix);
        ubig_stage_b_leveler_filter_blend(c->filter,width,mix,s->state_b_values[r],filtered);
        if(r==index&&width)memcpy(indexed_filtered,filtered,width*sizeof(float));
        if(out->row_ptrs&&out->row_ptrs[r])
            for(uint32_t k=0;k<width;k++)
                out->row_ptrs[r][k]=fmaf(filtered[k],0.25f,out->row_ptrs[r][k]);
        if(error_rows&&index>1u&&r<index){
            for(uint32_t k=0;k<width;k++){
                const float d0=filtered[k]-indexed_filtered[k];
                const float d1=filtered[k]-curve_bias;
                const float sum=leveler_abs_select(d0)+leveler_abs_select(d1);
                float value=0.0f;
                if(f32_bits(0x3ccccccbu)>sum){
                    float t=sum*32.0f;
                    t=fmaf(sum,8.0f,t);
                    value=almost_one-t;
                    value*=value;
                }
                error_rows[r*UBIG_STAGE_B_LEVELER_PRODUCER_WIDTH+k]=value;
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

static float leveler_parent_clamp(float value,float low,float high)
{
    if(value<low)value=low;
    if(high<value)value=high;
    return value;
}

float ubig_stage_b_leveler_parent_process(UbigStageBLevelerParentState *s,
                                          const UbigStageBLevelerParentConfig *c,
                                          const UbigStageBLevelerParentTuning *t,
                                          const UbigStageBLevelerParentControl *ctl,
                                          const float previous_curve[17],
                                          const float curve_template[18],
                                          const UbigStageBLevelerPairCoefficients *override_coefficients,
                                          const UbigStageBLevelerSourceGate *source_gate,
                                          UbigStageBLevelerInputRows *input,
                                          UbigStageBLevelerInputRows *output,
                                          int32_t *telemetry)
{
    if(!s||!c||!t||!ctl||!curve_template||!source_gate||!input||!output||
       !s->matrix_rows||!s->lookup||!s->lifecycle||!s->transition_rows||
       !s->producer||!s->writer||!s->adaptive||!c->base_row||!c->lifecycle||
       !c->lookup||!c->transition_large_rise||!c->transition_normal||!c->adaptive||
       !c->filter||!c->matrix_transition||!c->writer||!c->producer||
       !t->lookup_tables||!t->inverse_tables||!t->cubic||!t->lookup_offsets||
       !t->producer_thresholds||!t->adaptive_band_weights||!t->tail_coefficients||
       !input->rows||!output->rows)return 0.0f;
    if(input->count!=UBIG_STAGE_B_LEVELER_PARENT_ROWS||
       input->width!=UBIG_STAGE_B_LEVELER_PARENT_WIDTH||
       output->count!=UBIG_STAGE_B_LEVELER_PARENT_ROWS||
       output->width!=UBIG_STAGE_B_LEVELER_PARENT_WIDTH)return 0.0f;

    const float low=f32_bits(0xbe9d89d7u);
    const float bias_base=f32_bits(0x3ed4ad4bu);
    const float almost=f32_bits(0x3f7ffffeu);
    const float high=f32_bits(0x3e6c4ec3u);
    float bias_a=leveler_parent_clamp(ctl->row_bias_a,low,0.0f);
    bias_a=bias_base-bias_a;
    float bias_b=leveler_parent_clamp(ctl->row_bias_b,low,0.0f);
    bias_b=bias_base-bias_b;
    const float matrix_bias=leveler_parent_clamp(ctl->matrix_bias,-almost,high);

    float prepared_values[4][UBIG_STAGE_B_LEVELER_PARENT_WIDTH];
    float *prepared_ptrs[4]={prepared_values[0],prepared_values[1],prepared_values[2],prepared_values[3]};
    UbigStageBLevelerPreparedRows prepared={0u,0u,prepared_ptrs,4u,UBIG_STAGE_B_LEVELER_PARENT_WIDTH};
    ubig_stage_b_leveler_prepare_rows(c->base_row,input,&prepared,
                                      bias_a+f32_bits(0x3e48dc8cu));
    if(prepared.count==0u||prepared.count>4u)return 0.0f;

    UbigStageBLevelerRowResult lifecycle={0u,0u,0.0f};
    ubig_stage_b_leveler_row_update(s->lifecycle,c->lifecycle,
                                    prepared.rows[prepared.count-1u],prepared.width,
                                    ctl->lifecycle_force,&lifecycle,bias_a);
    const uint32_t linked_mode=(lifecycle.event||lifecycle.hold_expired)?1u:0u;

    float record_values[4][UBIG_STAGE_B_LEVELER_PARENT_WIDTH];
    UbigStageBLevelerRecord records[4];
    float producer_rows[4][UBIG_STAGE_B_LEVELER_PARENT_WIDTH]={0};
    float *producer_ptrs[4]={producer_rows[0],producer_rows[1],producer_rows[2],producer_rows[3]};
    for(uint32_t row=0;row<prepared.count;row++){
        records[row].values=record_values[row];
        records[row].reserved=0u;
        float *transition_state=s->transition_rows[row];
        ubig_stage_b_leveler_transition_row(prepared.rows[row],prepared.width,
                                            linked_mode,1u,
                                            c->transition_large_rise,c->transition_normal,
                                            transition_state,f32_bits(0x3d9d89d7u));
        ubig_stage_b_leveler_lookup_map(prepared.width,transition_state,
                                        record_values[row],t->lookup_tables);
        records[row].scalar=ubig_stage_b_leveler_lookup_regression(
                prepared.width,transition_state,record_values[row],
                t->lookup_offsets,t->lookup_tables);
    }
    UbigStageBLevelerProducerRows producer_output={producer_ptrs,records};

    UbigStageBLevelerLookupResult lookup_result={0u,0.0f,0.0f};
    ubig_stage_b_leveler_lookup_process(s->lookup,c->lookup,
                                        prepared.rows[prepared.count-1u],prepared.width,
                                        linked_mode,&lookup_result,ctl->lookup_control,
                                        lifecycle.coefficient,t->lookup_tables,t->cubic);

    if(lifecycle.event)
        ubig_stage_b_leveler_reset(s->writer,input->count+(input->count>1u),input->width);
    ubig_stage_b_leveler_update(s->writer,c->writer,input->count,input->width,
                                lookup_result.out0,lookup_result.out1,
                                lifecycle.coefficient,records);

    float curve_bias=0.0f;
    if(previous_curve)
        curve_bias=ubig_stage_b_leveler_piecewise(previous_curve,
                    s->writer->primary[input->count].scalar-f32_bits(0x3f4d4e84u));

    float curve[18];
    memcpy(curve,curve_template,sizeof curve);
    if(previous_curve){
        const float mix=lifecycle.coefficient;
        const float keep=1.0f-mix;
        float anchor=previous_curve[2]*keep;
        anchor=fmaf(curve_template[2],mix,anchor);
        float slope=(previous_curve[7]+1.0f)*keep;
        slope=fmaf(curve_template[7]+1.0f,mix,slope);
        const float delta=curve_template[1]-curve_template[2];
        ubig_stage_b_leveler_curve_build(curve,anchor,slope,delta);
    }

    float error_rows[4][UBIG_STAGE_B_LEVELER_PARENT_WIDTH]={0};
    ubig_stage_b_leveler_producer_process(s->producer,c->producer,
            s->writer->primary,s->writer->secondary,lookup_result.flag,
            input->width,input->count,curve,lookup_result.out0,lookup_result.out1,
            curve_bias,f32_bits(0x3f4d4e84u),override_coefficients,lifecycle.event,
            &error_rows[0][0],&producer_output,ctl->preserve_rows,t->producer_thresholds);

    ubig_stage_b_leveler_adaptive_filter_process(s->adaptive,c->adaptive,c->filter,
            source_gate,s->writer->secondary[input->count].values,
            t->adaptive_band_weights,ctl->adaptive_emit,lifecycle.event,
            ctl->adaptive_direct,&producer_output,telemetry,curve_bias,
            ctl->adaptive_output_scale,lifecycle.coefficient,lookup_result.out1,
            ctl->adaptive_target_scale);

    float matrix_rows[UBIG_STAGE_B_LEVELER_PARENT_ROWS][UBIG_STAGE_B_LEVELER_PARENT_WIDTH];
    ubig_stage_b_leveler_matrix_process(s->matrix_rows,c->matrix_transition,
            (const float *const*)prepared.rows,(const float *const*)producer_output.row_ptrs,
            &error_rows[0][0],input->count,input->width,linked_mode,
            matrix_bias,0.0f,&matrix_rows[0][0],t->lookup_tables,t->inverse_tables);

    float tail[UBIG_STAGE_B_LEVELER_PARENT_WIDTH];
    ubig_stage_b_leveler_tail_shape(input->width,tail,curve_bias,t->tail_coefficients);
    const float delta=bias_a-bias_b;
    const float telemetry_scale0=f32_bits(0x3f020000u);
    const float telemetry_scale1=f32_bits(0x45800000u);
    for(uint32_t row=0;row<input->count;row++){
        for(uint32_t lane=0;lane<input->width;lane++){
            float value;
            if(tail[lane]<=-1.0f||delta<=-1.0f||matrix_rows[row][lane]<=-1.0f){
                value=-1.0f;
            }else{
                value=matrix_rows[row][lane]+delta;
                value+=tail[lane];
                if(value<-1.0f)value=-1.0f;
                if(1.0f<value)value=1.0f;
            }
            output->rows[row][lane]+=value;
            input->rows[row][lane]+=value;
            if(telemetry&&row==0u)
                telemetry[lane]+=(int32_t)floorf((value*telemetry_scale0)*telemetry_scale1);
        }
    }

    float result=bias_a-bias_b;
    float extra=curve_bias*f32_bits(0x3f653949u);
    extra+=extra;
    result+=extra;
    return result;
}
