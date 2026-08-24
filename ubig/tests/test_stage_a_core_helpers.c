#include "stage_a_core_helpers.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){
 float a[2][20],base[2][20],main[2][20],comp[2][20],*ap[2]={a[0],a[1]},*bp[2]={base[0],base[1]},*mp[2]={main[0],main[1]},*cp[2]={comp[0],comp[1]};
 for(int ch=0;ch<2;ch++)for(int b=0;b<20;b++){a[ch][b]=-.91f+ch*.037f+b*.031f;base[ch][b]=-.04f+ch*.006f+b*.00125f;}
 struct ubig_float_rows ar={2,20,ap},br={2,20,bp},mr={2,20,mp},cr={2,20,cp};
 ubig_stage_a_build_rows(2,1,&ar,&br,&mr,&cr);
 float gains[40];for(int i=0;i<40;i++)gains[i]=-99.0f;ubig_stage_a_store_phase_gains(gains,3,base[1]);
 uint64_t h=1469598103934665603ULL;h=h64(h,main,sizeof main);h=h64(h,comp,sizeof comp);h=h64(h,gains,sizeof gains);
 if(h!=0x326a3f6d006e436cULL){fprintf(stderr,"core helper hash %016llx\n",(unsigned long long)h);return 2;}
 memset(main,0,sizeof main);memset(comp,0,sizeof comp);ubig_stage_a_build_rows(2,0,&ar,&br,&mr,&cr);if(memcmp(main,base,sizeof base)||memcmp(comp,base,sizeof base))return 3;
 puts("PASS Stage A orchestration helper regression");return 0;
}
