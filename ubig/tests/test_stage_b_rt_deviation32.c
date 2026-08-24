#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>

static uint32_t rng=0x91c73a55u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    float x[32];uint64_t h=1469598103934665603ULL;
    for(unsigned t=0;t<100000;t++){
        uint32_t shift=ru()%61u;
        for(unsigned i=0;i<32;i++)x[i]=(ru()%19u==0u)?0.0f:fr(-2.0f,2.0f);
        float mean=(ru()%23u==0u)?0.0f:fr(-1.0f,1.0f);
        float out=ubig_stage_b_rt_deviation32(mean,x,shift);
        h=h64(h,&out,sizeof out);
    }
    if(h!=0x469bebd9e7be7b0bULL){fprintf(stderr,"Stage-B deviation32 hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT supplied-mean deviation regression");return 0;
}
