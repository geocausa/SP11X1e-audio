#include "../src/core/stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;while(n--){h^=*b++;h*=1099511628211ULL;}return h;}
static uint32_t ru(void){static uint32_t s=0x6d2b79f5u;s^=s<<13;s^=s>>17;s^=s<<5;return s;}
int main(void){
 uint64_t h=1469598103934665603ULL;
 for(unsigned t=0;t<50000;t++){
  UbigStageBRtBandControlMap m;memset(&m,0,sizeof m);int32_t freq[20],cent[20],target[20],out[20];
  int32_t f=20;for(unsigned i=0;i<20;i++){f+=(int32_t)(1u+ru()%1200u);if(f>19999)f=19999;freq[i]=f;cent[i]=f;target[i]=(int32_t)(ru()%385u)-192;out[i]=(int32_t)ru();}
  uint32_t a=ubig_stage_b_rt_band_control_map_prepare(&m,freq,20,cent,20);
  uint32_t b=ubig_stage_b_rt_band_target_apply(&m,out,target,-576,576);
  uint32_t c=ubig_stage_b_rt_band_control_map_prepare(&m,freq,20,cent,20);
  h=h64(h,&a,4);h=h64(h,&b,4);h=h64(h,&c,4);h=h64(h,&m,sizeof m);h=h64(h,out,sizeof out);
 }
 if(h!=0x5bc02567a90e1c28ULL){fprintf(stderr,"Stage-B RT band-control hash %016llx\n",(unsigned long long)h);return 2;}
 puts("PASS Stage-B RT band-control regression");return 0;
}
