#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x8ce60d91u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    uint64_t h=1469598103934665603ULL;
    for(unsigned t=0;t<100000u;t++){
        float x=fr(-3.0f,3.0f),g=fr(-4.0f,4.0f),b=fr(-64.0f,64.0f);
        if((t%101u)==0u)x=0.0f;
        if((t%103u)==0u)g=0.0f;
        if((t%107u)==0u)b=0.0f;
        const float y=ubig_stage_b_rt_control_transfer(x,g,b);h=h64(h,&y,sizeof y);
    }

    float features[256];
    UbigStageBRtControlTerm terms[4][12];
    UbigStageBRtControlDescriptor descriptors[4];
    UbigStageBRtControlGroup groups[4];
    uint32_t result[UBIG_STAGE_B_RT_CONTROL_RESULT_WORDS];
    for(unsigned t=0;t<50000u;t++){
        for(unsigned i=0;i<256u;i++)features[i]=fr(-3.0f,3.0f);
        if((t%11u)==0u)features[0]=fr(-0.1f,-0.002f);
        else if((t%13u)==0u)features[0]=-0.0015625f;
        uint32_t slots[7]={0u,1u,2u,3u,4u,5u,6u};
        for(int i=6;i>0;i--){const unsigned j=ru()%(unsigned)(i+1);const uint32_t z=slots[i];slots[i]=slots[j];slots[j]=z;}
        for(unsigned g=0;g<4u;g++){
            descriptors[g].term_count=ru()%13u;
            descriptors[g].transfer_gain=fr(-4.0f,4.0f);
            descriptors[g].transfer_bias=fr(-64.0f,64.0f);
            descriptors[g].terms=terms[g];
            for(unsigned i=0;i<descriptors[g].term_count;i++){
                terms[g][i].feature_index=(uint16_t)(ru()%256u);
                terms[g][i].exponent=(uint16_t)(ru()%31u);
                terms[g][i].scale=fr(-8.0f,8.0f);
                terms[g][i].weight=fr(-8.0f,8.0f);
                terms[g][i].center=fr(-3.0f,3.0f);
            }
            groups[g].output_index=slots[g];groups[g].descriptor=&descriptors[g];
        }
        for(unsigned i=0;i<UBIG_STAGE_B_RT_CONTROL_RESULT_WORDS;i++)result[i]=ru();
        ubig_stage_b_rt_control_select_process(features,groups,result);
        h=h64(h,result,sizeof result);
        float pair[2]={0.0f,0.0f};
        ubig_stage_b_rt_control_score_process(features,&descriptors[t&3u],pair);
        h=h64(h,pair,sizeof pair);
    }
    if(h!=0xcfad6506600a4b95ULL){fprintf(stderr,"Stage-B control-select hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT control-select regression");return 0;
}
