#include "stage_a_math.h"
#include <math.h>
#include <string.h>

static float from_bits(uint32_t u){float f;memcpy(&f,&u,4);return f;}
static uint32_t to_bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}

/* DECODED 0x1800247c0. The reference normalizes the input by replacing its
   exponent with 126, then evaluates a fused quadratic around that mantissa
   and adds the original unbiased exponent. Domain in the live Stage-A path is
   positive finite amplitude. */
float ubig_stage_a_log2_approx(float x)
{
    uint32_t u=to_bits(x);
    uint32_t m=(u & ~(0x1ffu<<23)) | (126u<<23); /* ARM BFI #23,#9 */
    float mf=from_bits(m);
    int32_t exponent=(int32_t)((u>>23)&0xffu)-126;
    const float c0=from_bits(0x402aaaabu); /* 2.66666675 */
    const float c1=from_bits(0x3faaaaabu); /* 1.33333337 */
    /* AArch64 fused subtract forms here evaluate product-minus-addend,
       then accumulator-minus-product in this operand arrangement. */
    float q0=fmaf(mf,4.0f,-c0);            /* 4*m - 8/3 */
    float m2=mf*mf;
    float q1=fmaf(-m2,c1,q0);              /* q0 - (4/3)*m^2 */
    return q1+(float)exponent;
}

/* DECODED 0x180023d20. Four-wide in the reference, but each lane is
   independent. The exact cubic coefficients are shared with the known Stage-A
   amplitude conversion. The integer part is floor(x), not truncation, and the
   2^integer term is formed directly in the IEEE-754 exponent field. */
void ubig_stage_a_exp2_scaled(float *out,const float *in,uint32_t count,float scale)
{
    const float c1=from_bits(0x3f2fb000u);
    const float c2=from_bits(0x3e827800u);
    const float c3=from_bits(0x3d714000u);
    for(uint32_t i=0;i<count;i++){
        float x=in[i]*scale;
        int32_t trunc=(int32_t)x;
        float ti=(float)trunc;
        if(ti>x) trunc--;
        float ip=(float)trunc;
        float f=x-ip;
        uint32_t exp_delta=(uint32_t)trunc<<23;
        float p=1.0f;
        p=fmaf(f,c1,p);
        float f2=f*f;
        float f3=f2*f;
        p=fmaf(f2,c2,p);
        p=fmaf(f3,c3,p);
        /* FCVTZS #23 followed by integer ADD adjusts the polynomial float's
           exponent field directly; it is not a floating-point addition. */
        out[i]=from_bits(to_bits(p)+exp_delta);
    }
}
