#define _GNU_SOURCE
#include <dlfcn.h>
#include <ladspa.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
typedef const LADSPA_Descriptor *(*DescFn)(unsigned long);
typedef struct{const unsigned*sizes;size_t count;const char*name;}Pattern;
static uint32_t rng_state=0x12345678u;
static float frand_signed(void){rng_state=rng_state*1664525u+1013904223u;return ((float)((rng_state>>8)&0xffffffu)/(float)0x800000u)-1.0f;}
static void make_input(float*l,float*r,size_t n){rng_state=0x12345678u;for(size_t i=0;i<n;i++){double t=(double)i/48000.0;float noise=.006f*frand_signed();float a=.12f*sinf((float)(2*M_PI*997*t))+.04f*sinf((float)(2*M_PI*113*t))+noise;float b=.09f*sinf((float)(2*M_PI*701*t))+.03f*sinf((float)(2*M_PI*181*t))-noise*.7f;if((i/48000)%7==3&&(i%48000)<8000)a=b=0;if(i%65537==0){a+=.7f;b-=.55f;}l[i]=a;r[i]=b;}}
static uint64_t fnv1a(const void*vp,size_t n){const unsigned char*p=vp;uint64_t h=1469598103934665603ULL;for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ULL;}return h;}
static int run(const LADSPA_Descriptor*d,const float*il,const float*ir,float*out,size_t n,const Pattern*pat){
 LADSPA_Handle h=d->instantiate(d,48000);if(!h)return -1;float bypass=0;d->connect_port(h,4,&bypass);if(d->activate)d->activate(h);
 size_t pos=0,pi=0;float *ol=calloc(n,sizeof(float)),*or=calloc(n,sizeof(float));if(!ol||!or)return -2;
 while(pos<n){unsigned want=pat->sizes[pi++%pat->count];if(!want)want=1;size_t take=want;if(take>n-pos)take=n-pos;d->connect_port(h,0,(float*)(il+pos));d->connect_port(h,1,(float*)(ir+pos));d->connect_port(h,2,ol+pos);d->connect_port(h,3,or+pos);d->run(h,take);pos+=take;}
 size_t bad=0;for(size_t i=0;i<n;i++){out[2*i]=ol[i];out[2*i+1]=or[i];if(!isfinite(ol[i])||!isfinite(or[i]))bad++;}
 if(d->cleanup)d->cleanup(h);free(ol);free(or);return bad?-3:0;
}
int main(int ac,char**av){const char*so=ac>1?av[1]:"./sp11_dolby_windows_chain.so";size_t n=ac>2?strtoull(av[2],0,0):200000;void*lib=dlopen(so,RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}DescFn df=(DescFn)dlsym(lib,"ladspa_descriptor");const LADSPA_Descriptor*d=df?df(0):0;if(!d)return 3;
 float*il=malloc(n*4),*ir=malloc(n*4),*ref=calloc(2*n,4),*out=calloc(2*n,4);if(!il||!ir||!ref||!out)return 4;make_input(il,ir,n);
 static const unsigned p1[]={1},p64[]={64},p480[]={480},p1024[]={1024},podd[]={127,353},pmix[]={31,257,509,17,1024,3,480,65};Pattern pats[]={{p1,1,"1"},{p64,1,"64"},{p480,1,"480"},{p1024,1,"1024"},{podd,2,"127/353"},{pmix,8,"mixed"}};
 if(run(d,il,ir,ref,n,&pats[0]))return 5;printf("reference hash=%016"PRIx64"\n",fnv1a(ref,2*n*4));int fail=0;
 for(size_t k=1;k<sizeof(pats)/sizeof(pats[0]);k++){memset(out,0,2*n*4);if(run(d,il,ir,out,n,&pats[k]))return 6;size_t diff=0;float maxd=0;for(size_t i=0;i<2*n;i++)if(memcmp(ref+i,out+i,4)){diff++;float z=fabsf(ref[i]-out[i]);if(z>maxd)maxd=z;}printf("%-9s diff=%zu max=%.9g hash=%016"PRIx64"\n",pats[k].name,diff,maxd,fnv1a(out,2*n*4));if(diff)fail=1;}
 printf("PLUGIN_RESULT %s\n",fail?"FAIL":"PASS");free(il);free(ir);free(ref);free(out);dlclose(lib);return fail?20:0;}
