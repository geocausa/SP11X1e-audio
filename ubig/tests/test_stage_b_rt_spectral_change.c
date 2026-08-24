#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t rng=0x4d18c7a3u;static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){UbigStageBRtSpectralChangeHistory s={0};UbigStageBRtSpectralExport in={0};uint64_t h=1469598103934665603ULL;for(unsigned call=0;call<12000;call++){for(unsigned i=0;i<UBIG_STAGE_B_RT_SPECTRAL_BINS;i++)in.bins[i]=fr(-1.5f,1.5f);in.count=UBIG_STAGE_B_RT_SPECTRAL_BINS;in.exponent=(int32_t)(ru()%41)-20;in.aggregate=fr(.0001f,1.5f);ubig_stage_b_rt_spectral_change_process(&s,&in);h=h64(h,&s,sizeof s);}if(h!=0x17e4074bef4b1380ULL){fprintf(stderr,"Stage-B spectral-change hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT spectral-change regression");return 0;}
