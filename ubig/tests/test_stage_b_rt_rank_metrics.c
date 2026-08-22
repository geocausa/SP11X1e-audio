#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x6b51d2a3u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    float input[32],copy[32],scratch[32];
    uint64_t h=1469598103934665603ULL;
    for(unsigned t=0;t<50000;t++){
        for(unsigned i=0;i<32;i++)input[i]=(ru()%19u==0u)?0.0f:fr(0.0f,3.0f);
        memcpy(copy,input,sizeof input);
        const float gain=(ru()%23u==0u)?0.0f:fr(0.0f,2.0f);
        float peak=fr(-9.0f,9.0f),ratio=fr(-9.0f,9.0f);
        if((ru()&7u)==0u){
            ubig_stage_b_rt_rank_metrics(gain,copy,copy,&peak,&ratio);
            h=h64(h,copy,sizeof copy);
        }else{
            for(unsigned i=0;i<32;i++)scratch[i]=fr(-9.0f,9.0f);
            ubig_stage_b_rt_rank_metrics(gain,copy,scratch,&peak,&ratio);
            h=h64(h,copy,sizeof copy);
            h=h64(h,scratch,sizeof scratch);
        }
        h=h64(h,&peak,sizeof peak);
        h=h64(h,&ratio,sizeof ratio);
    }
    if(h!=0x9a0861d04a41b2fdULL){
        fprintf(stderr,"Stage-B rank-metrics hash %016llx\n",(unsigned long long)h);
        return 2;
    }
    puts("PASS Stage-B RT rank-metrics regression");
    return 0;
}
