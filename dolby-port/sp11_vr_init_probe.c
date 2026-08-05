#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ucontext.h>

#define VR_VTABLE_VA 0x1801D8AE0ULL
#define VR_INIT_VA    0x1800DBE30ULL
#define VR_CTOR_VA    0x1800DB270ULL
#define VR_RATE_MAP_INIT_VA 0x180005B40ULL
#define VR_PROCESS_VA 0x1800F65E0ULL
#define CRT_ATEXIT_REGISTER_VA 0x180133130ULL
#define WIN_CHKSTK_VA 0x180001010ULL
#define VR_LOGGER_VA  0x1800E0CA8ULL
#define VR_SCOPE_IN_VA 0x1800E0290ULL
#define VR_SCOPE_OUT_VA 0x1800E0558ULL
#define VR_RESOURCE_VA 0x1802D3200ULL
#define VR_RESOURCE_SIZE 17260u

typedef uint64_t (*InitFn)(void*,void*);
typedef void *(*CtorFn)(void*);
typedef void (*VoidFn)(void);
typedef void (*ProcessFn)(void*,float*,const float*,uint32_t);
static void *g_resource_data;
static uint8_t g_empty_prop_node[0xa0] __attribute__((aligned(16)));
static uint64_t g_empty_prop_map[2] __attribute__((aligned(16)));
static uint64_t g_heap_alloc_calls, g_heap_free_calls, g_heap_realloc_calls;

static void crash(int sig,siginfo_t*si,void*cv){ucontext_t*uc=cv;
#if defined(__aarch64__)
fprintf(stderr,"CRASH sig=%d addr=%p pc=%p lr=%p x0=%#llx x1=%#llx x2=%#llx x3=%#llx x8=%#llx x15=%#llx x18=%#llx x19=%#llx\n",sig,si->si_addr,(void*)uc->uc_mcontext.pc,(void*)uc->uc_mcontext.regs[30],(unsigned long long)uc->uc_mcontext.regs[0],(unsigned long long)uc->uc_mcontext.regs[1],(unsigned long long)uc->uc_mcontext.regs[2],(unsigned long long)uc->uc_mcontext.regs[3],(unsigned long long)uc->uc_mcontext.regs[8],(unsigned long long)uc->uc_mcontext.regs[15],(unsigned long long)uc->uc_mcontext.regs[18],(unsigned long long)uc->uc_mcontext.regs[19]);
#endif
_Exit(128+sig);}
static void hs(void){struct sigaction sa={0};sa.sa_sigaction=crash;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,0);sigaction(SIGBUS,&sa,0);sigaction(SIGILL,&sa,0);sigaction(SIGFPE,&sa,0);}
static uint64_t q(void*p,size_t o){uint64_t v;memcpy(&v,(char*)p+o,8);return v;}
static uint32_t d(void*p,size_t o){uint32_t v;memcpy(&v,(char*)p+o,4);return v;}
static void wq(void*p,size_t o,uint64_t v){memcpy((char*)p+o,&v,8);}
static void wd(void*p,size_t o,uint32_t v){memcpy((char*)p+o,&v,4);}

static void patch_iat(const Sp11PeImage*i,uint64_t va,void*f){*(uintptr_t*)sp11_pe_ptr_for_va(i,va)=(uintptr_t)f;}
static void patch_ret(const Sp11PeImage*i,uint64_t va){uint32_t*p=sp11_pe_ptr_for_va(i,va);p[0]=0xd65f03c0u;__builtin___clear_cache((char*)p,(char*)p+4);}
static void patch_ret0(const Sp11PeImage*i,uint64_t va){uint32_t*p=sp11_pe_ptr_for_va(i,va);p[0]=0xaa1f03e0u; p[1]=0xd65f03c0u;__builtin___clear_cache((char*)p,(char*)p+8);}

static void lock_noop(void *p){(void)p;}
static int srw_try_true(void *p){(void)p;return 1;}
static void srw_noop(void *p){(void)p;}
static void cv_wake_noop(void *p){(void)p;}
static int cv_sleep_true(void *cv,void *lock,uint32_t ms,uint32_t flags){(void)cv;(void)lock;(void)ms;(void)flags;return 1;}
static int lock_true(void *p){(void)p;return 1;}
static int initex_true(void *p,unsigned spin,unsigned flags){(void)p;(void)spin;(void)flags;return 1;}

static void *shim_GetProcessHeap(void){return (void*)1;}
static void *shim_HeapAlloc(void *heap,uint32_t flags,size_t n){(void)heap;g_heap_alloc_calls++;void *p=malloc(n?n:1);if(p && (flags&8))memset(p,0,n);return p;}
static int shim_HeapFree(void *heap,uint32_t flags,void *p){(void)heap;(void)flags;g_heap_free_calls++;free(p);return 1;}
static void *shim_HeapReAlloc(void *heap,uint32_t flags,void *p,size_t n){(void)heap;(void)flags;g_heap_realloc_calls++;return realloc(p,n?n:1);}
static size_t shim_HeapSize(void *heap,uint32_t flags,void *p){(void)heap;(void)flags;return p?malloc_usable_size(p):(size_t)-1;}
static int32_t shim_PropVariantClear(void *pv){if(pv)memset(pv,0,24);return 0;}
static void *shim_GetEmptyPropertyMap(void *unused){(void)unused;return g_empty_prop_map;}
static void init_empty_property_map(void){memset(g_empty_prop_node,0,sizeof(g_empty_prop_node));memset(g_empty_prop_map,0,sizeof(g_empty_prop_map));uintptr_t n=(uintptr_t)g_empty_prop_node;memcpy(g_empty_prop_node+0,&n,8);memcpy(g_empty_prop_node+8,&n,8);memcpy(g_empty_prop_node+16,&n,8);g_empty_prop_node[0x18]=1;g_empty_prop_node[0x19]=1;g_empty_prop_map[0]=n;g_empty_prop_map[1]=0;}

static int shim_GetModuleHandleExW(uint32_t flags,const uint16_t *name,void **out){(void)flags;(void)name;if(out)*out=(void*)1;return 1;}
static void *shim_FindResourceW(void *module,const uint16_t *name,const uint16_t *type){(void)module;(void)name;(void)type;return (void*)2;}
static void *shim_LoadResource(void *module,void *res){(void)module;(void)res;return (void*)3;}
static uint32_t shim_SizeofResource(void *module,void *res){(void)module;(void)res;return VR_RESOURCE_SIZE;}
static void *shim_LockResource(void *h){(void)h;return g_resource_data;}

static void patch_runtime(Sp11PeImage*i){
    /* single-threaded offline probe: Windows lock calls are semantic no-ops */
    patch_iat(i,0x1801D31A0ULL,(void*)lock_true);   /* TryEnterCriticalSection */
    patch_iat(i,0x1801D31A8ULL,(void*)lock_noop);   /* InitializeCriticalSection */
    patch_iat(i,0x1801D32A0ULL,(void*)lock_noop);   /* LeaveCriticalSection */
    patch_iat(i,0x1801D32A8ULL,(void*)lock_noop);   /* EnterCriticalSection */
    patch_iat(i,0x1801D32C8ULL,(void*)lock_noop);   /* DeleteCriticalSection */
    patch_iat(i,0x1801D32D0ULL,(void*)initex_true); /* InitializeCriticalSectionEx */
    patch_iat(i,0x1801D3340ULL,(void*)srw_noop); /* InitializeSRWLock */

    patch_iat(i,0x1801D3180ULL,(void*)shim_GetProcessHeap);
    patch_iat(i,0x1801D3198ULL,(void*)shim_HeapFree);
    patch_iat(i,0x1801D3310ULL,(void*)shim_HeapAlloc);
    patch_iat(i,0x1801D3490ULL,(void*)shim_HeapSize);
    patch_iat(i,0x1801D3498ULL,(void*)shim_HeapReAlloc);
    patch_iat(i,0x1801D35F0ULL,(void*)shim_PropVariantClear);

    /* Resource 255/254 is the embedded default headphone tuning blob. */
    patch_iat(i,0x1801D3148ULL,(void*)shim_GetModuleHandleExW);
    patch_iat(i,0x1801D30C0ULL,(void*)shim_FindResourceW);
    patch_iat(i,0x1801D30C8ULL,(void*)shim_SizeofResource);
    patch_iat(i,0x1801D30D0ULL,(void*)shim_LoadResource);
    patch_iat(i,0x1801D3150ULL,(void*)shim_LockResource);

    /* Dolby logging/ETW singleton is Windows TLS-heavy and not part of DSP semantics. */
    patch_ret0(i,VR_LOGGER_VA);
    patch_ret(i,VR_SCOPE_IN_VA);
    patch_ret(i,VR_SCOPE_OUT_VA);
    patch_ret0(i,CRT_ATEXIT_REGISTER_VA); /* offline probe: no DLL-global destructor registration */
    patch_ret(i,WIN_CHKSTK_VA); /* Windows TEB stack probe; Linux kernel grows stack pages */
}

int main(int ac,char**av){
    const char*dll=ac>1?av[1]:"DolbyAPOVR.dll";
    hs();
    Sp11PeImage img;
    if(sp11_pe_load(&img,dll)){fprintf(stderr,"load fail\n");return 2;}
    g_resource_data=sp11_pe_ptr_for_va(&img,VR_RESOURCE_VA);
    patch_runtime(&img);

    ((VoidFn)sp11_pe_ptr_for_va(&img,VR_RATE_MAP_INIT_VA))();

    uint8_t*o=aligned_alloc(64,0x200); memset(o,0,0x200);
    ((CtorFn)sp11_pe_ptr_for_va(&img,VR_CTOR_VA))(o);
    if(q(o,0)!=(uintptr_t)sp11_pe_ptr_for_va(&img,VR_VTABLE_VA)){fprintf(stderr,"ctor vtable mismatch got=%p expected=%p\n",(void*)(uintptr_t)q(o,0),sp11_pe_ptr_for_va(&img,VR_VTABLE_VA));return 5;}
    init_empty_property_map();
    *(uintptr_t*)sp11_pe_ptr_for_va(&img,VR_VTABLE_VA+0x18)=(uintptr_t)shim_GetEmptyPropertyMap;

    const size_t arena_sz=128u*1024u*1024u;
    uint8_t *arena=NULL;
    if(posix_memalign((void**)&arena,64,arena_sz)){perror("arena");return 3;}
    memset(arena,0,arena_sz);

    uint8_t cfg[0x60] __attribute__((aligned(16))); memset(cfg,0,sizeof(cfg));
    wd(cfg,0x00,48000); /* sample rate */
    wd(cfg,0x04,2);     /* input channels */
    wd(cfg,0x08,2);     /* output channels */
    wd(cfg,0x0c,2);     /* intermediate channels */
    wq(cfg,0x10,0);     /* optional property store */
    wq(cfg,0x20,arena_sz);              /* bump allocator size */
    wq(cfg,0x28,(uintptr_t)arena);       /* bump allocator base */
    wq(cfg,0x30,(uintptr_t)arena);       /* bump allocator current */

    fprintf(stderr,"pre obj=%p vt=%p cfg=%p arena=%p..%p resource=%p/%u\n",o,(void*)(uintptr_t)q(o,0),cfg,arena,arena+arena_sz,g_resource_data,VR_RESOURCE_SIZE);
    uint64_t rc=((InitFn)sp11_pe_ptr_for_va(&img,VR_INIT_VA))(o,cfg);
    fprintf(stderr,"init returned rc=%llu allocator_used=%llu\n",(unsigned long long)rc,(unsigned long long)(q(cfg,0x30)-(uintptr_t)arena));
    printf("RETURNED vt=%p cfgptr=%p in=%p out=%p fill=%u b38=%u b3c=%u core130=%p tune120=%p tune128=%p phrtf138=%p flags_f8=%u fc=%u\n",(void*)(uintptr_t)q(o,0),(void*)(uintptr_t)q(o,8),(void*)(uintptr_t)q(o,0x10),(void*)(uintptr_t)q(o,0x18),d(o,0x20),d(o,0x38),d(o,0x3c),(void*)(uintptr_t)q(o,0x130),(void*)(uintptr_t)q(o,0x120),(void*)(uintptr_t)q(o,0x128),(void*)(uintptr_t)q(o,0x138),d(o,0xf8),d(o,0xfc));
    if(!rc)return 4;

    enum { N=8192 };
    float *in=calloc((size_t)N*2,sizeof(float)), *out=calloc((size_t)N*2,sizeof(float));
    if(!in||!out)return 6;
    for(unsigned j=0;j<N;j++){double t=(double)j/48000.0;in[2*j]=(float)(0.10*sin(2.0*M_PI*997.0*t));in[2*j+1]=(float)(0.07*sin(2.0*M_PI*1553.0*t+0.31));}
    ((ProcessFn)sp11_pe_ptr_for_va(&img,VR_PROCESS_VA))(o,out,in,N);
    double si=0,so=0,sd=0;float peak=0;unsigned bad=0,first=N;
    for(unsigned j=0;j<N*2;j++){float v=out[j];if(!isfinite(v)){bad++;continue;}si+=(double)in[j]*in[j];so+=(double)v*v;double e=(double)v-in[j];sd+=e*e;if(fabsf(v)>peak)peak=fabsf(v);if(v!=0.0f && first==N)first=j/2;}
    printf("AUDIO frames=%u bad=%u first_nonzero=%u in_rms=%.9g out_rms=%.9g diff_rms=%.9g peak=%.9g fill_after=%u\n",N,bad,first,sqrt(si/(N*2)),sqrt(so/(N*2)),sqrt(sd/(N*2)),peak,d(o,0x20));
    for(unsigned j=0;j<12;j++)printf("S%02u in=(%+.7f,%+.7f) out=(%+.7f,%+.7f)\n",j,in[2*j],in[2*j+1],out[2*j],out[2*j+1]);
    return bad?7:0;
}
