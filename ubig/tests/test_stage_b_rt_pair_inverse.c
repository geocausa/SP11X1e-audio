#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x38528a55u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float rf(void){uint32_t u=(ru()&0x807fffffu)|(((ru()%60u)+96u)<<23);float f;memcpy(&f,&u,4);return f;}
static void reference(float*a,float*b,uint32_t n,float s){
    for(uint32_t k=0;k<n;k++){
        float ar=a[2*k],ai=a[2*k+1],br=b[2*k],bi=b[2*k+1];
        float ars,ais,ao,aio,bo,bio;
#if defined(__aarch64__)
        __asm__ volatile("fmul %s0,%s1,%s2":"=w"(ars):"w"(ar),"w"(s));
        __asm__ volatile("fmul %s0,%s1,%s2":"=w"(ais):"w"(ai),"w"(s));
        __asm__ volatile("fmadd %s0,%s1,%s2,%s3":"=w"(ao):"w"(br),"w"(s),"w"(ars));
        __asm__ volatile("fmadd %s0,%s1,%s2,%s3":"=w"(aio):"w"(bi),"w"(s),"w"(ais));
        __asm__ volatile("fmsub %s0,%s1,%s2,%s3":"=w"(bo):"w"(br),"w"(s),"w"(ars));
        __asm__ volatile("fmsub %s0,%s1,%s2,%s3":"=w"(bio):"w"(bi),"w"(s),"w"(ais));
#else
        ars=ar*s;ais=ai*s;ao=__builtin_fmaf(br,s,ars);aio=__builtin_fmaf(bi,s,ais);
        bo=__builtin_fmaf(-br,s,ars);bio=__builtin_fmaf(-bi,s,ais);
#endif
        a[2*k]=ao;a[2*k+1]=aio;b[2*k]=bo;b[2*k+1]=bio;
    }
}
int main(void){
    enum{N=77,W=154};float a[W],b[W],c[W],d[W];uint64_t h=UINT64_C(1469598103934665603);
    for(uint32_t t=0;t<1000000u;t++){
        uint32_t n=ru()%(N+1u);float s;uint32_t su=(ru()&0x007fffffu)|0x3e800000u;memcpy(&s,&su,4);
        for(uint32_t i=0;i<W;i++){a[i]=rf();b[i]=rf();c[i]=a[i];d[i]=b[i];}
        reference(a,b,n,s);ubig_stage_b_rt_pair_inverse_transform(c,d,n,s);
        if(memcmp(a,c,sizeof a)||memcmp(b,d,sizeof b)){fprintf(stderr,"inverse pair mismatch t=%u n=%u s=%a\n",t,n,s);return 1;}
        for(uint32_t i=0;i<2u*n;i++){uint32_t u;memcpy(&u,&c[i],4);h^=u;h*=UINT64_C(1099511628211);memcpy(&u,&d[i],4);h^=u;h*=UINT64_C(1099511628211);}
    }
    printf("PASS Stage-B RT inverse pair transform hash=%016llx\n",(unsigned long long)h);return 0;
}
