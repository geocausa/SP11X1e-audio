#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t rng=0x4a570a55u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float rf(void){uint32_t u=(ru()&0x007fffffu)|(((ru()%40u)+100u)<<23);if(ru()&1u)u|=0x80000000u;float f;memcpy(&f,&u,4);return f;}
static void hash_bytes(uint64_t *h,const void *p,size_t n){const uint8_t *b=p;for(size_t i=0;i<n;i++){*h^=b[i];*h*=UINT64_C(1099511628211);}}
int main(void){
    uint64_t h=UINT64_C(1469598103934665603);
    for(uint32_t t=0;t<20000u;t++){
        const uint32_t count=1u+ru()%16u;float a[32],b[32],tail[6],out[32];for(uint32_t i=0;i<32u;i++){a[i]=rf();b[i]=rf();}for(uint32_t i=0;i<6u;i++)tail[i]=rf();
        _Alignas(64) uint8_t base[16u*24u+96u+64u];const size_t bytes=count*24u+96u;memset(base,0,sizeof base);const uint32_t off=ru()%17u;float *raw=(float*)(base+off);float *aligned=(float*)(((uintptr_t)raw+31u)&~(uintptr_t)31u);for(uint32_t i=0;i<count*6u;i++)aligned[i]=rf();float *rows[1]={raw};
        UbigStageBRtHistoryTransform32 s={rows,a,b,count,(int32_t)(ru()%17u)-8};ubig_stage_b_rt_history_transform32(&s,0u,out,tail);hash_bytes(&h,out,sizeof out);hash_bytes(&h,base,bytes);
    }
    if(h!=UINT64_C(0x53ef0081132f4037)){fprintf(stderr,"history32 hash mismatch=%016llx\n",(unsigned long long)h);return 1;}printf("PASS Stage-B RT history32 hash=%016llx\n",(unsigned long long)h);return 0;
}
