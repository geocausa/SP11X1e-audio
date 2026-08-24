#include "stage_a_lowband.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}static void sf(unsigned char*p,unsigned o,float v){memcpy(p+o,&v,4);}
int main(void){unsigned char s[0x6c]={0};for(int c=0;c<8;c++)sf(s,0x24+4*c,.15f+c*.025f);struct ubig_stage_a_lowband_config cfg={5,.0375f,.021f,.72f,.31f,{.18f,.14f,.10f,.06f,.025f}};float in[3][20]={{0}},out[3][20]={{0}},*ip[3]={in[0],in[1],in[2]},*op[3]={out[0],out[1],out[2]};for(int c=0;c<3;c++)for(int i=0;i<20;i++){in[c][i]=-.38f+c*.047f+i*.006f;out[c][i]=-.02f+c*.01f+i*.001f;}struct ubig_float_rows ri={3,0,ip},ro={3,0,op};int32_t telemetry[8][20];ubig_stage_a_lowband_process(s,3,&cfg,&ri,&ro,telemetry);uint64_t h=1469598103934665603ULL;h=h64(h,s,sizeof s);h=h64(h,in,sizeof in);h=h64(h,out,sizeof out);h=h64(h,telemetry,sizeof telemetry);if(h!=0x2fe13a228b52eb15ULL){fprintf(stderr,"lowband hash %016llx != 2fe13a228b52eb15\n",(unsigned long long)h);return 2;}puts("PASS Stage A low-band proven-regression vector");return 0;}
