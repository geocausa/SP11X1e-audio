#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0xbb6e0a55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(void){uint32_t u=(ru()&0x807fffffu)|((96u+ru()%50u)<<23);float f;memcpy(&f,&u,4);return f;}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){float v[80] __attribute__((aligned(16)));uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<100000;t++){uint32_t n=4u*(1u+ru()%20u);for(unsigned i=0;i<80;i++)v[i]=rf();if((t&255u)==0u){v[0]=0.0f;v[1]=-0.0f;}float r=ubig_stage_b_rt_max_abs4(v,n);h=h64(h,&r,sizeof r);}if(h!=0x4c720017ecc09f55ULL){fprintf(stderr,"Stage-B RT max-abs4 hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT max-abs4 regression");return 0;}
