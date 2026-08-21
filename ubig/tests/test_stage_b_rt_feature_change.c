#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t rng=0x781c42a5u;static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){UbigStageBRtFeatureChangeHistory s={0};float in[UBIG_STAGE_B_RT_FEATURE_COUNT],out[UBIG_STAGE_B_RT_FEATURE_COUNT];uint64_t h=1469598103934665603ULL;for(unsigned call=0;call<20000;call++){for(unsigned i=0;i<UBIG_STAGE_B_RT_FEATURE_COUNT;i++)in[i]=fr(-2,2);ubig_stage_b_rt_feature_change_process(&s,in,out);h=h64(h,&s,sizeof s);h=h64(h,out,sizeof out);}if(h!=0xe50402a9fd590cfdULL){fprintf(stderr,"Stage-B feature-change hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT feature-change regression");return 0;}
