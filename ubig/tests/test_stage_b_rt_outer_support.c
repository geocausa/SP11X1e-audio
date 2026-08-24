#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=UINT32_C(0x45600a55);
static uint32_t ru(void){rng=rng*UINT32_C(1664525)+UINT32_C(1013904223);return rng;}
static uint32_t r32(const void *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint64_t r64(const void *p){uint64_t v;memcpy(&v,p,8);return v;}
static uint64_t h64(uint64_t h,const void *p,size_t n){const unsigned char *b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}return h;}
static uint32_t rel(const void *base,uint64_t p){return (uint32_t)((uintptr_t)p-(uintptr_t)base);}
#define HASH(v) h=h64(h,&(v),sizeof(v))

int main(void){
    _Alignas(32) unsigned char arena[0x1700];
    uint64_t h=UINT64_C(1469598103934665603);
    for(uint32_t n=0;n<30000u;n++){
        const uint32_t width=ru()%21u,active=ru()%21u,offset=ru()%32u;
        memset(arena,0xa5,sizeof arena);
        void *base=arena+offset;
        unsigned char *root=ubig_stage_b_rt_outer_support_build(width,active,base);
        if(!root)return 2;
        const uint32_t root_off=(uint32_t)(root-(unsigned char *)base);
        HASH(width);HASH(active);HASH(offset);HASH(root_off);

        uint32_t top[7];
        for(uint32_t i=0;i<7u;i++){top[i]=rel(base,r64(root+(size_t)i*8u));HASH(top[i]);}
        unsigned char *three=(unsigned char *)base+top[0];
        unsigned char *pair=(unsigned char *)base+top[1];
        unsigned char *two=(unsigned char *)base+top[2];
        unsigned char *four=(unsigned char *)base+top[3];
        unsigned char *bands=(unsigned char *)base+top[4];
        unsigned char *controller=(unsigned char *)base+top[5];
        unsigned char *tail=(unsigned char *)base+top[6];

        for(uint32_t i=0;i<2u;i++){uint32_t ro=rel(base,r64(two+8u*i));HASH(ro);h=h64(h,(unsigned char *)base+ro,80u);}
        uint32_t tw=r32(two+16),one=r32(two+20),eps=r32(two+24);HASH(tw);HASH(one);HASH(eps);

        uint32_t neg=rel(base,r64(pair+8)),zero=rel(base,r64(pair+16));HASH(neg);HASH(zero);h=h64(h,(unsigned char *)base+neg,80u);h=h64(h,(unsigned char *)base+zero,80u);uint32_t lim=r32(pair+28);HASH(lim);

        uint32_t four_rows=rel(base,r64(four));HASH(four_rows);
        for(uint32_t i=0;i<4u;i++){uint32_t ro=rel(base,r64((unsigned char *)base+four_rows+8u*i));HASH(ro);h=h64(h,(unsigned char *)base+ro,80u);}
        uint32_t three_rows=rel(base,r64(three));HASH(three_rows);
        for(uint32_t i=0;i<3u;i++){uint32_t ro=rel(base,r64((unsigned char *)base+three_rows+8u*i));HASH(ro);h=h64(h,(unsigned char *)base+ro,80u);}

        uint32_t reca=rel(base,r64(controller+16)),recb=rel(base,r64(controller+24));HASH(reca);HASH(recb);
        for(uint32_t i=0;i<4u;i++){
            unsigned char *a=(unsigned char *)base+reca+16u*i,*b=(unsigned char *)base+recb+16u*i;
            uint32_t ao=rel(base,r64(a)),bo=rel(base,r64(b)),av=r32(a+8),bv=r32(b+8);HASH(ao);HASH(bo);HASH(av);HASH(bv);h=h64(h,(unsigned char *)base+ao,80u);h=h64(h,(unsigned char *)base+bo,80u);
        }
        h=h64(h,controller+0x20,0x5e8u);

        uint64_t one64=r64(bands+672);HASH(one64);
        for(uint32_t g=0;g<4u;g++){uint32_t a=r32(bands+0x140u+4u*g),b=r32(bands+0x140u+4u*g+336u);HASH(a);HASH(b);}
        h=h64(h,bands,0x2a8u);

        uint32_t ta=rel(base,r64(tail)),tb=rel(base,r64(tail+8));HASH(ta);HASH(tb);h=h64(h,(unsigned char *)base+ta,80u);h=h64(h,(unsigned char *)base+tb,80u);
    }
    if(h!=UINT64_C(0xb6629040d8469bdb)){fprintf(stderr,"Stage-B RT outer-support hash %016llx\n",(unsigned long long)h);return 3;}
    puts("PASS Stage-B RT outer-support regression");
    return 0;
}
