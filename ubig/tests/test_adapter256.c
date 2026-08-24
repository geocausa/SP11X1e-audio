#include "adapter256.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void identity(void *o,const float in[512],float out[512]){(void)o;memcpy(out,in,512*sizeof(float));}

static int run_schedule(const size_t *chunks,size_t nchunks,float *ol,float *or_)
{
    enum { N=4096 };
    float *il=malloc(N*sizeof(float)),*ir=malloc(N*sizeof(float));
    if(!il||!ir)return 1;
    for(int i=0;i<N;i++){il[i]=(float)(i+1)*0.001f;ir[i]=-(float)(i+1)*0.002f;}
    ubig_adapter256 a;ubig_adapter256_reset(&a);
    size_t pos=0,ci=0;
    while(pos<N){size_t n=chunks[ci++%nchunks];if(n>N-pos)n=N-pos;ubig_adapter256_process(&a,identity,NULL,il+pos,ir+pos,ol+pos,or_+pos,n);pos+=n;}
    int bad=0;
    for(int i=0;i<N;i++){
        float el=i<256?0.0f:il[i-256], er=i<256?0.0f:ir[i-256];
        if(ol[i]!=el||or_[i]!=er){fprintf(stderr,"delay mismatch at %d got %.9g %.9g exp %.9g %.9g\n",i,ol[i],or_[i],el,er);bad=1;break;}
    }
    free(il);free(ir);return bad;
}

int main(void)
{
    enum { N=4096 };
    float *a=calloc(N,sizeof(float)),*b=calloc(N,sizeof(float)),*c=calloc(N,sizeof(float)),*d=calloc(N,sizeof(float));
    if(!a||!b||!c||!d)return 2;
    const size_t s1[]={480},s2[]={1,64,127,353,1024,7,511};
    if(run_schedule(s1,1,a,b)||run_schedule(s2,sizeof(s2)/sizeof(s2[0]),c,d))return 3;
    if(memcmp(a,c,N*sizeof(float))||memcmp(b,d,N*sizeof(float))){fprintf(stderr,"chunk invariance failed\n");return 4;}
    puts("PASS adapter256: exact 256-frame delay + chunk invariance");
    return 0;
}
