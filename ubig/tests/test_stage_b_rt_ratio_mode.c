#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t rng=0x2d7c4195u;static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){uint64_t h=1469598103934665603ULL;for(unsigned i=0;i<100000;i++){float x=fr(.000001f,4.0f);int32_t mode=(int32_t)(ru()%33u)-16;if((i%37u)==0u){x=1.0f;mode=0;}else if((i%41u)==0u)mode=3;else if((i%43u)==0u)mode=7;float y=ubig_stage_b_rt_ratio_map_mode(x,mode);h=h64(h,&y,4);}if(h!=0xe4c286a800ac8bd9ULL){fprintf(stderr,"Stage-B ratio-mode hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT generalized ratio-map regression");return 0;}
