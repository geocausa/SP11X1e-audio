#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0x1c2638a5u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float fb(uint32_t u){float f;memcpy(&f,&u,4);return f;}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){uint64_t h=1469598103934665603ULL;static const uint32_t edge[]={0,0x80000000u,0x3f800000u,0xbf800000u,0x3f7fffffu,0xbf7fffffu,0x3a172c4du,0xba172c4du,0x3f000000u,0xbf000000u,0x3eaaaaabu,0xbeaaaaabu,0x00800000u,0x80800000u,1u,0x80000001u};for(unsigned i=0;i<100000;i++){float x;if(i<sizeof edge/sizeof edge[0])x=fb(edge[i]);else{x=fb((ru()&0x80000000u)|((ru()%127u)<<23)|(ru()&0x7fffffu));}int32_t y=ubig_stage_b_rt_q31_encode(x);h=h64(h,&y,sizeof y);}if(h!=0x022d210f8a601583ULL){fprintf(stderr,"Stage-B RT Q31 hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT Q31 conversion regression");return 0;}
