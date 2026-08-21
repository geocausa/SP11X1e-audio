#include "stage_b_leveler_primitives.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t rng=0x6a168123u;static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float lo,float hi){return lo+(hi-lo)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){uint64_t h=1469598103934665603ULL;for(unsigned n=0;n<20000;n++){uint32_t mode=ru()&1u;float c[3]={fr(.05f,.95f),fr(.05f,.95f),fr(.05f,.95f)};float a=fr(0,.999f),b=fr(0,.999f),v=fr(.70f,1.25f),o[3];ubig_stage_b_leveler_coeff_triplet(mode,c,a,b,v,&o[0],&o[1],&o[2]);h=h64(h,o,sizeof o);}if(h!=0xbb435c3d5066b2bcULL){fprintf(stderr,"Stage-B leveler primitive hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B leveler coefficient primitive regression");return 0;}
