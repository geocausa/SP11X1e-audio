#define SP11_VR_OUTER_NO_MAIN
#include "sp11_vr_outer_probe.c"

typedef struct { uint8_t *outer; uint8_t cfg[0x60] __attribute__((aligned(16))); } OuterInst;
typedef struct { uint8_t *inner,*arena; uint8_t cfg[0x60] __attribute__((aligned(16))); } DirectInst;

static int outer_new(Sp11PeImage*img,OuterInst*v){
    memset(v,0,sizeof(*v)); if(posix_memalign((void**)&v->outer,64,VR_OUTER_SIZE))return -1; memset(v->outer,0,VR_OUTER_SIZE);
    ((OuterCtorFn)sp11_pe_ptr_for_va(img,VR_OUTER_CTOR_VA))(v->outer);
    finish_outer_factory_vtables(img,v->outer);
    uint8_t *inner=(uint8_t*)(uintptr_t)q(v->outer,VR_INNER_PTR_OFF); if(inner!=v->outer+VR_INNER_OFF)return -2;
    if(init_outer_inner(img,v->outer,v->cfg))return -3;
    uint32_t window,stride,enabled;dump_inner_tuple(img,inner,&window,&stride,&enabled);
    if(!((TransInitFn)sp11_pe_ptr_for_va(img,VR_TRANS_INIT_VA))(v->outer+VR_TRANS_OFF,inner,window,enabled,stride,2,2,0))return -4;
    return 0;
}
static void outer_free(OuterInst*v){free(v->outer);memset(v,0,sizeof(*v));}
static int direct_new(Sp11PeImage*img,DirectInst*v){
    memset(v,0,sizeof(*v));v->inner=aligned_alloc(64,0x200);if(!v->inner)return -1;memset(v->inner,0,0x200);((CtorFn)sp11_pe_ptr_for_va(img,VR_CTOR_VA))(v->inner);if(init_inner(img,v->inner,&v->arena,v->cfg))return -2;return 0;
}
static void direct_free(DirectInst*v){free(v->arena);free(v->inner);memset(v,0,sizeof(*v));}
static void fill_input(float*x,size_t n){uint32_t z=0x12345678u;for(size_t i=0;i<n;i++){z=z*1664525u+1013904223u;float noise=((int32_t)(z>>8))/8388608.0f;double t=i/48000.0;x[2*i]=(float)(.07*sin(2*M_PI*997*t)+.015*noise);x[2*i+1]=(float)(.055*sin(2*M_PI*1553*t+.31)-.012*noise);}}
static int run_outer(Sp11PeImage*img,const float*in,float*out,size_t n,const unsigned*pat,size_t np,const char*name,const float*ref){
    OuterInst v;if(outer_new(img,&v)){fprintf(stderr,"outer_new failed %s\n",name);return 1;}OuterHotFn hot=(OuterHotFn)sp11_pe_ptr_for_va(img,VR_OUTER_HOT_VA);
    uint64_t a0=g_heap_alloc_calls,f0=g_heap_free_calls,r0=g_heap_realloc_calls;size_t off=0,k=0;uint32_t bad_meta=0;
    while(off<n){unsigned c=pat[k++%np];if(c>n-off)c=(unsigned)(n-off);Conn ci={(float*)in+2*off,c,1},co={out+2*off,0,0};Conn*pi=&ci,*po=&co;hot(v.outer+VR_RT_OFF,1,&pi,1,&po,NULL);if(co.frames!=c||co.flags!=1)bad_meta++;off+=c;}
    size_t first=(size_t)-1;float md=0;if(ref){for(size_t i=0;i<2*n;i++){if(memcmp(&out[i],&ref[i],4)){if(first==(size_t)-1)first=i;float x=fabsf(out[i]-ref[i]);if(x>md)md=x;}}}
    printf("%-10s calls=%zu state=%u meta_bad=%u heap_delta=%"PRIu64"/%"PRIu64"/%"PRIu64, name,k,d(v.outer+VR_TRANS_OFF,0),bad_meta,g_heap_alloc_calls-a0,g_heap_free_calls-f0,g_heap_realloc_calls-r0);
    if(ref) printf(" equal=%s first_diff=%s max_abs=%g",first==(size_t)-1?"YES":"NO",first==(size_t)-1?"-":"set",md);
    puts(""); outer_free(&v); return bad_meta||first!=(size_t)-1;
}
int main(int ac,char**av){
    const char*dll=ac>1?av[1]:"DolbyApoVr.dll";hs();Sp11PeImage img;if(init_dll(&img,dll))return 2;
    const size_t N=1000000;float*in=malloc(2*N*sizeof(float)),*ref=calloc(2*N,sizeof(float)),*out=calloc(2*N,sizeof(float));if(!in||!ref||!out)return 3;fill_input(in,N);
    DirectInst di;if(direct_new(&img,&di))return 4;uint64_t a0=g_heap_alloc_calls,f0=g_heap_free_calls,r0=g_heap_realloc_calls;((ProcessFn)sp11_pe_ptr_for_va(&img,VR_PROCESS_VA))(di.inner,ref,in,(uint32_t)N);printf("direct_ref heap_delta=%"PRIu64"/%"PRIu64"/%"PRIu64"\n",g_heap_alloc_calls-a0,g_heap_free_calls-f0,g_heap_realloc_calls-r0);direct_free(&di);
    struct T{const char*n;unsigned p[12];size_t np;}tests[]={{"one",{1},1},{"64",{64},1},{"480",{480},1},{"1024",{1024},1},{"odd",{127,353},2},{"mixed",{1,17,64,127,480,1024,3,511,29,256,777,5},12},{"all",{1000000},1}};
    int bad=0;for(size_t t=0;t<sizeof(tests)/sizeof(tests[0]);t++){memset(out,0,2*N*sizeof(float));bad|=run_outer(&img,in,out,N,tests[t].p,tests[t].np,tests[t].n,ref);}printf("RESULT %s\n",bad?"FAIL":"PASS");return bad?5:0;
}
