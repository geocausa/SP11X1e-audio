#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t q=0x56b80a55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(void){return ((int32_t)(ru()>>8)-0x7fffff)*(1.0f/8388608.0f);}static uint64_t hh(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){float data[8][20],acc[20];float*rp[8];for(unsigned r=0;r<8;r++)rp[r]=data[r];uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<100000;t++){unsigned nr=1u+ru()%8u,nb=1u+ru()%20u;for(unsigned r=0;r<8;r++)for(unsigned b=0;b<20;b++)data[r][b]=rf();for(unsigned b=0;b<20;b++)acc[b]=rf();UbigStageBRtBandRows rows={nr,nb,rp,20};ubig_stage_b_rt_linked_row_accumulate(&rows,acc);h=hh(h,acc,sizeof acc);}if(h!=0x70eb81e8929aadbbULL){fprintf(stderr,"Stage-B RT linked-accumulate hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT linked-row accumulator regression");return 0;}
