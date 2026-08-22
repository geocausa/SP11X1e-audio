#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0x7fc08a55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(void){return ((int32_t)(ru()>>8)-0x7fffff)*(1.0f/8388608.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){int32_t st[20];float in[20],out[20];uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<100000;t++){uint32_t n=ru()%21u;for(unsigned i=0;i<20;i++){st[i]=(int32_t)(ru()%3u)-1;in[i]=rf();out[i]=rf();}ubig_stage_b_rt_neighbor_smooth(n,st,in,out);h=h64(h,out,sizeof out);}if(h!=0x429de12325cd4eacULL){fprintf(stderr,"Stage-B RT neighbor-smooth hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT neighbor smoother regression");return 0;}
