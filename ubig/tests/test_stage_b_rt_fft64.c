#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0xa68c0f64u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(void){uint32_t u=(ru()&0x807fffffu)|(((ru()%40u)+100u)<<23);float f;memcpy(&f,&u,4);return f;}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){float in[UBIG_STAGE_B_RT_FFT64_FLOATS],out[UBIG_STAGE_B_RT_FFT64_FLOATS];uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<20000u;t++){for(unsigned i=0;i<UBIG_STAGE_B_RT_FFT64_FLOATS;i++)in[i]=rf();ubig_stage_b_rt_fft64(out,in);h=h64(h,out,sizeof out);}if(h!=0x5370d7a298fc74d9ULL){fprintf(stderr,"Stage-B RT FFT64 hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT FFT64 regression");return 0;}
