#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t q=0x1c2638a5u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){uint64_t h=1469598103934665603ULL;for(unsigned i=0;i<100000;i++){float x;switch(i&15u){case 0:x=1.0f;break;case 1:x=-1.0f;break;case 2:x=0.0f;break;case 3:x=0.99999994f;break;case 4:x=-1.25f;break;default:x=rf(-1.5f,1.5f);}int32_t y=ubig_stage_b_rt_q31_encode(x);h=h64(h,&y,sizeof y);}if(h!=0x26cd2dbd9bad0811ULL){fprintf(stderr,"Stage-B RT Q31 hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT Q31 conversion regression");return 0;}
