#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t q=0x4f1b8a55u;
static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}
static float rf(void){uint32_t u=(ru()&0x007fffffu)|0x3e800000u;float f;memcpy(&f,&u,4);return (ru()&1u)?f:-f;}
static uint64_t h64(uint64_t h,const void *p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){
    unsigned char arena[16384+64];float dense[12*12];uint64_t h=1469598103934665603ULL;
    for(unsigned n=0;n<30000;n++){
        uint32_t src=ru()%13u,tgt=ru()%13u,off=ru()%32u;
        for(uint32_t i=0;i<src*tgt;i++){dense[i]=(ru()%5u<2u)?0.0f:rf();if((ru()&31u)==0u)dense[i]=-0.0f;}
        memset(arena,0xa5,sizeof arena);
        void *workspace=arena+off;
        UbigStageBRtSparseRemapPlan *p=ubig_stage_b_rt_sparse_plan_build(src,tgt,dense,workspace);
        if(!p)return 2;
        uint32_t po=(uint32_t)((uintptr_t)p-(uintptr_t)workspace);
        uint32_t mo=(uint32_t)((uintptr_t)p->mixes-(uintptr_t)workspace);
        h=h64(h,&src,4);h=h64(h,&tgt,4);h=h64(h,&po,4);h=h64(h,&mo,4);
        for(uint32_t r=0;r<tgt;r++){
            const UbigStageBRtSparseMix *m=&p->mixes[r];
            uint32_t io=(uint32_t)((uintptr_t)m->indices-(uintptr_t)workspace);
            uint32_t wo=(uint32_t)((uintptr_t)m->weights-(uintptr_t)workspace);
            h=h64(h,&io,4);h=h64(h,&wo,4);h=h64(h,&m->count,4);
            h=h64(h,m->indices,(size_t)m->count*4u);h=h64(h,m->weights,(size_t)m->count*4u);
        }
    }
    if(h!=UINT64_C(0x11a40bff151a3d35)){fprintf(stderr,"Stage-B RT sparse-plan hash %016llx\n",(unsigned long long)h);return 3;}
    puts("PASS Stage-B RT sparse-plan regression");
    return 0;
}
