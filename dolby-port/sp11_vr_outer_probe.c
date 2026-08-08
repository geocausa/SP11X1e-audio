#define main sp11_vr_probe_embedded_main
#include "sp11_vr_init_probe.c"
#undef main
#include <inttypes.h>

#define VR_OUTER_CTOR_VA 0x1800D3B18ULL
#define VR_OUTER_HOT_VA  0x1801D10C8ULL
#define VR_TRANS_INIT_VA 0x1801B96B8ULL
#define VR_OUTER_SIZE     0x3C0430u
#define VR_INNER_OFF      0x12C2F0u
#define VR_INNER_PTR_OFF  0x12C150u
#define VR_RT_OFF         0x8u
#define VR_TRANS_OFF      0x80u
#define VR_ARENA_OFF      0x12C430u
#define VR_ARENA_SIZE     0x294000u

#define VR_OUTER_VT0_VA   0x1801D5980ULL
#define VR_OUTER_VT1_VA   0x1801D5A18ULL
#define VR_OUTER_VT2_VA   0x1801D5A50ULL
#define VR_OUTER_VT3_VA   0x1801D5A80ULL
#define VR_OUTER_VT4_VA   0x1801D5AB8ULL
#define VR_OUTER_VT5_VA   0x1801D5B00ULL
#define VR_OUTER_VT6_VA   0x1801D5B30ULL
#define VR_OUTER_VT7_VA   0x1801D5B58ULL

typedef void *(*OuterCtorFn)(void *);
typedef uint64_t (*TransInitFn)(void*,void*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*OuterHotFn)(void*,uint32_t,void*,uint32_t,void*,void*);
typedef uint32_t (*GetU32Fn)(void*);
typedef uint8_t (*GetU8Fn)(void*);

typedef struct { float *pBuffer; uint32_t frames; uint32_t flags; } Conn;

static int init_dll(Sp11PeImage *img,const char*dll){
    if(sp11_pe_load(img,dll)) return -1;
    g_resource_data=sp11_pe_ptr_for_va(img,VR_RESOURCE_VA);
    patch_runtime(img);
    ((VoidFn)sp11_pe_ptr_for_va(img,VR_RATE_MAP_INIT_VA))();
    init_empty_property_map();
    *(uintptr_t*)sp11_pe_ptr_for_va(img,VR_VTABLE_VA+0x18)=(uintptr_t)shim_GetEmptyPropertyMap;
    return 0;
}

static void finish_outer_factory_vtables(Sp11PeImage *img,uint8_t *outer){
    static const uint64_t vas[8]={
        VR_OUTER_VT0_VA,VR_OUTER_VT1_VA,VR_OUTER_VT2_VA,VR_OUTER_VT3_VA,
        VR_OUTER_VT4_VA,VR_OUTER_VT5_VA,VR_OUTER_VT6_VA,VR_OUTER_VT7_VA
    };
    for(unsigned i=0;i<8;i++) wq(outer,i*8,(uintptr_t)sp11_pe_ptr_for_va(img,vas[i]));
}

static int init_inner_arena(Sp11PeImage *img,uint8_t *inner,uint8_t *arena,size_t arena_sz,uint8_t cfg[0x60]){
    memset(arena,0,arena_sz); memset(cfg,0,0x60);
    wd(cfg,0,48000); wd(cfg,4,2); wd(cfg,8,2); wd(cfg,0xc,2);
    wq(cfg,0x20,arena_sz); wq(cfg,0x28,(uintptr_t)arena); wq(cfg,0x30,(uintptr_t)arena);
    uint64_t rc=((InitFn)sp11_pe_ptr_for_va(img,VR_INIT_VA))(inner,cfg);
    if(!rc) return -2;
    uintptr_t cur=(uintptr_t)q(cfg,0x30),beg=(uintptr_t)arena,end=beg+arena_sz;
    if(cur<beg || cur>end){fprintf(stderr,"VR arena cursor escaped: %p not in [%p,%p]\n",(void*)cur,arena,(void*)end);return -3;}
    return 0;
}

/* Standalone LibWrapperVr tests keep an owned arena. */
static int init_inner(Sp11PeImage *img,uint8_t *inner,uint8_t **arena_out,uint8_t cfg[0x60]){
    const size_t arena_sz=8u*1024u*1024u;
    uint8_t *arena=NULL;
    if(posix_memalign((void**)&arena,64,arena_sz)) return -1;
    if(init_inner_arena(img,inner,arena,arena_sz,cfg)){free(arena);return -2;}
    *arena_out=arena; return 0;
}

/* Exact factory geometry: LibWrapperVr consumes the 0x294000-byte arena
 * embedded in the same 0x3c0430-byte APO allocation. */
static int init_outer_inner(Sp11PeImage *img,uint8_t *outer,uint8_t cfg[0x60]){
    uint8_t *inner=(uint8_t*)(uintptr_t)q(outer,VR_INNER_PTR_OFF);
    if(inner!=outer+VR_INNER_OFF) return -1;
    return init_inner_arena(img,inner,outer+VR_ARENA_OFF,VR_ARENA_SIZE,cfg);
}

static void dump_inner_tuple(Sp11PeImage*img,void*inner,uint32_t *window,uint32_t *stride,uint32_t *enabled){
    uint32_t a=((GetU32Fn)sp11_pe_ptr_for_va(img,0x1800F6410ULL))(inner);
    uint32_t b=((GetU32Fn)sp11_pe_ptr_for_va(img,0x1800F6430ULL))(inner);
    uint32_t c=((GetU32Fn)sp11_pe_ptr_for_va(img,0x1800DB7B0ULL))(inner);
    uint32_t e=((GetU8Fn)sp11_pe_ptr_for_va(img,0x1800DC630ULL))(inner);
    *window=a+b; *stride=c; *enabled=e;
#ifndef SP11_VR_OUTER_NO_MAIN
    printf("INNER tuple a=%u b=%u window=%u stride=%u enabled=%u\n",a,b,*window,*stride,*enabled);
#endif
}

#ifndef SP11_VR_OUTER_NO_MAIN
int main(int ac,char**av){
    const char*dll=ac>1?av[1]:"DolbyApoVr.dll"; hs();
    Sp11PeImage img; if(init_dll(&img,dll)){fprintf(stderr,"load failed\n");return 2;} fprintf(stderr,"PE mapped base=%p size=%#zx preferred=%#llx\n",img.base,img.size,(unsigned long long)img.image_base);
    uint8_t *outer=NULL; if(posix_memalign((void**)&outer,64,VR_OUTER_SIZE)){return 3;} memset(outer,0,VR_OUTER_SIZE);
    fprintf(stderr,"calling real outer ctor base=%p size=%#x\n",outer,VR_OUTER_SIZE);
    ((OuterCtorFn)sp11_pe_ptr_for_va(&img,VR_OUTER_CTOR_VA))(outer);
    finish_outer_factory_vtables(&img,outer);
    uint8_t *inner=(uint8_t*)(uintptr_t)q(outer,VR_INNER_PTR_OFF);
    printf("CTOR base=%p rt=%p inner_ptr=%p expected=%p gate=%u vtbl0=%p vtblRT=%p arena=%p+%#x\n",outer,outer+VR_RT_OFF,inner,outer+VR_INNER_OFF,outer[0x78],(void*)(uintptr_t)q(outer,0),(void*)(uintptr_t)q(outer,8),outer+VR_ARENA_OFF,VR_ARENA_SIZE);
    if(inner!=outer+VR_INNER_OFF){fprintf(stderr,"inner pointer mismatch\n");return 4;}
    uint8_t cfg[0x60] __attribute__((aligned(16)));
    if(init_outer_inner(&img,outer,cfg)){fprintf(stderr,"inner init failed\n");return 5;}
    printf("ARENA used=%#llx of %#x\n",(unsigned long long)(q(cfg,0x30)-(uintptr_t)(outer+VR_ARENA_OFF)),VR_ARENA_SIZE);
    uint32_t window,stride,enabled; dump_inner_tuple(&img,inner,&window,&stride,&enabled);
    uint64_t tr=((TransInitFn)sp11_pe_ptr_for_va(&img,VR_TRANS_INIT_VA))(outer+VR_TRANS_OFF,inner,window,enabled,stride,2,2,0);
    printf("TRANS rc=%"PRIu64" state=%u inner_at_state=%p buf=%p dims=%u/%u flag=%u\n",tr,d(outer+VR_TRANS_OFF,0),(void*)(uintptr_t)q(outer+VR_TRANS_OFF,0x12c040),(void*)(uintptr_t)q(outer+VR_TRANS_OFF,0x12c010),d(outer+VR_TRANS_OFF,0x12c030),d(outer+VR_TRANS_OFF,0x12c034),d(outer+VR_TRANS_OFF,0x12c038));
    if(!tr)return 6;

    enum {N=8192}; float *in=calloc((size_t)N*2,sizeof(float)),*out=calloc((size_t)N*2,sizeof(float)); if(!in||!out)return 7;
    for(unsigned j=0;j<N;j++){double t=(double)j/48000.0;in[2*j]=(float)(.10*sin(2*M_PI*997*t));in[2*j+1]=(float)(.07*sin(2*M_PI*1553*t+.31));}
    Conn ci={in,N,1}, co={out,0,0}; Conn *pi=&ci,*po=&co;
    uint64_t a0=g_heap_alloc_calls,f0=g_heap_free_calls,r0=g_heap_realloc_calls;
    ((OuterHotFn)sp11_pe_ptr_for_va(&img,VR_OUTER_HOT_VA))(outer+VR_RT_OFF,1,&pi,1,&po,NULL);
    double si=0,so=0,sd=0;float peak=0;unsigned bad=0,first=N;for(unsigned j=0;j<N*2;j++){float v=out[j];if(!isfinite(v)){bad++;continue;}si+=(double)in[j]*in[j];so+=(double)v*v;double e=(double)v-in[j];sd+=e*e;if(fabsf(v)>peak)peak=fabsf(v);if(v!=0.0f&&first==N)first=j/2;}
    printf("OUTER AUDIO frames=%u out_frames=%u flags=%u bad=%u first_nonzero=%u in_rms=%.9g out_rms=%.9g diff_rms=%.9g peak=%.9g state=%u heap_delta=%"PRIu64"/%"PRIu64"/%"PRIu64"\n",N,co.frames,co.flags,bad,first,sqrt(si/(N*2)),sqrt(so/(N*2)),sqrt(sd/(N*2)),peak,d(outer+VR_TRANS_OFF,0),g_heap_alloc_calls-a0,g_heap_free_calls-f0,g_heap_realloc_calls-r0);
    for(unsigned j=0;j<12;j++)printf("S%02u in=(%+.7f,%+.7f) out=(%+.7f,%+.7f)\n",j,in[2*j],in[2*j+1],out[2*j],out[2*j+1]);
    return bad?8:0;
}
#endif
