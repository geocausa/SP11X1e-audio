#define main sp11_vr_probe_embedded_main
#include "sp11_vr_init_probe.c"
#undef main
#include <inttypes.h>

typedef struct { uint8_t *o,*arena; uint8_t cfg[0x60] __attribute__((aligned(16))); } VrInst;

static int vr_new(Sp11PeImage *img,VrInst *v){
    memset(v,0,sizeof(*v));
    v->o=aligned_alloc(64,0x200); if(!v->o)return -1; memset(v->o,0,0x200);
    ((CtorFn)sp11_pe_ptr_for_va(img,VR_CTOR_VA))(v->o);
    const size_t asz=8u*1024u*1024u;
    if(posix_memalign((void**)&v->arena,64,asz)) return -2;
    memset(v->arena,0,asz);
    memset(v->cfg,0,sizeof(v->cfg));
    wd(v->cfg,0,48000);wd(v->cfg,4,2);wd(v->cfg,8,2);wd(v->cfg,0xc,2);
    wq(v->cfg,0x20,asz);wq(v->cfg,0x28,(uintptr_t)v->arena);wq(v->cfg,0x30,(uintptr_t)v->arena);
    uint64_t rc=((InitFn)sp11_pe_ptr_for_va(img,VR_INIT_VA))(v->o,v->cfg);
    if(!rc){fprintf(stderr,"vr init failed\n");return -3;}
    return 0;
}
static void vr_free(VrInst*v){free(v->arena);free(v->o);memset(v,0,sizeof(*v));}
static void fill_input(float*x,size_t n){
    uint32_t z=0x12345678u;
    for(size_t i=0;i<n;i++){z=z*1664525u+1013904223u; float noise=((int32_t)(z>>8))/8388608.0f; double t=i/48000.0; x[2*i]=(float)(.07*sin(2*M_PI*997*t)+.015*noise); x[2*i+1]=(float)(.055*sin(2*M_PI*1553*t+.31)-.012*noise);}
}
static int run_pattern(Sp11PeImage*img,const float*in,float*out,size_t n,const unsigned*pat,size_t np,const char*name,const float*ref){
    VrInst v;if(vr_new(img,&v))return 1; ProcessFn proc=(ProcessFn)sp11_pe_ptr_for_va(img,VR_PROCESS_VA);
    uint64_t a0=g_heap_alloc_calls,f0=g_heap_free_calls,r0=g_heap_realloc_calls;
    size_t off=0,k=0;while(off<n){unsigned c=pat[k++%np];if(c>n-off)c=(unsigned)(n-off);proc(v.o,out+2*off,in+2*off,c);off+=c;}
    size_t first=(size_t)-1;float md=0; if(ref){for(size_t i=0;i<2*n;i++){if(memcmp(&out[i],&ref[i],4)){if(first==(size_t)-1)first=i;float d=fabsf(out[i]-ref[i]);if(d>md)md=d;}}}
    printf("%-10s calls=%zu fill=%u heap_delta=%"PRIu64"/%"PRIu64"/%"PRIu64, name,k,d(v.o,0x20),g_heap_alloc_calls-a0,g_heap_free_calls-f0,g_heap_realloc_calls-r0);
    if(ref) printf(" equal=%s first_diff=%s max_abs=%g",first==(size_t)-1?"YES":"NO",first==(size_t)-1?"-":"set",md);
    puts("");
    vr_free(&v); return first!=(size_t)-1;
}
int main(int ac,char**av){
    const char*dll=ac>1?av[1]:"DolbyApoVr.dll";hs();Sp11PeImage img;if(sp11_pe_load(&img,dll))return 2;g_resource_data=sp11_pe_ptr_for_va(&img,VR_RESOURCE_VA);patch_runtime(&img);
    ((VoidFn)sp11_pe_ptr_for_va(&img,VR_RATE_MAP_INIT_VA))(); init_empty_property_map(); *(uintptr_t*)sp11_pe_ptr_for_va(&img,VR_VTABLE_VA+0x18)=(uintptr_t)shim_GetEmptyPropertyMap;
    const size_t N=1000000;float*in=malloc(2*N*sizeof(float)),*ref=calloc(2*N,sizeof(float)),*out=calloc(2*N,sizeof(float));if(!in||!ref||!out)return 3;fill_input(in,N);
    unsigned p_ref[]={1000000}; if(run_pattern(&img,in,ref,N,p_ref,1,"reference",NULL))return 4;
    struct T{const char*n;unsigned p[12];size_t np;} tests[]={
      {"one",{1},1},{"64",{64},1},{"480",{480},1},{"1024",{1024},1},{"odd",{127,353},2},{"mixed",{1,17,64,127,480,1024,3,511,29,256,777,5},12}
    };
    int bad=0;for(size_t t=0;t<sizeof(tests)/sizeof(tests[0]);t++){memset(out,0,2*N*sizeof(float));bad|=run_pattern(&img,in,out,N,tests[t].p,tests[t].np,tests[t].n,ref);}
    printf("RESULT %s total_heap=%"PRIu64" frees=%"PRIu64" reallocs=%"PRIu64"\n",bad?"FAIL":"PASS",g_heap_alloc_calls,g_heap_free_calls,g_heap_realloc_calls);return bad?5:0;
}
