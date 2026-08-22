#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t rng=0x65e06440u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float rf(void){uint32_t u=(ru()&0x007fffffu)|(((ru()%40u)+100u)<<23);if(ru()&1u)u|=0x80000000u;float f;memcpy(&f,&u,4);return f;}
static void hb(uint64_t*h,const void*p,size_t n){const uint8_t*b=p;for(size_t i=0;i<n;i++){*h^=b[i];*h*=UINT64_C(1099511628211);}}
typedef struct {uint32_t calls;} Ctx;
static void block(void *v,float *out,float *in){Ctx*c=v;c->calls++;for(uint32_t i=0;i<512u;i++)out[i]=in[i]*(float)(c->calls+1u)+((i&1u)?-0.25f:0.125f);}
int main(void){
    float src[512]={0},dst[512]={0},in[4096],out[4096];Ctx ctx={0};
    UbigStageBRtStream256State st={src,dst,0u,2u,2u};uint64_t h=UINT64_C(1469598103934665603);
    for(uint32_t pass=0;pass<12000u;pass++){
        uint32_t frames=1u+ru()%2048u;for(uint32_t i=0;i<frames*2u;i++)in[i]=rf();
        ubig_stage_b_rt_stream256_process(&st,out,in,frames,block,&ctx);
        hb(&h,out,(size_t)frames*2u*sizeof(float));hb(&h,src,sizeof src);hb(&h,dst,sizeof dst);hb(&h,&st.position,sizeof st.position);hb(&h,&ctx.calls,sizeof ctx.calls);
    }
    if(h!=UINT64_C(0x985788b580e79a72)){fprintf(stderr,"stream256 hash mismatch=%016llx\n",(unsigned long long)h);return 1;}
    printf("PASS Stage-B RT stream256 hash=%016llx calls=%u pos=%u\n",(unsigned long long)h,ctx.calls,st.position);return 0;
}
