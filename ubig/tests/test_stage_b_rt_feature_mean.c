#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t rng=0x5f81c2b7u;static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){float r[32][20];uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<100000;t++){for(unsigned i=0;i<32;i++)for(unsigned j=0;j<20;j++)r[i][j]=(ru()%17u==0u)?0.0f:fr(-2,2);if(t%997u==0u)for(unsigned i=0;i<32;i++)r[i][1]=0.0f;float v=ubig_stage_b_rt_feature_history_mean(r);h=h64(h,&v,sizeof v);}if(h!=0x0830f86ff2f1ce3cULL){fprintf(stderr,"Stage-B feature-mean hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT feature-history mean regression");return 0;}
