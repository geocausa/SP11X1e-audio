#include "ubig/ubig.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x51a7c39du;
static float sample(void){rng=rng*1664525u+1013904223u;int32_t v=(int32_t)(rng>>8)-(1<<23);return (float)v*(0x1p-26f);}
static uint64_t hash_bytes(uint64_t h,const void *ptr,size_t n){const unsigned char *p=ptr;for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ULL;}return h;}

int main(void)
{
    static const int32_t postgain[]={0,-1,-10,-50,-385,-595,-1200,0};
    enum{SEG=512,N=SEG*(int)(sizeof postgain/sizeof postgain[0])};
    float il[N],ir[N],ol[N],or_[N];
    for(int i=0;i<N;i++){il[i]=sample();ir[i]=sample();}
    ubig_engine_config cfg={UBIG_ABI_VERSION,UBIG_SAMPLE_RATE,UBIG_CHANNELS,UBIG_PROFILE_DYNAMIC};
    ubig_engine *e=ubig_engine_create(&cfg);if(!e)return 2;
    if(ubig_engine_set_postgain(NULL,0)!=UBIG_EINVAL ||
       ubig_engine_set_postgain(e,-1201)!=UBIG_EINVAL ||
       ubig_engine_set_postgain(e,1)!=UBIG_EINVAL)return 3;
    for(size_t k=0;k<sizeof postgain/sizeof postgain[0];k++){
        if(ubig_engine_set_postgain(e,postgain[k])!=UBIG_OK)return 4;
        if(ubig_engine_process(e,il+k*SEG,ir+k*SEG,ol+k*SEG,or_+k*SEG,SEG)!=UBIG_OK)return 5;
    }
    uint64_t h=1469598103934665603ULL;
    h=hash_bytes(h,ol,sizeof ol);h=hash_bytes(h,or_,sizeof or_);
    ubig_engine_destroy(e);
    if(h!=0x118d9bc2d1524da1ULL){fprintf(stderr,"Stage-A postgain hash mismatch: %016llx\n",(unsigned long long)h);return 6;}
    printf("PASS Stage-A endpoint postgain hash=%016llx\n",(unsigned long long)h);
    return 0;
}
