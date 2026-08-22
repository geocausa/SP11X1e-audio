#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t q=0x3a244a55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t hh(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<100000;t++){int32_t code[20];float acc[20];for(unsigned i=0;i<20;i++){code[i]=(int32_t)(ru()%1001u)-500;acc[i]=rf(-1,1);}ubig_stage_b_rt_telemetry_bias(code,acc,1u+ru()%20u,rf(-.5f,.5f),rf(-.5f,.5f),rf(-.5f,.5f),rf(-.5f,.5f));h=hh(h,code,sizeof code);h=hh(h,acc,sizeof acc);}if(h!=0x22294d1b2a24fa6aULL){fprintf(stderr,"Stage-B RT telemetry-bias hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT telemetry bias regression");return 0;}
