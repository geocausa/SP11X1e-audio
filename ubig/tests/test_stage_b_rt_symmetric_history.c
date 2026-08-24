#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0x6dcf8a55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(void){return ((int32_t)(ru()>>8)-0x7fffff)*(1.0f/8388608.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){enum{M=64,S=4};float k[M],hist[S*M],in[M],out[M];uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<50000;t++){uint32_t n=4u*(1u+ru()%16u),slot=ru()%S;for(unsigned i=0;i<M;i++){k[i]=rf();in[i]=rf();out[i]=rf();}for(unsigned i=0;i<S*M;i++)hist[i]=rf();UbigStageBRtSymmetricHistoryMix s={k,n,rf(),rf(),rf(),rf(),hist};ubig_stage_b_rt_symmetric_history_mix(&s,out,in,slot);h=h64(h,out,sizeof out);h=h64(h,hist,sizeof hist);}if(h!=0xc3af0d13d4cae940ULL){fprintf(stderr,"Stage-B RT symmetric-history hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT symmetric history mixer regression");return 0;}
