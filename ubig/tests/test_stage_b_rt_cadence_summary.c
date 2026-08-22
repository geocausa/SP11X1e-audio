#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>

static uint32_t rng=0x4c17a2d9u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    UbigStageBRtCadenceSummary s={0};float out[UBIG_STAGE_B_RT_CADENCE_OUTPUTS],scratch[32];
    uint64_t h=1469598103934665603ULL;
    for(unsigned t=0;t<50000;t++){
        for(unsigned r=0;r<32;r++)for(unsigned c=0;c<UBIG_STAGE_B_RT_CADENCE_COLUMNS;c++)
            s.matrix[r][c]=(ru()%17u==0u)?0.0f:fr(-2.0f,2.0f);
        s.cursor.step=ru()%32u;s.cursor.index=ru()%32u;
        for(unsigned c=0;c<UBIG_STAGE_B_RT_CADENCE_COLUMNS;c++){
            s.column_accumulator[c]=(ru()%19u==0u)?0.0f:fr(-2.0f,2.0f);
            s.column_shift[c]=ru()%61u;
        }
        for(unsigned c=0;c<UBIG_STAGE_B_RT_CADENCE_DELTAS;c++){
            s.delta_accumulator[c]=(ru()%19u==0u)?0.0f:fr(-2.0f,2.0f);
            s.delta_shift[c]=ru()%61u;
        }
        ubig_stage_b_rt_cadence_summary_process(&s,out,scratch);
        h=h64(h,out,sizeof out);h=h64(h,scratch,sizeof scratch);h=h64(h,&s.cursor,sizeof s.cursor);
    }
    if(h!=0x2fa8b774beb5b760ULL){fprintf(stderr,"Stage-B cadence-summary hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT cadence-summary regression");return 0;
}
