#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0x38200a55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){enum{N=77,W=2*N};float a[W],b[W];uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<100000;t++){for(unsigned i=0;i<W;i++){a[i]=rf(-2.0f,2.0f);b[i]=rf(-2.0f,2.0f);}uint32_t n=ru()%(N+1u);float s=rf(.1f,.95f);ubig_stage_b_rt_pair_transform(a,b,n,s);h=h64(h,a,sizeof a);h=h64(h,b,sizeof b);}if(h!=0x923dba7f3410ff71ULL){fprintf(stderr,"Stage-B RT pair-transform hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT pair-transform regression");return 0;}
