#include "stage_a_compressor_primitives.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t h64(uint64_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}
static int expect(const char *name, uint64_t got, uint64_t want)
{
    if (got == want) return 1;
    fprintf(stderr, "%s hash %016llx != %016llx\n", name,
            (unsigned long long)got, (unsigned long long)want);
    return 0;
}
int main(void)
{
    const uint64_t seed = 1469598103934665603ULL;
    unsigned char obj[0x114]; memset(obj, 0xa5, sizeof obj);
    ubig_comp_band_state_init(obj, NULL, 20);
    if (!expect("band_init", h64(seed,obj,sizeof obj), 0xb63b5757ce2fb957ULL)) return 2;

    float cfg2[32]; for (int i=0;i<32;i++) cfg2[i]=(i+1)*0.0078125f;
    struct ubig_directional_smoother ds={cfg2,20,{0}};
    cfg2[0]=0.8125f; cfg2[1]=0.1875f;
    int32_t flags[20]; float target[20];
    for(int i=0;i<20;i++){ds.value[i]=(i-8)*0.02734375f;target[i]=(10-i)*0.01953125f;flags[i]=i%3==0;}
    ubig_comp_directional_smooth(&ds,flags,target);
    if(!expect("direction",h64(seed,ds.value,80),0x1028faab8a223eddULL)) return 3;

    float cfg6[6]={0.75f,0.125f,0.625f,0.25f,0.875f,0.0625f};
    struct ubig_scalar_state sg={cfg6,20,-0.03125f};
    float ia[20],ib[20],mix[20],lo[20],hi[20];
    for(int i=0;i<20;i++){ia[i]=(i-10)*0.03125f;ib[i]=(7-i)*0.0234375f;mix[i]=0.75f+i*0.0078125f;}
    ubig_comp_slow_gain_bounds(&sg,ia,ib,lo,hi,mix,-0.015625f,0.03125f,-0.5f,0.25f,0.375f);
    uint64_t h=seed; h=h64(h,&sg.value,4);h=h64(h,lo,80);h=h64(h,hi,80);
    if(!expect("slow_bounds",h,0xa9175f239f062933ULL)) return 4;

    float cfg4[4]={0.875f,0.125f,0.75f,0.25f};
    struct ubig_scalar_state nc={cfg4,20,-0.0625f};
    float in[20],fl[20],ce[20],half[20],corr[20]; int32_t mask[20];
    for(int i=0;i<20;i++){in[i]=(i-9)*0.0390625f;fl[i]=-0.25f+i*0.00390625f;ce[i]=fl[i]+0.125f;mask[i]=(i%4)==1;}
    ubig_comp_nonlinear_correction(&nc,in,fl,ce,mask,half,corr,0.8125f,-0.09375f);
    h=seed;h=h64(h,&nc.value,4);h=h64(h,half,80);h=h64(h,corr,80);
    if(!expect("nonlinear",h,0xe9357328e13713d3ULL)) return 5;

    float ref[20],u[20],l[20];
    for(int i=0;i<20;i++){ref[i]=(11-i)*0.0546875f;u[i]=(i-6)*0.025390625f;l[i]=u[i]-0.078125f;mask[i]=(i%5)==2;}
    ubig_comp_linked_deviation(mask,ref,20,u,l,0.59375f);
    h=seed;h=h64(h,u,80);h=h64(h,l,80);
    if(!expect("linked",h,0x2267a9d66c382442ULL)) return 6;

    float ni[20],no[20];
    for(int i=0;i<20;i++){ni[i]=(i%7)*0.0625f-0.125f;mask[i]=(i%3)==0;}
    ubig_comp_neighbor_limit(20,mask,ni,no);
    if(!expect("neighbor",h64(seed,no,80),0xc6ea133fecf3def6ULL)) return 7;

    float sm[8]={ubig_comp_soft_max(-0.2f,-0.1f),ubig_comp_soft_max(0.0f,0.01f),
                 ubig_comp_soft_max(0.5f,0.55f),ubig_comp_soft_max(-0.9f,-0.91f),
                 ubig_comp_soft_max(0.9f,0.91f),ubig_comp_soft_max(-1.2f,-1.19f),
                 ubig_comp_soft_max(0.2f,0.5f),ubig_comp_soft_max(-0.5f,-0.2f)};
    if(!expect("softmax",h64(seed,sm,sizeof sm),0x98a2371bc52b8001ULL)) return 8;

    puts("PASS Stage A compressor primitive proven-regression vectors");
    return 0;
}
