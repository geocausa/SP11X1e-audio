#define _GNU_SOURCE
#define SP11_VR_OUTER_NO_MAIN
#include "sp11_vr_outer_probe.c"
#include <dlfcn.h>
#include <ladspa.h>
#include <inttypes.h>

#define CHAIN_VR_OUTER_SIZE VR_OUTER_SIZE

typedef const LADSPA_Descriptor *(*DescFn)(unsigned long);
typedef struct { const unsigned *sizes; size_t count; const char *name; } Pattern;
typedef struct { uint8_t *outer; uint8_t cfg[0x60] __attribute__((aligned(16))); } ChainVr;

static int chain_vr_new(Sp11PeImage *img, ChainVr *v){
    memset(v,0,sizeof(*v));
    if(posix_memalign((void**)&v->outer,64,CHAIN_VR_OUTER_SIZE)) return -1;
    memset(v->outer,0,CHAIN_VR_OUTER_SIZE);
    ((OuterCtorFn)sp11_pe_ptr_for_va(img,VR_OUTER_CTOR_VA))(v->outer);
    finish_outer_factory_vtables(img,v->outer);
    uint8_t *inner=(uint8_t*)(uintptr_t)q(v->outer,VR_INNER_PTR_OFF);
    if(inner!=v->outer+VR_INNER_OFF) return -2;
    if(init_outer_inner(img,v->outer,v->cfg)) return -3;
    uint32_t window,stride,enabled;
    dump_inner_tuple(img,inner,&window,&stride,&enabled);
    if(!((TransInitFn)sp11_pe_ptr_for_va(img,VR_TRANS_INIT_VA))
       (v->outer+VR_TRANS_OFF,inner,window,enabled,stride,2,2,0)) return -4;
    return 0;
}
static void chain_vr_free(ChainVr *v){ free(v->outer); memset(v,0,sizeof(*v)); }

static uint32_t rng_state=0x12345678u;
static float frand_signed(void){ rng_state=rng_state*1664525u+1013904223u; return ((float)((rng_state>>8)&0xffffffu)/(float)0x800000u)-1.0f; }
static void make_input(float *l,float *r,size_t n){
    rng_state=0x12345678u;
    for(size_t i=0;i<n;i++){
        double t=(double)i/48000.0;
        float noise=.006f*frand_signed();
        float a=.12f*sinf((float)(2.0*M_PI*997.0*t))+.04f*sinf((float)(2.0*M_PI*113.0*t))+noise;
        float b=.09f*sinf((float)(2.0*M_PI*701.0*t))+.03f*sinf((float)(2.0*M_PI*181.0*t))-noise*.7f;
        if((i/48000)%7==3 && (i%48000)<8000) a=b=0.0f;
        if(i%65537==0){a+=.7f;b-=.55f;}
        l[i]=a;r[i]=b;
    }
}
static uint64_t fnv1a(const void *vp,size_t n){ const unsigned char*p=vp;uint64_t h=1469598103934665603ULL;for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ULL;}return h; }

static int run_chain(const LADSPA_Descriptor *vd, Sp11PeImage *vrimg,
                     const float *inl,const float *inr,float *final,size_t n,
                     const Pattern *pat, uint64_t *vr_heap_delta){
    LADSPA_Handle vh=vd->instantiate(vd,48000); if(!vh){fprintf(stderr,"VLLDP instantiate failed %s\n",pat->name);return -1;}
    float bypass=0.0f; vd->connect_port(vh,4,&bypass); if(vd->activate)vd->activate(vh);
    ChainVr vr; if(chain_vr_new(vrimg,&vr)){fprintf(stderr,"VR instantiate failed %s\n",pat->name);vd->cleanup(vh);return -2;}
    OuterHotFn hot=(OuterHotFn)sp11_pe_ptr_for_va(vrimg,VR_OUTER_HOT_VA);
    float *stageL=malloc(n*sizeof(float)), *stageR=malloc(n*sizeof(float));
    float *vrin=malloc(2*n*sizeof(float));
    if(!stageL||!stageR||!vrin){free(stageL);free(stageR);free(vrin);chain_vr_free(&vr);vd->cleanup(vh);return -3;}
    memset(stageL,0,n*sizeof(float)); memset(stageR,0,n*sizeof(float)); memset(vrin,0,2*n*sizeof(float)); memset(final,0,2*n*sizeof(float));
    uint64_t a0=g_heap_alloc_calls,f0=g_heap_free_calls,r0=g_heap_realloc_calls;
    size_t pos=0,pi=0,calls=0; unsigned meta_bad=0,finite_bad=0;
    while(pos<n){
        unsigned want=pat->sizes[pi++%pat->count]; if(!want)want=1; size_t take=want; if(take>n-pos)take=n-pos;
        vd->connect_port(vh,0,(LADSPA_Data*)(inl+pos));vd->connect_port(vh,1,(LADSPA_Data*)(inr+pos));
        vd->connect_port(vh,2,stageL+pos);vd->connect_port(vh,3,stageR+pos);vd->run(vh,take);
        for(size_t i=0;i<take;i++){vrin[2*(pos+i)]=stageL[pos+i];vrin[2*(pos+i)+1]=stageR[pos+i];}
        Conn ci={vrin+2*pos,(uint32_t)take,1},co={final+2*pos,0,0};Conn*pi_conn=&ci,*po_conn=&co;
        hot(vr.outer+VR_RT_OFF,1,&pi_conn,1,&po_conn,NULL);
        if(co.frames!=take || co.flags!=1) meta_bad++;
        for(size_t i=0;i<2*take;i++) if(!isfinite(final[2*pos+i])) finite_bad++;
        pos+=take;calls++;
    }
    vr_heap_delta[0]=g_heap_alloc_calls-a0;vr_heap_delta[1]=g_heap_free_calls-f0;vr_heap_delta[2]=g_heap_realloc_calls-r0;
    printf("chain %-9s calls=%zu vr_state=%u meta_bad=%u finite_bad=%u vr_heap=%"PRIu64"/%"PRIu64"/%"PRIu64"\n",
           pat->name,calls,d(vr.outer+VR_TRANS_OFF,0),meta_bad,finite_bad,vr_heap_delta[0],vr_heap_delta[1],vr_heap_delta[2]);
    free(stageL);free(stageR);free(vrin);chain_vr_free(&vr);vd->cleanup(vh);
    return (meta_bad||finite_bad)?-4:0;
}

int main(int argc,char **argv){
    const char *vlso=argc>1?argv[1]:"./dolby-port/sp11_vlldp_scheduler.so";
    const char *vrdll=argc>2?argv[2]:"/home/geoca/Documents/SP11-PROJECT/00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/Research_Hub_Audio/SOURCE/Dolby/SpeakerDLLs/DolbyApoVr.dll";
    size_t n=argc>3?strtoull(argv[3],0,0):200000ULL;
    hs();
    void *lib=dlopen(vlso,RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"dlopen VLLDP: %s\n",dlerror());return 2;}
    DescFn df=(DescFn)dlsym(lib,"ladspa_descriptor");const LADSPA_Descriptor*vd=df?df(0):NULL;if(!vd)return 3;
    Sp11PeImage vrimg;if(init_dll(&vrimg,vrdll)){fprintf(stderr,"VR DLL load failed\n");return 4;}
    float *inl=malloc(n*sizeof(float)),*inr=malloc(n*sizeof(float)),*ref=calloc(2*n,sizeof(float)),*out=calloc(2*n,sizeof(float));if(!inl||!inr||!ref||!out)return 5;make_input(inl,inr,n);
    static const unsigned p1[]={1},p64[]={64},p480[]={480},p1024[]={1024},podd[]={127,353},pmix[]={31,257,509,17,1024,3,480,65};
    Pattern pats[]={{p1,1,"1"},{p64,1,"64"},{p480,1,"480"},{p1024,1,"1024"},{podd,2,"127/353"},{pmix,8,"mixed"}};
    uint64_t hd[3];if(run_chain(vd,&vrimg,inl,inr,ref,n,&pats[0],hd))return 6;
    size_t first_nonzero=n,bad=0;float peak=0;double s=0;for(size_t i=0;i<n;i++){float l=ref[2*i],r=ref[2*i+1];if(first_nonzero==n&&(l!=0.0f||r!=0.0f))first_nonzero=i;if(!isfinite(l)||!isfinite(r))bad++;float a=fabsf(l)>fabsf(r)?fabsf(l):fabsf(r);if(a>peak)peak=a;s+=(double)l*l+(double)r*r;}
    printf("reference frames=%zu first_nonzero=%zu rms=%.9g peak=%.9g finite_bad=%zu hash=%016"PRIx64"\n",n,first_nonzero,sqrt(s/(2*n)),peak,bad,fnv1a(ref,2*n*sizeof(float)));
    int fail=bad!=0;
    for(size_t k=1;k<sizeof(pats)/sizeof(pats[0]);k++){
        memset(out,0,2*n*sizeof(float));if(run_chain(vd,&vrimg,inl,inr,out,n,&pats[k],hd))return 7;
        size_t diff=0,first=(size_t)-1;float maxd=0;for(size_t i=0;i<2*n;i++){if(memcmp(&out[i],&ref[i],4)){if(first==(size_t)-1)first=i;diff++;float z=fabsf(out[i]-ref[i]);if(z>maxd)maxd=z;}}
        printf("compare %-9s diff_samples=%zu first=%s max_abs=%.9g hash=%016"PRIx64"\n",pats[k].name,diff,first==(size_t)-1?"none":"set",maxd,fnv1a(out,2*n*sizeof(float)));if(diff)fail=1;
    }
    printf("FULL_CHAIN_RESULT %s\n",fail?"FAIL":"PASS");
    free(inl);free(inr);free(ref);free(out);sp11_pe_unload(&vrimg);dlclose(lib);return fail?20:0;
}
