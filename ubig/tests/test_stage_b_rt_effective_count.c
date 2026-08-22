#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
static uint32_t rng=0xf93a8a55u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void){
    uint64_t h=UINT64_C(1469598103934665603);
    for(uint32_t i=0;i<1000000u;i++){
        uint32_t en=ru()&0xffu,blocked=ru()&3u,base=ru(),expanded=ru();
        uint32_t got=ubig_stage_b_rt_effective_count(en,blocked,base,expanded);
        uint32_t expected=(en!=0u&&blocked==0u&&expanded>base)?expanded:base;
        if(got!=expected){fprintf(stderr,"effective-count mismatch at %u\n",i);return 1;}
        for(unsigned b=0;b<4;b++){h^=(uint8_t)(got>>(8u*b));h*=UINT64_C(1099511628211);}
    }
    if(h!=UINT64_C(0xf0da54428e921941)){fprintf(stderr,"effective-count hash mismatch=%016llx\n",(unsigned long long)h);return 1;}
    printf("PASS Stage-B RT effective-count hash=%016llx\n",(unsigned long long)h);
    return 0;
}
