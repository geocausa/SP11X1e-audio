#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0x42590a55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(void){uint32_t u=(ru()&0x807fffffu)|(((ru()%32u)+102u)<<23);float f;memcpy(&f,&u,4);return f;}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}return h;}
static void mock_fft(float*out,const float*in){for(uint32_t i=0;i<128u;i++)out[i]=in[(i*37u)&127u];}
int main(void){uint64_t h=UINT64_C(1469598103934665603);float filter[640],phase[128],src[64],state_mem[2][576],out[128];float *rows[2]={state_mem[0],state_mem[1]};uint32_t counter[2]={3u,8u};for(uint32_t i=0;i<640u;i++)filter[i]=rf();for(uint32_t i=0;i<128u;i++)phase[i]=rf();for(uint32_t r=0;r<2u;r++)for(uint32_t i=0;i<576u;i++)state_mem[r][i]=rf();UbigStageBRtHistoryFilter64State s={rows,counter,mock_fft};for(uint32_t t=0;t<20000u;t++){for(uint32_t i=0;i<64u;i++)src[i]=rf();ubig_stage_b_rt_history_filter64_process(&s,filter,phase,t&1u,out,src);h=h64(h,out,sizeof out);h=h64(h,state_mem,sizeof state_mem);h=h64(h,counter,sizeof counter);}if(h!=UINT64_C(0xd0118b361d08db6d)){fprintf(stderr,"history-filter64 hash mismatch=%016llx\n",(unsigned long long)h);return 1;}printf("PASS Stage-B RT history-filter64 hash=%016llx\n",(unsigned long long)h);return 0;}
