#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0xc04b4e51u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float rf(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
static void wf(uint32_t *p,float f){memcpy(p,&f,4);}

int main(void)
{
    UbigStageBRtControlAggregateState s={0};uint64_t h=1469598103934665603ULL;
    for(unsigned t=0;t<50000u;t++){
        if((t%11u)==0u){
            memset(&s,0,sizeof s);s.enabled=(t%33u)!=0u;s.slot2_state=rf(-.15f,1.05f);
            s.hysteresis.response_a=rf(-.15f,1.05f);s.hysteresis.response_b=rf(-.15f,1.05f);s.hysteresis.response_c=rf(-.15f,1.05f);s.hysteresis.input=rf(-.1f,1.0f);
            s.hysteresis.countdown_scale=rf(.01f,.9f);s.hysteresis.countdown_bias=rf(-.1f,.1f);s.hysteresis.smoothed_input=rf(-.1f,1.0f);s.hysteresis.toggle_keep=rf(.1f,.99f);s.hysteresis.toggle_state=rf(0,1);s.hysteresis.countdown=(int32_t)(ru()%15u)-3;s.hysteresis.toggle=ru()&1u;
            s.smoothing_keep=rf(.1f,.99f);s.activity_alpha_low=rf(.1f,.99f);s.activity_alpha_high=rf(.1f,.99f);s.activity_state=rf(0,1);s.final_blend=rf(0,1);s.final_state=rf(0,1);
        }
        const unsigned n=ru()%5u;UbigStageBRtControlCadence controls[UBIG_STAGE_B_RT_CONTROL_BANK_CHANNELS];memset(controls,0,sizeof controls);
        for(unsigned i=0;i<n;i++){
            controls[i].primary_result[0]=ru()%8u;
            wf(&controls[i].primary_result[3],rf(-.1f,1.1f));wf(&controls[i].primary_result[5],rf(-.1f,1.1f));
            wf(&controls[i].primary_result[11],rf(-.1f,1.1f));wf(&controls[i].primary_result[13],rf(-.1f,1.1f));
            controls[i].secondary_result[0]=rf(-.1f,1.1f);
        }
        int32_t out[UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS];ubig_stage_b_rt_control_bank_export(&s,controls,n,out);
        h=h64(h,out,sizeof out);h=h64(h,&s,sizeof s);
    }
    if(h!=UINT64_C(0xe2aa9b498a3d9f5f)){fprintf(stderr,"Stage-B RT control-bank hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT control-bank regression");return 0;
}
