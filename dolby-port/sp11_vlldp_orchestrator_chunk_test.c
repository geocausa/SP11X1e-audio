#define _GNU_SOURCE
#include <dlfcn.h>
#include <ladspa.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef const LADSPA_Descriptor *(*DescFn)(unsigned long);

typedef struct { const unsigned *sizes; size_t count; const char *name; } Pattern;

static uint32_t rng_state=0x12345678u;
static float frand_signed(void){
    rng_state=rng_state*1664525u+1013904223u;
    return ((float)((rng_state>>8)&0xFFFFFFu)/(float)0x800000u)-1.0f;
}

static void make_input(float *l,float *r,size_t n){
    rng_state=0x12345678u;
    for(size_t i=0;i<n;i++){
        double t=(double)i/48000.0;
        float noise=0.006f*frand_signed();
        float a=0.12f*sinf((float)(2.0*M_PI*997.0*t))+0.04f*sinf((float)(2.0*M_PI*113.0*t))+noise;
        float b=0.09f*sinf((float)(2.0*M_PI*701.0*t))+0.03f*sinf((float)(2.0*M_PI*181.0*t))-noise*0.7f;
        if((i/48000)%7==3 && (i%48000)<8000){a=b=0.0f;}
        if(i%65537==0){a+=0.7f;b-=0.55f;}
        l[i]=a;r[i]=b;
    }
}

static int run_pattern(const LADSPA_Descriptor *d,const float *inl,const float *inr,float *outl,float *outr,size_t n,const Pattern *pat){
    LADSPA_Handle h=d->instantiate(d,48000); if(!h){fprintf(stderr,"instantiate failed %s\n",pat->name);return -1;}
    float bypass=0.0f;
    d->connect_port(h,4,&bypass);
    if(d->activate)d->activate(h);
    size_t pos=0,pi=0;
    while(pos<n){
        unsigned want=pat->sizes[pi++%pat->count]; if(!want)want=1;
        size_t take=want; if(take>n-pos)take=n-pos;
        d->connect_port(h,0,(LADSPA_Data*)(inl+pos));
        d->connect_port(h,1,(LADSPA_Data*)(inr+pos));
        d->connect_port(h,2,outl+pos);
        d->connect_port(h,3,outr+pos);
        d->run(h,take); pos+=take;
    }
    d->cleanup(h); return 0;
}

static uint64_t fnv1a(const void *vp,size_t n){const unsigned char*p=vp;uint64_t h=1469598103934665603ULL;for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ULL;}return h;}

int main(int argc,char**argv){
    const char *so=argc>1?argv[1]:"./sp11_vlldp_orchestrator.so";
    size_t n=argc>2?strtoull(argv[2],0,0):1000000ULL;
    void *lib=dlopen(so,RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
    DescFn df=(DescFn)dlsym(lib,"ladspa_descriptor");const LADSPA_Descriptor*d=df?df(0):NULL;if(!d)return 3;
    float *inl=malloc(n*sizeof(float)),*inr=malloc(n*sizeof(float)),*refL=calloc(n,sizeof(float)),*refR=calloc(n,sizeof(float)),*outL=calloc(n,sizeof(float)),*outR=calloc(n,sizeof(float));
    if(!inl||!inr||!refL||!refR||!outL||!outR)return 4;make_input(inl,inr,n);
    static const unsigned p1[]={1},p64[]={64},p480[]={480},p1024[]={1024},podd[]={127,353},pmix[]={31,257,509,17,1024,3,480,65};
    Pattern pats[]={{p1,1,"1"},{p64,1,"64"},{p480,1,"480"},{p1024,1,"1024"},{podd,2,"127/353"},{pmix,8,"mixed"}};
    if(run_pattern(d,inl,inr,refL,refR,n,&pats[0]))return 5;
    size_t startup_bad=0;for(size_t i=0;i<n&&i<256;i++)if(refL[i]!=0.0f||refR[i]!=0.0f)startup_bad++;
    size_t finite_bad=0;for(size_t i=0;i<n;i++)if(!isfinite(refL[i])||!isfinite(refR[i]))finite_bad++;
    printf("reference pattern=%s frames=%zu hashL=%016llx hashR=%016llx startup_bad=%zu finite_bad=%zu\n",pats[0].name,n,(unsigned long long)fnv1a(refL,n*4),(unsigned long long)fnv1a(refR,n*4),startup_bad,finite_bad);
    int fail=(startup_bad||finite_bad);
    for(size_t k=1;k<sizeof(pats)/sizeof(pats[0]);k++){
        memset(outL,0,n*4);memset(outR,0,n*4);if(run_pattern(d,inl,inr,outL,outR,n,&pats[k]))return 6;
        size_t diff=0,first=(size_t)-1;float maxd=0;
        for(size_t i=0;i<n;i++){
            if(memcmp(&outL[i],&refL[i],4)||memcmp(&outR[i],&refR[i],4)){if(first==(size_t)-1)first=i;diff++;}
            float dl=fabsf(outL[i]-refL[i]),dr=fabsf(outR[i]-refR[i]);if(dl>maxd)maxd=dl;if(dr>maxd)maxd=dr;
        }
        printf("pattern=%-8s diff_frames=%zu first=%s max_abs=%.9g hashL=%016llx hashR=%016llx\n",pats[k].name,diff,first==(size_t)-1?"none":"set",maxd,(unsigned long long)fnv1a(outL,n*4),(unsigned long long)fnv1a(outR,n*4));
        if(diff)fail=1;
    }
    free(inl);free(inr);free(refL);free(refR);free(outL);free(outR);dlclose(lib);
    printf("RESULT: %s\n",fail?"FAIL":"PASS");return fail?20:0;
}
