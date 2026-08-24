#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>

static uint32_t rng=0x63a91d47u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void *p,size_t n){const unsigned char *b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void)
{
    UbigStageBRtStatCursor cursor={7u,13u};
    float input[32],scratch[32],output[2];
    uint64_t h=1469598103934665603ULL;
    for(unsigned call=0;call<100000;call++){
        for(unsigned lane=0;lane<32;lane++)
            input[lane]=(ru()%19u==0u)?0.0f:fr(-2.0f,2.0f);
        cursor.step=ru()%32u;
        ubig_stage_b_rt_stat32_step(&cursor,input,scratch,output);
        h=h64(h,output,sizeof output);
        h=h64(h,scratch,sizeof scratch);
        h=h64(h,&cursor,sizeof cursor);
    }
    if(h!=0xef736d1ae28c87ceULL){
        fprintf(stderr,"Stage-B stat32 hash %016llx\n",(unsigned long long)h);
        return 2;
    }
    puts("PASS Stage-B RT 32-value statistic regression");
    return 0;
}
