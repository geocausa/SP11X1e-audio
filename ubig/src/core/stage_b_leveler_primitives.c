#include "stage_b_leveler_primitives.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static float f32_bits(uint32_t u){float f;memcpy(&f,&u,4);return f;}

static float leveler_exp2_poly(float x)
{
    const float c0=f32_bits(0x3d714000u);
    const float c1=f32_bits(0x3e827800u);
    const float c2=f32_bits(0x3f2fb000u);
    const float fl=floorf(x);
    const float frac=x-fl;
    const int32_t exponent=(int32_t)fl;
    float p=fmaf(frac,c0,c1);
    p=fmaf(p,frac,c2);
    p=fmaf(p,frac,1.0f);
    const uint32_t bits=(uint32_t)(exponent+127)<<23;
    return p*f32_bits(bits);
}

void ubig_stage_b_leveler_coeff_triplet(uint32_t mode,
                                        const float config[3],
                                        float blend,
                                        float history,
                                        float drive,
                                        float *out_a,
                                        float *out_b,
                                        float *out_adaptive)
{
    if(!config||!out_a||!out_b||!out_adaptive)return;
    const float almost_one=f32_bits(0x3f7ffffeu);
    const float tenth=f32_bits(0x3dcccccdu);
    const float smoothed=fmaf(almost_one-history,tenth,history);
    const float base=mode?config[2]:config[1];
    const float scale=mode?f32_bits(0x40149a78u):f32_bits(0x408a4d3cu);
    const float shaped=leveler_exp2_poly(scale*(drive-1.0f))*smoothed;
    const float r_a=(float)((double)config[1]/(double)smoothed);
    const float r_b=(float)((double)config[2]/(double)smoothed);
    const float r_adaptive=(float)((double)base/(double)shaped);
    const float keep=almost_one-blend;
    *out_a=fmaf(leveler_exp2_poly(r_a),blend,keep);
    *out_b=fmaf(leveler_exp2_poly(r_b),blend,keep);
    *out_adaptive=fmaf(leveler_exp2_poly(r_adaptive),blend,keep);
}

_Static_assert(sizeof(UbigStageBLevelerHistory)==0x5e8,"Leveler history size");
_Static_assert(offsetof(UbigStageBLevelerHistory,total)==0xcc,"Leveler total offset");
_Static_assert(offsetof(UbigStageBLevelerHistory,ring_bin)==0xd4,"Leveler ring-bin offset");
_Static_assert(offsetof(UbigStageBLevelerHistory,ring_lo)==0x214,"Leveler ring-lo offset");
_Static_assert(offsetof(UbigStageBLevelerHistory,ring_hi)==0x354,"Leveler ring-hi offset");
_Static_assert(offsetof(UbigStageBLevelerHistory,ring_total)==0x494,"Leveler ring-total offset");
_Static_assert(offsetof(UbigStageBLevelerHistory,ring_pos)==0x5d4,"Leveler ring-pos offset");

void ubig_stage_b_leveler_history_update(UbigStageBLevelerHistory *s,
                                         float step,
                                         float value_a,
                                         float value_b)
{
    if(!s)return;
    if(s->reset_max){
        s->reset_max=0u;s->max_a=value_a;s->max_b=value_b;
    }else if(s->max_b<value_b){
        s->max_a=value_a;s->max_b=value_b;
    }else if(value_b==s->max_b && s->max_a<value_a){
        s->max_a=value_a;
    }
    s->phase+=step;
    if(s->phase<0.5f)return;
    s->phase-=0.5f;s->reset_max=1u;
    const uint32_t rp=s->ring_pos;
    const uint32_t old_bin=s->ring_bin[rp];
    s->bins[old_bin]-=s->ring_lo[rp];
    s->bins[old_bin+1u]-=s->ring_hi[rp];
    s->total-=s->ring_total[rp];

    const float map_offset=f32_bits(0x3f11a2f0u);
    const float map_scale=f32_bits(0x3f0c0000u);
    const float map_bins=f32_bits(0x43800000u);
    float x=(s->max_a-map_offset)*map_scale;
    x*=map_bins;
    const float fl=floorf(x);
    int32_t bin=(int32_t)fl;
    float sn,cs;
    if(bin<0){bin=0;sn=0.0f;cs=1.0f;}
    else if(bin>49){bin=49;sn=1.0f;cs=0.0f;}
    else{
        const float frac=(x-fl)*f32_bits(0x3ec90fdbu);
        const float angle=frac*4.0f;
        sn=sinf(angle);cs=cosf(angle);
    }
    const float weight=s->max_b*f32_bits(0x3c087a8du);
    const float lo=cs*weight,hi=sn*weight;
    s->ring_bin[rp]=(uint32_t)bin;
    s->ring_lo[rp]=lo;s->ring_hi[rp]=hi;s->ring_total[rp]=weight;
    s->bins[(uint32_t)bin]+=lo;
    s->bins[(uint32_t)bin+1u]+=hi;
    s->total+=weight;
    s->ring_pos=(rp+1u>=80u)?0u:rp+1u;
    const uint32_t count=s->count+1u;
    s->count=(count>=80u)?80u:count;
}

static int leveler_curve_shift(float v)
{
    uint32_t u;
    memcpy(&u,&v,4);
    const uint32_t exponent=(u>>23)&0xffu;
    const int e=((u<<1)==0u)?-127:(int)exponent-126;
    int shift=-e;
    if(shift<0)shift=0;
    if(shift>60)shift=60;
    return shift;
}

static int leveler_clamp_signed60(int v)
{
    if(v>=60)v=60;
    if(v<=-60)v=-60;
    return v;
}

void ubig_stage_b_leveler_curve_build(float c[17],
                                      float anchor,
                                      float slope_control,
                                      float delta)
{
    if(!c)return;
    const float one=1.0f,half=0.5f;
    c[1]=anchor+delta;
    c[2]=anchor;
    const float slope=slope_control-one;
    const float midpoint=fmaf(delta,half,anchor);
    c[6]=midpoint*(-slope);
    c[7]=slope;

    const float ad=fabsf(delta);
    float z=0.0f;
    if(ad>=f32_bits(0x322bcc77u)){
        const int shift_delta=leveler_curve_shift(delta);
        const float delta_scale=f32_bits((uint32_t)(shift_delta+127)<<23);
        const float normalized=delta_scale*delta;
        float reciprocal;
        if(f32_bits(0x3f350600u)<normalized)
            reciprocal=fmaf(-normalized,half,one);
        else{
            const float q=one-normalized;
            reciprocal=q+q;
        }
        for(unsigned i=0;i<4;i++){
            float error=fmaf(-reciprocal,normalized,half);
            error*=reciprocal;
            error+=error;
            reciprocal+=error;
        }
        z=slope*reciprocal;
        const int shift_z=leveler_curve_shift(z);
        const float z_scale=f32_bits((uint32_t)(shift_z+127)<<23);
        const float z_normalized=z_scale*z;
        const int shift_delta_z=shift_delta-shift_z;
        const int exponent_adjust=leveler_clamp_signed60(shift_delta_z);
        const float adjust=f32_bits((uint32_t)(exponent_adjust+127)<<23);
        z=adjust*z_normalized;
        if(shift_delta>=shift_z){
            const int32_t saved=exponent_adjust;
            memcpy(&c[11],&saved,4);
        }else{
            const uint32_t zero=0u;
            memcpy(&c[11],&zero,4);
        }
    }else{
        const uint32_t zero=0u;
        memcpy(&c[11],&zero,4);
    }
    c[9]=0.0f;
    c[10]=z;
}

float ubig_stage_b_leveler_piecewise(const float c[17],float input)
{
    if(!c)return 0.0f;
    float v=input<c[0]?input:c[0];
    if(c[1]<v)return fmaf(c[7],v,c[6]);
    if(c[2]<v){v-=c[2];const float t=fmaf(c[10],v,c[9]);return t*v;}
    if(c[3]<v)return 0.0f;
    if(c[5]>v)v=c[5];
    if(c[4]<v){v-=c[3];const float t=fmaf(c[13],v,c[12]);return t*v;}
    return fmaf(c[16],v,c[15]);
}

_Static_assert(sizeof(UbigStageBLevelerRowState)==0x20,"Leveler row-state size");
_Static_assert(sizeof(UbigStageBLevelerRowConfig)==0x10,"Leveler row-config size");
_Static_assert(sizeof(UbigStageBLevelerRowResult)==0x0c,"Leveler row-result size");

static float leveler_soft_max(float a,float b)
{
    const float maximum=(b>a)?b:a;
    const float d=a-b;
    const float ad=(-d>d)?-d:d;
    if(ad>=0x1.3b13b2p-3f)return maximum;
    float t=fmaf(-ad,0x1.45328cp+3f,0x1.e44c28p+1f);
    t=fmaf(t,ad,-0x1.f7e15p-2f);
    t=fmaf(t,ad,0x1.7b6302p-6f);
    float out=maximum+t;
    if(out< -1.0f)out=-1.0f;
    if(out>  1.0f)out= 1.0f;
    return out;
}

void ubig_stage_b_leveler_row_update(UbigStageBLevelerRowState *s,
                                     const UbigStageBLevelerRowConfig *c,
                                     const float *input,
                                     uint32_t count,
                                     uint32_t force_event,
                                     UbigStageBLevelerRowResult *r,
                                     float metric)
{
    if(!s||!c||!input||!r)return;
    float reduced=-1.0f;
    float delta_sum=0.0f;
    for(uint32_t i=0;i<count;i++){
        const float v=input[i];
        reduced=leveler_soft_max(reduced,v);
        delta_sum+=s->current[i]-v;
        s->current[i]=s->previous[i];
        s->previous[i]=v;
    }
    r->hold_expired=0u;
    if(!((metric-f32_bits(0x3ed4ad4bu))>reduced)){
        const uint32_t old_hold=s->hold;
        s->hold=0u;
        r->hold_expired=(old_hold<c->hold_limit)?0u:1u;
        const float decayed=c->release*s->coefficient;
        s->coefficient=decayed;
        s->coefficient=(1.0f-c->release)+decayed;
    }else if(s->hold<c->hold_limit){
        const uint32_t next=s->hold+1u;
        s->hold=(next>=c->hold_limit)?c->hold_limit:next;
    }

    int32_t effective=s->event_age;
    if(c->delta_threshold<delta_sum || effective>0){
        effective+=1;
        s->event_age=effective;
    }
    if(effective>1 || (effective<=1 && force_event!=0u)){
        s->event_age=0;
        r->event=1u;
        s->coefficient=f32_bits(0x3c23d70au);
        r->coefficient=s->coefficient;
        return;
    }
    r->event=0u;
    r->coefficient=s->coefficient;
}

void ubig_stage_b_leveler_apply_row_floors(uint32_t count,float *v)
{
    if(!v||count<7u)return;
    const float floors[7]={-0.25f,-0.3f,-0.35f,-0.35f,-0.4f,-0.4f,-0.4f};
    for(uint32_t i=0;i<7u;i++)if(floors[i]>v[i])v[i]=floors[i];
    for(uint32_t i=7u;i<count;i++)if(-0.4f>v[i])v[i]=-0.4f;
}

_Static_assert(sizeof(UbigStageBLevelerInputRows)==0x10,"Leveler input-row descriptor size");
_Static_assert(sizeof(UbigStageBLevelerPreparedRows)==0x18,"Leveler prepared-row descriptor size");

void ubig_stage_b_leveler_prepare_rows(const float *base,
                                       const UbigStageBLevelerInputRows *input,
                                       UbigStageBLevelerPreparedRows *output,
                                       float bias)
{
    if(!base||!input||!output||!output->rows)return;
    const uint32_t count=input->count;
    const uint32_t width=input->width;
    float *aggregate=NULL;
    if(count>1u){
        aggregate=output->rows[count];
        output->count=count+1u;
    }else{
        output->count=count;
    }
    output->width=width;
    if(count==0u)return;
    for(uint32_t row=0;row<count;row++){
        float *dst=output->rows[row];
        const float *src=input->rows[row];
        for(uint32_t i=0;i<width;i++){
            const float shifted=src[i]+bias;
            dst[i]=shifted+base[i];
        }
        for(uint32_t i=width;i<output->width_capacity;i++)dst[i]=-1.0f;
        ubig_stage_b_leveler_apply_row_floors(width,dst);
        if(aggregate){
            if(row==0u){
                memcpy(aggregate,dst,(size_t)output->width_capacity*sizeof(float));
            }else{
                for(uint32_t i=0;i<output->width_capacity;i++)
                    aggregate[i]=leveler_soft_max(aggregate[i],dst[i]);
            }
        }
    }
}

_Static_assert(sizeof(UbigStageBLevelerTransitionRecord)==0x18,"Leveler transition-record size");

float *ubig_stage_b_leveler_transition_row(const float *input,
                                           uint32_t count,
                                           uint32_t copy_only,
                                           uint32_t common_config,
                                           const UbigStageBLevelerTransitionRecord *large_rise,
                                           const UbigStageBLevelerTransitionRecord *normal,
                                           float *state,
                                           float rise_threshold)
{
    if(!input||!state)return state;
    if(copy_only){
        for(uint32_t i=0;i<count;i++)state[i]=input[i];
        return state;
    }
    if(!large_rise||!normal)return state;
    for(uint32_t i=0;i<count;i++){
        const float src=input[i];
        const float previous=state[i];
        const uint32_t rising=previous<src;
        const uint32_t config_index=common_config?0u:i;
        const UbigStageBLevelerTransitionRecord *cfg;
        if(rising && rise_threshold<(src-previous))cfg=&large_rise[config_index];
        else cfg=&normal[config_index];
        const float a=rising?cfg->rise_previous:cfg->fall_previous;
        const float b=rising?cfg->rise_input:cfg->fall_input;
        const float weighted=fmaf(b,src,a*previous);
        const float previous_floor=(previous>-1.0f)?cfg->previous_offset+previous:-1.0f;
        const float input_floor=(src>-1.0f)?cfg->input_offset+src:-1.0f;
        float floor=(input_floor>previous_floor)?input_floor:previous_floor;
        if(floor< -1.0f)floor=-1.0f;
        state[i]=(weighted>floor)?weighted:floor;
    }
    return state;
}
