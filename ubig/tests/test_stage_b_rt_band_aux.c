#include "../src/core/stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t s=0x8f1bbcddu;static uint32_t ru(void){s=s*1664525u+1013904223u;return s;}static float rf(void){int32_t x=(int32_t)(ru()>>8);return ((float)x)*(1.0f/8388608.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;while(n--){h^=*b++;h*=1099511628211ULL;}return h;}
int main(void){uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<50000;t++){float a[3][20],o[3][20],off[20];float *ap[3]={a[0],a[1],a[2]},*op[3]={o[0],o[1],o[2]};int32_t m[20];uint32_t rows=1+ru()%3,width=1+ru()%20;for(unsigned r=0;r<3;r++)for(unsigned i=0;i<20;i++){a[r][i]=rf();o[r][i]=rf();}for(unsigned i=0;i<20;i++){off[i]=rf()*.2f;m[i]=(int32_t)ru();}UbigStageBRtBandRows ar={rows,width,ap,0},or={rows,width,op,0};int32_t scale=(int32_t)(ru()%4097)-2048;ubig_stage_b_rt_band_aux_apply(&ar,&or,off,scale,(ru()&1)?m:NULL);h=h64(h,a,sizeof a);h=h64(h,o,sizeof o);h=h64(h,m,sizeof m);}if(h!=0xb3edc401cfc13040ULL){fprintf(stderr,"Stage-B RT band-aux hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT band-aux regression");return 0;}
