#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x58480c73u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    uint64_t h=1469598103934665603ULL;
    UbigStageBRtControlAggregateItem items[8];
    for(unsigned t=0;t<100000u;t++){
        UbigStageBRtControlAggregateState s;memset(&s,0,sizeof s);
        s.enabled=(t%19u)?1u:0u;
        s.slot2_state=fr(-.25f,1.1f);
        s.hysteresis.response_a=fr(-.25f,1.1f);
        s.hysteresis.response_b=fr(-.25f,1.1f);
        s.hysteresis.response_c=fr(-.25f,1.1f);
        s.hysteresis.input=fr(-.25f,1.1f);
        s.smoothing_keep=fr(.05f,.98f);
        s.activity_alpha_low=fr(.05f,.98f);
        s.activity_alpha_high=fr(.05f,.98f);
        s.activity_state=fr(-.2f,1.0f);
        s.hysteresis.countdown_scale=fr(.01f,1.0f);
        s.hysteresis.countdown_bias=fr(-.2f,.2f);
        s.hysteresis.smoothed_input=fr(-.2f,1.0f);
        s.hysteresis.toggle_keep=fr(.05f,.98f);
        s.hysteresis.toggle_state=fr(-.2f,1.0f);
        s.hysteresis.countdown=(int32_t)(ru()%12u)-2;
        s.hysteresis.toggle=ru()&1u;
        s.final_blend=fr(0.0f,1.0f);
        s.final_state=fr(-.2f,1.1f);
        const uint32_t count=ru()%9u;
        for(uint32_t i=0;i<count;i++){
            items[i].winner=ru()%8u;
            items[i].slot1_transfer=fr(-.2f,1.2f);
            items[i].slot2_transfer=fr(-.2f,1.2f);
            items[i].slot5_transfer=fr(-.2f,1.2f);
            items[i].slot6_transfer=fr(-.2f,1.2f);
            items[i].secondary_transfer=fr(-.2f,1.2f);
        }
        float out[UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS];
        ubig_stage_b_rt_control_aggregate_process(&s,items,count,out);
        h=h64(h,&s,sizeof s);h=h64(h,out,sizeof out);
    }
    if(h!=0xfa8f1c78e3c17089ULL){fprintf(stderr,"Stage-B control-aggregate hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT control-aggregate regression");return 0;
}
