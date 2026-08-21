#include "stage_b_leveler_primitives.h"
#include <math.h>
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
