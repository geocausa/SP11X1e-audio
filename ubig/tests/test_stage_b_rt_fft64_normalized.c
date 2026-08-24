#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0xd15e0a55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(void){uint32_t u=(ru()&0x807fffffu)|(((ru()%36u)+100u)<<23);float f;memcpy(&f,&u,4);return f;}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}return h;}
int main(void){uint64_t h=UINT64_C(1469598103934665603);float in[128],out[128];for(uint32_t t=0;t<20000u;t++){for(uint32_t i=0;i<128u;i++)in[i]=rf();ubig_stage_b_rt_fft64_normalized(out,in);h=h64(h,out,sizeof out);}if(h!=UINT64_C(0xa51d8a3291486a98)){fprintf(stderr,"normalized FFT64 hash mismatch=%016llx\n",(unsigned long long)h);return 1;}printf("PASS Stage-B RT normalized FFT64 hash=%016llx\n",(unsigned long long)h);return 0;}
