#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t rng=0x59c217a3u;static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){float x[64];uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<100000;t++){uint32_t n=1u+ru()%64u;for(uint32_t i=0;i<n;i++)x[i]=fr(-4,4);int32_t e=(int32_t)(ru()%41u)-20;float y=ubig_stage_b_rt_scaled_sum(x,n,e);h=h64(h,&y,4);}if(h!=0x12c52764e464a67dULL){fprintf(stderr,"Stage-B scaled-sum hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT scaled-sum regression");return 0;}
