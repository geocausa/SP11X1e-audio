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
