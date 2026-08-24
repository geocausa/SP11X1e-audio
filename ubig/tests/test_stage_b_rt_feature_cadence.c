#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x997d8c43u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float scale){return ((int32_t)(ru()>>8))*(scale/8388608.0f);}
static float fb(uint32_t u){float f;memcpy(&f,&u,4);return f;}
static float p2(int e){return fb((uint32_t)(e+127)<<23);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    UbigStageBRtFeatureHistory state;float output[UBIG_STAGE_B_RT_FEATURE_CADENCE_OUTPUTS];
    memset(&state,0,sizeof state);state.phase=0u;uint64_t h=1469598103934665603ULL;
    for(unsigned t=0;t<20000u;t++){
        state.index=state.phase;
        const float scale=p2((int)(ru()%17u)-8);
        for(unsigned row=0;row<UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH;row++){
            for(unsigned column=0;column<UBIG_STAGE_B_RT_FEATURE_RECORD_VALUES;column++){
                float value=(column==1u)?(0.5f+fr(0.49f)):fr(2.0f);
                if((ru()%37u)==0u)value=0.0f;
                state.records[row][column]=value*scale;
            }
        }
        for(unsigned lane=0;lane<UBIG_STAGE_B_RT_FEATURE_SEGMENTS;lane++){
            state.segment_sum[lane]=fr(4.0f)*scale;
            state.segment_shift[lane]=ru()%31u;
        }
        for(unsigned lane=0;lane<UBIG_STAGE_B_RT_FEATURE_SEGMENTS-1u;lane++){
            state.delta_sum[lane]=fr(4.0f)*scale;
            state.delta_shift[lane]=ru()%31u;
        }
        const uint32_t step=1u+ru()%31u;
        memset(output,0xa5,sizeof output);
        ubig_stage_b_rt_feature_cadence_process(&state,step,output);
        h=h64(h,output,sizeof output);h=h64(h,&state.phase,sizeof state.phase);
    }
    if(h!=0x9c8318bbb8e0b00bULL){fprintf(stderr,"Stage-B feature-cadence hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT feature-cadence regression");return 0;
}
