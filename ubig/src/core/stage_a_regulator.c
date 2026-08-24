#include "stage_a_regulator.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static float load_f32(const unsigned char *p, unsigned off)
{
    float v; memcpy(&v,p+off,4); return v;
}
static uint32_t load_u32(const unsigned char *p, unsigned off)
{
    uint32_t v; memcpy(&v,p+off,4); return v;
}
static uintptr_t load_ptr(const unsigned char *p, unsigned off)
{
    uintptr_t v; memcpy(&v,p+off,sizeof(v)); return v;
}
static void store_f32(unsigned char *p, unsigned off, float v)
{
    memcpy(p+off,&v,4);
}
static void store_u32(unsigned char *p, unsigned off, uint32_t v)
{
    memcpy(p+off,&v,4);
}

void ubig_stage_a_regulator_adapt(void *state,
                                  float drive,
                                  float observed_max,
                                  const void *source_tuning,
                                  void *working_tuning)
{
    unsigned char *s = state;
    unsigned char *dst = working_tuning;
    const float *cfg = (const float *)load_ptr(s,0x68);
    const float attack = cfg[0];
    const float release = cfg[1];
    const float diff_keep = cfg[2];

    uint32_t ring_index = load_u32(s,0xd4);
    store_f32(s,0x70 + 4u*ring_index, observed_max);
    ring_index = (ring_index + 1u) % 5u;
    store_u32(s,0xd4,ring_index);

    const float previous = load_f32(s,0xd8);
    float smoothed;
    if (previous < observed_max) {
        const float incoming = (1.0f - attack) * observed_max;
        smoothed = fmaf(previous, attack, incoming);
    } else {
        float recent = load_f32(s,0x70);
        if (recent < -1.0f) recent = -1.0f;
        for (unsigned i=1;i<5;i++) {
            const float v = load_f32(s,0x70 + 4u*i);
            if (v > recent) recent = v;
        }
        const float incoming = (1.0f - release) * recent;
        smoothed = fmaf(previous, release, incoming);
    }
    store_f32(s,0xd8,smoothed);

    memcpy(dst,source_tuning,0xa4);

    float diff = smoothed - drive;
    if (diff > 0.0f) diff = 0.0f;
    const float previous_diff = load_f32(s,0xdc);
    const float incoming_diff = diff * (1.0f - diff_keep);
    const float smooth_diff = fmaf(previous_diff,diff_keep,incoming_diff);
    store_f32(s,0xdc,smooth_diff);

    for (unsigned group=0; group<4; ++group) {
        if (load_u32(dst,4u*group) > 20u)
            return;

        const unsigned stride = 36u*group;
        const unsigned threshold_off = 0x18u + stride;
        const unsigned pad_off = 0x28u + stride;
        const float threshold = load_f32(dst,threshold_off);
        float scale = 1.0f;
        if (threshold < smooth_diff) {
            const double ratio = (double)smooth_diff / (double)threshold;
            scale = sqrtf((float)ratio);
        }

        const float pad = load_f32(dst,pad_off);
        if (smooth_diff > -pad) {
            const double ratio = (double)(-smooth_diff) / (double)pad;
            const float limited = (float)ratio;
            if (limited < scale) scale = limited;
        }

        const float root_scale = sqrtf(scale);
        store_f32(dst,threshold_off,threshold*scale);
        store_f32(dst,pad_off,pad*root_scale);
    }
}

void ubig_stage_a_monotone_cubic(const int32_t *knots_x,
                                  const float *knots_y,
                                  uint32_t knot_count,
                                  const int32_t *query_x,
                                  float *output,
                                  uint32_t output_count)
{
    if (knot_count < 2u || output_count == 0u) return;
    output[0]=knots_y[0];
    output[output_count-1u]=knots_y[knot_count-1u];
    if (output_count <= 1u) return;

    uint32_t query=1u, right=1u, left=0u, start=0u;
    while(query < output_count && right < knot_count){
        if(query_x[query] < knots_x[right]){
            ++query;
            continue;
        }

        const int32_t x0=knots_x[left], x1=knots_x[right];
        const float y0=knots_y[left], y1=knots_y[right];
        const int32_t dx=x1-x0;
        const float inv_dx=(float)(1.0/(double)dx);
        const float slope=(y1-y0)*inv_dx;

        float m0=slope;
        if(left>0u){
            const float prev=knots_y[left-1u];
            const float test=(y0-prev)*slope;
            if(test>0.0f){
                const int32_t span=x1-knots_x[left-1u];
                const float inv_span=(float)(1.0/(double)span);
                m0=(y1-prev)*inv_span;
            }
        }

        float m1=0.0f;
        if(right+1u<knot_count){
            const float next=knots_y[right+1u];
            const float test=(next-y1)*slope;
            if(test>0.0f){
                const int32_t span=knots_x[right+1u]-x0;
                const float inv_span=(float)(1.0/(double)span);
                m1=(next-y0)*inv_span;
            }
        }

        const float two_m0=m0+m0;
        const float msum=two_m0+m1;
        const float b_num=fmaf(slope,3.0f,-msum);
        const float two_slope=slope+slope;
        const float a_num=(m1+m0)-two_slope;
        const int32_t dx2=dx*dx;
        const float inv_dx2=(float)(1.0/(double)dx2);
        const float a=a_num*inv_dx2;
        const float b=b_num*inv_dx;

        uint32_t i=start;
        const uint32_t count=query-start;
        const uint32_t vector_count=(count/8u)*8u;
        const uint32_t vector_end=start+vector_count;
        for(;i<vector_end;++i){
            const float t=(float)(query_x[i]-x0);
            float v=a*t;
            v=v+b;
            v=v*t;
            v=v+m0;
            v=v*t;
            v=v+y0;
            output[i]=v;
        }
        for(;i<query;++i){
            const float t=(float)(query_x[i]-x0);
            float v=fmaf(t,a,b);
            v=fmaf(v,t,m0);
            v=fmaf(v,t,y0);
            output[i]=v;
        }

        left=right;
        ++right;
        start=query;
        if(right>=knot_count) break;
        ++query;
    }
}

void ubig_stage_a_regulator_expand(void *state,
                                   const void *working_tuning,
                                   uint32_t group_count,
                                   uint32_t channel,
                                   float output20[20])
{
    unsigned char *s=state;
    const unsigned char *t=working_tuning;
    float weighted=0.0f;
    uint32_t previous=0u;
    for(uint32_t g=0;g<group_count;++g){
        const uint32_t boundary=load_u32(t,4u*g);
        const float value=load_f32(s,4u*(channel+8u+2u*g));
        const float width=(float)((int32_t)boundary-(int32_t)previous);
        weighted=fmaf(width*0.05f,value,weighted);
        previous=boundary;
    }
    for(uint32_t g=0;g<group_count;++g){
        const unsigned off=4u*(channel+8u+2u*g);
        const float value=load_f32(s,off);
        if(weighted<value) store_f32(s,off,weighted);
    }

    int32_t knots_x[6]={0};
    float knots_y[6]={0};
    static const int32_t query_x[20]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
    uint32_t knot_count=1u;
    const float first=load_f32(s,4u*(channel+8u))+load_f32(t,0x28);
    knots_y[0]=first;
    previous=0u;
    for(uint32_t g=0;g<group_count;++g){
        const uint32_t boundary=load_u32(t,4u*g);
        const uint32_t midpoint=(boundary+previous-1u)>>1;
        if(midpoint> (uint32_t)knots_x[knot_count-1u]){
            knots_x[knot_count]=(int32_t)midpoint;
            knots_y[knot_count]=load_f32(s,4u*(channel+8u+2u*g))
                               +load_f32(t,0x28u+36u*g);
            ++knot_count;
        }
        previous=boundary;
    }
    knots_x[knot_count]=19;
    const uint32_t last=(group_count?group_count-1u:0u);
    knots_y[knot_count]=load_f32(s,4u*(channel+8u+2u*last))
                       +load_f32(t,0x28u+36u*last);
    ++knot_count;

    ubig_stage_a_monotone_cubic(knots_x,knots_y,knot_count,query_x,output20,20u);
}

void ubig_stage_a_regulator_process(float drive,
                                    float slow_mix,
                                    void *state,
                                    const void *source_tuning,
                                    uint32_t adaptive_enable,
                                    uint32_t slow_enable,
                                    const struct ubig_float_rows *main_rows,
                                    const struct ubig_float_rows *secondary_rows,
                                    int32_t *input_telemetry,
                                    int32_t *group_telemetry,
                                    int32_t *expanded_telemetry)
{
    unsigned char *s=state;
    const unsigned char *src=source_tuning;
    const uint32_t channels=main_rows->count;
    const uint32_t bands=main_rows->reserved;
    const float *transition_cfg=(const float *)load_ptr(s,0x60);
    const float *dynamic_cfg=(const float *)load_ptr(s,0x68);
    uint32_t group_count=0u;

    for(uint32_t ch=0;ch<channels;++ch){
        uint32_t cursor=0u;
        uint32_t groups=0u;
        while(cursor<bands && groups<4u){
            const uint32_t boundary=load_u32(src,4u*groups);
            if(boundary>bands) break;
            float maximum=main_rows->rows[ch][cursor++];
            while(cursor<boundary){
                maximum=ubig_comp_soft_max(maximum,main_rows->rows[ch][cursor]);
                ++cursor;
            }
            const unsigned detector_off=4u*(ch+(groups+8u)*2u);
            const float previous=load_f32(s,detector_off);
            store_f32(s,detector_off,ubig_comp_transition5(transition_cfg,previous,maximum));
            ++groups;
        }
        group_count=groups;
    }

    float observed=-1.0f;
    const uint32_t max_channels=channels<2u?channels:2u;
    for(uint32_t ch=0;ch<max_channels;++ch){
        float v=-1.0f;
        for(uint32_t band=0;band<bands;++band)
            v=ubig_comp_soft_max(v,main_rows->rows[ch][band]);
        if(v>observed) observed=v;
    }

    if(adaptive_enable)
        ubig_stage_a_regulator_adapt(s,drive,observed,src,s+0x100);
    else
        memcpy(s+0x100,src,0xa4);

    for(uint32_t group=0;group<group_count;++group){
        const unsigned base=0x110u+36u*group;
        const float f0=load_f32(s,base+0x00);
        const float f1=load_f32(s,base+0x04);
        const float f2=load_f32(s,base+0x08);
        const float threshold=load_f32(s,base+0x0c);
        const float knee=load_f32(s,base+0x10);
        const float full_gain=load_f32(s,base+0x14);
        const float fast_down=load_f32(s,base+0x1c);
        const float fast_up=load_f32(s,base+0x20);
        float raw[2]={0.0f,0.0f};
        float minimum=0.0f;
        const uint32_t nch=channels<2u?channels:2u;

        for(uint32_t ch=0;ch<nch;++ch){
            const float input=load_f32(s,4u*(16u+2u*group+ch));
            float gain=full_gain;
            if(input>threshold){
                const float knee_end=threshold+knee;
                if(input<=knee_end){
                    const double ratio=(double)(input-threshold)/(double)knee;
                    gain=(1.0f-(float)ratio)*full_gain;
                }else{
                    const float lower=f0-f1;
                    if(input>=lower && input<f0){
                        const double ratio=(double)(input-lower)/(double)f1;
                        gain=(float)ratio*f2;
                    }else{
                        gain=(input>=f0)?f2:0.0f;
                    }
                }
            }
            raw[ch]=gain;
            if(gain<minimum) minimum=gain;
        }

        const float global_offset=load_f32(s,0x1a0);
        for(uint32_t ch=0;ch<nch;++ch){
            float limited=raw[ch];
            const float cap=global_offset+minimum;
            if(cap<limited) limited=cap;
            raw[ch]=limited;
            const float previous=load_f32(s,4u*(8u+2u*group+ch));
            const float coeff=(limited<previous)?fast_down:fast_up;
            const unsigned fast_off=4u*(2u*group+ch);
            const float old_fast=load_f32(s,fast_off);
            const float incoming=(1.0f-coeff)*limited;
            store_f32(s,fast_off,fmaf(old_fast,coeff,incoming));
        }

        float mix=0.0f;
        if(slow_enable) mix=(group==0u)?slow_mix:1.0f;
        const float mix_keep=1.0f-mix;
        for(uint32_t ch=0;ch<nch;++ch){
            const unsigned slow_off=4u*(56u+2u*group+ch);
            const float old_slow=load_f32(s,slow_off);
            const float coeff=(raw[ch]<old_slow)?dynamic_cfg[3]:dynamic_cfg[4];
            const float incoming=(1.0f-coeff)*raw[ch];
            const float slow=fmaf(old_slow,coeff,incoming);
            store_f32(s,slow_off,slow);

            const float fast=load_f32(s,4u*(2u*group+ch));
            const float selected=(fast<slow)?fast:slow;
            const float mixed=selected*mix;
            const float final=fmaf(fast,mix_keep,mixed);
            store_f32(s,4u*(8u+2u*group+ch),final);
        }
    }

    for(uint32_t ch=0;ch<max_channels;++ch){
        float expanded[20];
        ubig_stage_a_regulator_expand(s,s+0x100,group_count,ch,expanded);

        if(input_telemetry){
            for(uint32_t b=0;b<bands;++b)
                input_telemetry[ch*bands+b]=(int32_t)floorf(main_rows->rows[ch][b]*2080.0f);
        }
        if(group_telemetry){
            for(uint32_t g=0;g<group_count;++g)
                group_telemetry[ch*4u+g]=(int32_t)floorf(load_f32(s,4u*(ch+(g+8u)*2u))*2080.0f);
            for(uint32_t g=group_count;g<4u;++g)
                group_telemetry[ch*4u+g]=-2080;
        }
        if(expanded_telemetry){
            for(uint32_t b=0;b<bands;++b)
                expanded_telemetry[ch*bands+b]=(int32_t)floorf(expanded[b]*2080.0f);
        }
        for(uint32_t b=0;b<bands;++b){
            main_rows->rows[ch][b]+=expanded[b];
            secondary_rows->rows[ch][b]+=expanded[b];
        }
    }
}
