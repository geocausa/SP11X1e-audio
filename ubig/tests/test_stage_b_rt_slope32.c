#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x4e73b19du;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float scale){return ((int32_t)(ru()>>8))*(scale/8388608.0f);}
static float fb(uint32_t u){float f;memcpy(&f,&u,4);return f;}
static float p2(int e){return fb((uint32_t)(e+127)<<23);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    float row0[32],row1[32];UbigStageBRtSlope32 output;
    uint64_t h=1469598103934665603ULL;
    for(unsigned t=0;t<100000;t++){
        const float scale=p2((int)(ru()%25u)-12);
        for(unsigned i=0;i<32;i++){
            row0[i]=(ru()%23u==0u)?0.0f:fr(3.0f)*scale;
            row1[i]=(ru()%29u==0u)?0.0f:fr(3.0f)*scale;
        }
        if((t%127u)==0u){memset(row0,0,sizeof row0);memset(row1,0,sizeof row1);}
        ubig_stage_b_rt_slope32_prepare(row0,row1,&output);
        h=h64(h,&output,sizeof output);
    }
    if(h!=0x6cefd05c85465fdaULL){fprintf(stderr,"Stage-B slope32 hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT slope32 regression");return 0;
}
