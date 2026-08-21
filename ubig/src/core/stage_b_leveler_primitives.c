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

void ubig_stage_b_leveler_history_init(UbigStageBLevelerHistory *s)
{
    if(!s)return;
    memset(s,0,sizeof(*s));
    s->reset_max=1u;
    s->max_a=f32_bits(0x3f11a2f0u);
}

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

_Static_assert(sizeof(UbigStageBLevelerSymmetricFilter)==0x18,"Leveler symmetric-filter descriptor size");

void ubig_stage_b_leveler_symmetric_filter(const UbigStageBLevelerSymmetricFilter *f,
                                           const float *input,
                                           uint32_t count,
                                           float *output)
{
    if(!f||!input||!output||count==0u)return;
    for(uint32_t pos=0;pos<count;pos++){
        float acc=f->coefficients[0]*input[pos];
        for(uint32_t tap=1;tap<f->taps;tap++){
            const uint32_t right=pos+tap;
            if(right<count)acc=fmaf(input[right],f->coefficients[tap],acc);
            if(pos>=tap)acc=fmaf(input[pos-tap],f->coefficients[tap],acc);
        }
        acc=f->post_scale[pos]*acc;
        if(acc< -1.0f)acc=-1.0f;
        output[pos]=acc;
    }
}

void ubig_stage_b_leveler_row_ceiling(float *values,uint32_t count,float ceiling)
{
    if(!values)return;
    for(uint32_t i=0;i<=count;i++)
        if(ceiling<values[i])values[i]=ceiling;
}

void ubig_stage_b_leveler_filter_blend(const UbigStageBLevelerSymmetricFilter *filter,
                                       uint32_t count,
                                       const float *blend,
                                       const float *input,
                                       float *output)
{
    if(!filter||!blend||!input||!output)return;
    ubig_stage_b_leveler_symmetric_filter(filter,input,count,output);
    const uint32_t vector_prefix=count&~7u;
    for(uint32_t i=0;i<vector_prefix;i++){
        if(output[i]>input[i]){
            const float correction=(blend[i]-1.0f)*output[i];
            const float weighted=input[i]*blend[i];
            output[i]=weighted-correction;
        }
    }
    for(uint32_t i=vector_prefix;i<count;i++){
        if(output[i]>input[i]){
            const float weighted=input[i]*blend[i];
            const float correction=blend[i]-1.0f;
            output[i]=fmaf(-correction,output[i],weighted);
        }
    }
}

_Static_assert(sizeof(UbigStageBLevelerPairCoefficients)==0x18,"Leveler pair-coefficient size");
_Static_assert(sizeof(UbigStageBLevelerPairControl)==0x18,"Leveler pair-control size");

void ubig_stage_b_leveler_pair_smooth(const UbigStageBLevelerPairCoefficients *a,
                                      const UbigStageBLevelerPairControl *b,
                                      float *state_a,
                                      float *state_b,
                                      float target_a,
                                      float target_b,
                                      float mix)
{
    if(!a||!b||!state_a||!state_b)return;
    const float almost_one=f32_bits(0x3f7ffffeu);
    const float threshold=f32_bits(0x3caff1e7u);
    float coeff;
    if(*state_a<=target_a && *state_b<=target_b){
        coeff=b->use_alternate?b->alternate:b->base;
    }else{
        const float half_delta=target_a*0.5f-(*state_a)*0.5f;
        if(half_delta>threshold)coeff=a->positive_far;
        else if(half_delta>0.0f)coeff=a->positive_near;
        else if(half_delta<=-threshold && b->negative_mode){
            if(mix>0.0f){
                const float remainder=almost_one-mix;
                const float base=remainder*a->negative_primary;
                coeff=fmaf(a->negative_mix,mix,base);
            }else{
                coeff=b->negative;
                if(b->compare_enable && target_b<*state_b)coeff=a->negative_primary;
            }
        }else{
            if(mix>0.0f){
                const float remainder=almost_one-mix;
                const float base=remainder*a->neutral_primary;
                coeff=fmaf(a->neutral_mix,mix,base);
            }else{
                coeff=b->base;
                if(b->compare_enable && target_b<*state_b)coeff=a->neutral_primary;
            }
        }
    }
    const float remainder=almost_one-coeff;
    const float old_a=(*state_a)*coeff;
    const float old_b=(*state_b)*coeff;
    *state_a=fmaf(remainder,target_a,old_a);
    *state_b=fmaf(remainder,target_b,old_b);
}

float ubig_stage_b_leveler_distribution_stat(uint32_t count,const float *values)
{
    if(!values&&count)return 0.0f;
    const float weight=f32_bits(0x3d4ccccdu);
    float mean=0.0f;
    for(uint32_t i=0;i<count;i++)mean=fmaf(values[i],weight,mean);

    float high=0.0f,low=0.0f;
    if(count){
        high=low=values[0]-mean;
        for(uint32_t i=1;i<count;i++){
            const float v=values[i]-mean;
            if(v>high)high=v;
            if(v<low)low=v;
        }
    }
    if(high==low)return f32_bits(0x3e19999au);

    const float max_abs=(-low>high)?-low:high;
    uint32_t raw;memcpy(&raw,&max_abs,4);
    int32_t exponent=((raw<<1)==0u)?-127:(int32_t)((raw>>23)&0xffu)-126;
    int32_t shift=-exponent;if(shift<0)shift=0;if(shift>60)shift=60;
    float scale=f32_bits((uint32_t)(shift+127)<<23);

    float m2=0.0f,m3=0.0f;
    const float cubic_weight=f32_bits(0x3d579436u);
    for(uint32_t i=0;i<count;i++){
        const float z=(values[i]-mean)*scale;
        const float z2=z*z;
        m2=fmaf(z2,weight,m2);
        const float z3=z2*z;
        m3=fmaf(z3,cubic_weight,m3);
    }

    const float root=sqrtf(m2);
    const float denominator=root*m2;
    const float numerator=m3*f32_bits(0x3d638e39u);
    const float denominator_abs=(-denominator>denominator)?-denominator:denominator;
    if(denominator_abs<=f32_bits(0x31abcc77u))return 0.0f;
    const float numerator_abs=(-numerator>numerator)?-numerator:numerator;
    if(denominator_abs<=numerator_abs)
        return ((numerator<0.0f)==(denominator<0.0f))?1.0f:-1.0f;

    memcpy(&raw,&denominator,4);
    exponent=((raw<<1)==0u)?-127:(int32_t)((raw>>23)&0xffu)-126;
    shift=-exponent;if(shift<0)shift=0;if(shift>60)shift=60;
    scale=f32_bits((uint32_t)(shift+127)<<23);
    const float d=denominator*scale;
    const float q=numerator*scale;
    float reciprocal;
    if(d>f32_bits(0x3f350600u))reciprocal=1.0f-d*0.5f;
    else reciprocal=(1.0f-d)+(1.0f-d);
    for(int i=0;i<4;i++){
        float error=fmaf(-reciprocal,d,0.5f);
        error*=reciprocal;
        error+=error;
        reciprocal=error+reciprocal;
    }
    float result=reciprocal*q;
    if(result< -0.5f)result=-0.5f;
    if(result>0.5f)result=0.5f;
    return result+result;
}

_Static_assert(sizeof(UbigStageBLevelerNormalizedCubic)==0x20,"Leveler normalized-cubic config size");

static float leveler_pow2i(int32_t exponent)
{
    return f32_bits((uint32_t)(exponent+127)<<23);
}

float ubig_stage_b_leveler_normalized_cubic(const float *input,
                                            float *output,
                                            uint32_t count,
                                            uint32_t write_only,
                                            const UbigStageBLevelerNormalizedCubic *c)
{
    if((!input||!output||!c)&&count)return 0.0f;
    const float base=f32_bits(0x3f11a2f0u);
    const float threshold=f32_bits(0x3ec1095eu);
    const float center=f32_bits(0x3ef3adcdu);
    const float weight=f32_bits(0x3d4ccccdu);
    float maximum=base;
    for(uint32_t i=0;i<count;i++)if(input[i]>maximum)maximum=input[i];
    const float shift=base-maximum;
    float change=0.0f;
    for(uint32_t i=0;i<count;i++){
        const float v=input[i]+shift;
        float y;
        if(v<threshold)y=0.0f;
        else{
            const float t=v-center;
            const float t2=t*t;
            float term=leveler_pow2i(c->exp1)*t;
            y=fmaf(term,c->coeff1,c->constant);
            term=leveler_pow2i(c->exp2)*t2;
            y=fmaf(term,c->coeff2,y);
            term=t2*t;
            term*=leveler_pow2i(c->exp3);
            y=fmaf(term,c->coeff3,y);
            if(y<0.0f)y=0.0f;
            if(y>1.0f)y=1.0f;
        }
        if(write_only)output[i]=y;
        else{
            float delta=y-output[i];
            output[i]=y;
            if(-delta>delta)delta=-delta;
            change=fmaf(delta,weight,change);
        }
    }
    return change;
}

static float leveler_lookup_interp(float x,const float *table)
{
    const float fallback=f32_bits(0xbf7ffffeu);
    if(x<=0.0f)return fallback;
    const float p0=x*64.0f;
    const float floor0=floorf(p0);
    const uint32_t index=(uint32_t)(int32_t)floor0;
    const float p1=x*64.0f;
    const float floor1=floorf(p1);
    const float frac=p1-floor1;
    const float lo=table[index];
    const float hi=table[index+1u];
    return fmaf(frac,hi-lo,lo);
}

void ubig_stage_b_leveler_lookup_map(uint32_t count,
                                     const float *input,
                                     float *output,
                                     const UbigStageBLevelerLookupTables *tables)
{
    if(!input||!output||!tables)return;
    const float gain=0.3125f;
    static const uint32_t baseline_bits[7]={
        0xbda00000u,0xbdc00000u,0xbde00000u,0xbde00000u,
        0xbe000000u,0xbe000000u,0xbe000000u
    };
    for(uint32_t i=0;i<7u;i++){
        if(!tables->table[i])return;
        const float temp=fmaf(-input[i],gain,f32_bits(baseline_bits[i]));
        output[i]=leveler_lookup_interp(-temp,tables->table[i]);
    }
    if(count>7u){
        if(!tables->table[7])return;
        for(uint32_t i=7u;i<count;i++){
            const float temp=fmaf(-input[i],gain,f32_bits(0xbe000000u));
            output[i]=leveler_lookup_interp(-temp,tables->table[7]);
        }
    }
}

_Static_assert(sizeof(UbigStageBLevelerLookupState)==0x20,"Leveler lookup-state size");
_Static_assert(sizeof(UbigStageBLevelerLookupConfig)==0x20,"Leveler lookup-config size");
_Static_assert(sizeof(UbigStageBLevelerLookupResult)==0x0c,"Leveler lookup-result size");

static float leveler_lookup_parent_exp2(float x)
{
    const float fl=floorf(x),frac=x-fl;
    const int32_t exponent=(int32_t)fl;
    float p=fmaf(frac,f32_bits(0x3d714000u),f32_bits(0x3e827800u));
    p=fmaf(p,frac,f32_bits(0x3f2fb000u));
    p=fmaf(p,frac,1.0f);
    return p*f32_bits((uint32_t)(exponent+127)<<23);
}

static float leveler_lookup_response(float value,const UbigStageBLevelerLookupConfig *c)
{
    if(c->high<value)return 1.0f;
    const float delta=value-c->low;
    if(delta<=0.0f)return 0.0f;
    return (c->response_scale*delta)*f32_bits((uint32_t)(c->response_exp+127)<<23);
}

void ubig_stage_b_leveler_lookup_process(UbigStageBLevelerLookupState *state,
                                         const UbigStageBLevelerLookupConfig *config,
                                         const float *input,
                                         uint32_t count,
                                         uint32_t copy_only,
                                         UbigStageBLevelerLookupResult *result,
                                         float control,
                                         float history,
                                         const UbigStageBLevelerLookupTables *tables,
                                         const UbigStageBLevelerNormalizedCubic *cubic)
{
    if(!state||!config||!input||!result||!tables||!cubic||
       !state->transition_state||!state->cubic_state||!config->transition)return;
    if(count<7u||count>20u)return;
    float maximum=-1.0f;
    for(uint32_t i=0;i<count;i++)if(input[i]>maximum)maximum=input[i];
    float mapped[20];
    for(uint32_t i=0;i<count;i++){
        float value=leveler_lookup_parent_exp2((input[i]-maximum)*f32_bits(0x422cbe00u));
        if(value>1.0f)value=1.0f;
        mapped[i]=value;
    }
    const float statistic=ubig_stage_b_leveler_distribution_stat(count,mapped);
    const float mix=config->feedback_mix*history;
    float feedback=fmaf(-mix,statistic,statistic);
    feedback=fmaf(state->feedback,mix,feedback);
    if(feedback<f32_bits(0x3d360b61u))feedback=f32_bits(0x3d360b61u);
    if(feedback>f32_bits(0x3e19999au))feedback=f32_bits(0x3e19999au);
    float factor=(feedback-f32_bits(0x3d360b61u))*f32_bits(0x3f179436u);
    factor*=16.0f;
    factor*=control;
    state->factor=factor;
    state->feedback=feedback;

    ubig_stage_b_leveler_transition_row(input,count,copy_only,1u,
                                         config->transition,config->transition,
                                         state->transition_state,0.0f);
    ubig_stage_b_leveler_lookup_map(count,state->transition_state,mapped,tables);
    const float change=ubig_stage_b_leveler_normalized_cubic(mapped,state->cubic_state,
                                                              count,copy_only,cubic);
    float target0=leveler_lookup_response(change,config);
    float target1=leveler_lookup_response(change*factor,config);
    if(state->out0>=target0)target0=state->out0*config->decay;
    state->out0=target0;
    result->out0=target0;
    result->flag=(state->out1<target1);
    if(!result->flag)target1=state->out1*config->decay;
    state->out1=target1;
    result->out1=target1;
}

static float leveler_lookup_link_interp(float x,const float *table)
{
    if(x<=0.0f)return f32_bits(0xbf7ffffeu);
    const float p0=x*64.0f;
    const float floor0=floorf(p0);
    const uint32_t index=(uint32_t)(int32_t)floor0;
    const float p1=x*64.0f;
    const float floor1=floorf(p1);
    const float frac=p1-floor1;
    const float lo=table[index],hi=table[index+1u];
    return fmaf(frac,hi-lo,lo);
}

float ubig_stage_b_leveler_lookup_link(const float *fallback,
                                       const float *input,
                                       uint32_t count,
                                       float floor_value,
                                       const float *offsets,
                                       const UbigStageBLevelerLookupTables *tables)
{
    if((!fallback||!input||!offsets||!tables)&&count)return f32_bits(0xbf7ffffeu);
    static const uint32_t baseline_bits[8]={
        0xbda00000u,0xbdc00000u,0xbde00000u,0xbde00000u,
        0xbe000000u,0xbe000000u,0xbe000000u,0xbe000000u
    };
    float linked=-1.0f;
    for(uint32_t i=0;i<count;i++){
        float top=(floor_value>input[i])?floor_value:input[i];
        float value;
        if(-1.0f>=top)value=fallback[i];
        else{
            float normalized=top-offsets[i];
            if(normalized>1.0f)normalized=1.0f;
            const uint32_t lane=i<7u?i:7u;
            if(!tables->table[lane])return f32_bits(0xbf7ffffeu);
            const float temp=fmaf(-normalized,0.3125f,f32_bits(baseline_bits[lane]));
            value=leveler_lookup_link_interp(-temp,tables->table[lane]);
        }
        const float maximum=(value>linked)?value:linked;
        const float delta=value-linked;
        const float ad=(-delta>delta)?-delta:delta;
        if(ad>=f32_bits(0x3e921ff3u))linked=maximum;
        else{
            float correction=fmaf(-ad,f32_bits(0x403d013bu),f32_bits(0x400288ceu));
            correction=fmaf(correction,ad,-f32_bits(0x3efbf488u));
            correction=fmaf(correction,ad,f32_bits(0x3d3020c5u));
            linked=maximum+correction;
            if(linked< -1.0f)linked=-1.0f;
            if(linked>1.0f)linked=1.0f;
        }
    }
    linked+=f32_bits(0x3d2ff1e7u);
    if(linked<f32_bits(0xbf7ffffeu))linked=f32_bits(0xbf7ffffeu);
    if(linked>f32_bits(0x3f7ffffeu))linked=f32_bits(0x3f7ffffeu);
    return linked;
}

float ubig_stage_b_leveler_lookup_regression(uint32_t count,
                                             const float *input,
                                             const float *fallback,
                                             const float *offsets,
                                             const UbigStageBLevelerLookupTables *tables)
{
    if((!input||!fallback||!offsets||!tables)&&count)return f32_bits(0xbf7ffffeu);
    float row[20];
    if(count>20u)return f32_bits(0xbf7ffffeu);
    float minimum=1.0f;
    for(uint32_t i=0;i<count;i++){
        row[i]=offsets[i]+input[i];
        if(row[i]<minimum)minimum=row[i];
    }
    float sum2=0.0f,sum3=0.0f;
    for(uint32_t i=0;i<count;i++){
        const float delta=row[i]-minimum;
        const float square=delta*delta;
        sum2+=square;
        sum3=fmaf(row[i],square,sum3);
    }
    float floor_value;
    if(sum2==0.0f)floor_value=-1.0f;
    else{
        floor_value=(float)((double)sum3/(double)sum2);
        floor_value-=f32_bits(0x3dcccccbu);
        if(floor_value< -1.0f)floor_value=-1.0f;
    }
    return ubig_stage_b_leveler_lookup_link(fallback,row,count,floor_value,offsets,tables);
}
