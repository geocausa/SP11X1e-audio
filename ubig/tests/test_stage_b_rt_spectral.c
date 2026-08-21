#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t rng=0x71a9c255u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){
 float a[2*UBIG_STAGE_B_RT_SPECTRAL_BINS],b[2*UBIG_STAGE_B_RT_SPECTRAL_BINS];
 UbigStageBRtSpectralAccumulator s={0};s.period=16;s.exponent_offset=3;s.output_scale=1.0f;
 UbigStageBRtSpectralExport o={0};uint64_t h=1469598103934665603ULL;
 for(unsigned call=0;call<12000;call++){
  if((call%173u)==0u){s.period=1u+ru()%20u;s.exponent_offset=(int32_t)(ru()%13u)-6;s.output_scale=fr(.25f,1.75f);if(s.counter>=s.period)s.counter=0;}
  for(unsigned i=0;i<2*UBIG_STAGE_B_RT_SPECTRAL_BINS;i++){a[i]=fr(-1.5f,1.5f);b[i]=fr(-1.5f,1.5f);}
  ubig_stage_b_rt_spectral_accumulate(&s,a,b,&o);
  h=h64(h,&s,sizeof s);h=h64(h,&o,sizeof o);
 }
 if(h!=0x48d731d02294bb0fULL){fprintf(stderr,"Stage-B spectral hash %016llx\n",(unsigned long long)h);return 2;}
 puts("PASS Stage-B RT spectral accumulator regression");return 0;
}
