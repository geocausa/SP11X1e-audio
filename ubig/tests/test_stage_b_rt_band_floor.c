#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t rng=0x74220a55u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float rf(void){uint32_t u=(ru()&0x007fffffu)|(((ru()%24u)+112u)<<23);if(ru()&1u)u|=0x80000000u;float f;memcpy(&f,&u,4);return f;}
static void hb(uint64_t*h,const void*p,size_t n){const uint8_t*b=p;for(size_t i=0;i<n;i++){*h^=b[i];*h*=UINT64_C(1099511628211);}}
int main(void){uint64_t h=UINT64_C(1469598103934665603);for(uint32_t t=0;t<50000u;t++){uint32_t n=ru()%21u;float in[20],w[20],out[20],sum;for(uint32_t i=0;i<20;i++){in[i]=rf();uint32_t q=(ru()&0x003fffffu)|0x3e800000u;memcpy(&w[i],&q,4);out[i]=rf();}ubig_stage_b_rt_band_floor_normalize(in,w,n,out,&sum);hb(&h,out,n*4u);hb(&h,&sum,4);}if(h!=UINT64_C(0xec6fbb394aa2a5b8)){fprintf(stderr,"band-floor hash mismatch=%016llx\n",(unsigned long long)h);return 1;}printf("PASS Stage-B RT band-floor hash=%016llx\n",(unsigned long long)h);return 0;}
