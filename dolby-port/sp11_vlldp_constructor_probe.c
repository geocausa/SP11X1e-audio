#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>

#define DLL_RUNTIME_BASE 0x00007ff9fe4a0000ULL
#define STATE_PAGE_BASE  0x0000020bdc68c000ULL
#define STATE_BASE       0x0000020bdc68c360ULL
#define STATE_MAP_SIZE   0x20000ULL
#define CTOR_VA          0x18001BFB0ULL
#define CRIT_INIT_VA     0x1800226F0ULL

typedef void *(*CtorFn)(uint32_t, uint32_t, uint32_t, uint32_t, void *);

static void crash_handler(int sig, siginfo_t *si, void *ctxv){
    ucontext_t *uc=(ucontext_t*)ctxv;
#if defined(__aarch64__)
    fprintf(stderr,"CRASH sig=%d addr=%p pc=%p lr=%p x0=%#llx x1=%#llx x8=%#llx\n",sig,si->si_addr,(void*)uc->uc_mcontext.pc,(void*)uc->uc_mcontext.regs[30],(unsigned long long)uc->uc_mcontext.regs[0],(unsigned long long)uc->uc_mcontext.regs[1],(unsigned long long)uc->uc_mcontext.regs[8]);
#else
    fprintf(stderr,"CRASH sig=%d addr=%p\n",sig,si->si_addr);
#endif
    _Exit(128+sig);
}
static void install_handlers(void){
    struct sigaction sa; memset(&sa,0,sizeof(sa)); sa.sa_sigaction=crash_handler; sa.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&sa,NULL); sigaction(SIGBUS,&sa,NULL); sigaction(SIGILL,&sa,NULL); sigaction(SIGFPE,&sa,NULL);
}
static uint64_t q64(uint8_t *s,size_t off){ uint64_t v; memcpy(&v,s+off,8); return v; }
static uint32_t q32(uint8_t *s,size_t off){ uint32_t v; memcpy(&v,s+off,4); return v; }
static void patch_ret(const Sp11PeImage *img,uint64_t va){
    uint32_t *p=(uint32_t*)sp11_pe_ptr_for_va(img,va); *p=0xd65f03c0u;
    __builtin___clear_cache((char*)p,(char*)p+4);
}

static void win_lock_noop(void *p){ (void)p; }
static int win_lock_true(void *p){ (void)p; return 1; }
static int win_init_ex_true(void *p,unsigned spin,unsigned flags){ (void)p;(void)spin;(void)flags;return 1; }
static void patch_iat_ptr(const Sp11PeImage *img,uint64_t va,void *fn){
    uintptr_t *p=(uintptr_t*)sp11_pe_ptr_for_va(img,va); *p=(uintptr_t)fn;
}
static void patch_lock_imports(const Sp11PeImage *img){
    patch_iat_ptr(img,0x1801070E0ULL,(void*)win_lock_noop); /* LeaveCriticalSection */
    patch_iat_ptr(img,0x1801070E8ULL,(void*)win_lock_noop); /* EnterCriticalSection */
    patch_iat_ptr(img,0x180107190ULL,(void*)win_lock_noop); /* InitializeCriticalSection */
    patch_iat_ptr(img,0x180107198ULL,(void*)win_lock_true); /* TryEnterCriticalSection */
    patch_iat_ptr(img,0x180107248ULL,(void*)win_lock_noop); /* DeleteCriticalSection */
    patch_iat_ptr(img,0x180107250ULL,(void*)win_init_ex_true); /* InitializeCriticalSectionEx */
}
int main(int argc,char **argv){
    const char *dll=argc>1?argv[1]:"dll/DolbyAPOvlldp150.dll";
    install_handlers();
    Sp11PeImage img;
    int rc=sp11_pe_load_at(&img,dll,DLL_RUNTIME_BASE);
    if(rc){fprintf(stderr,"fixed PE load failed rc=%d\n",rc);return 2;}
    fprintf(stderr,"mapped DLL at %p size=%#zx delta=%#llx\n",img.base,img.size,(unsigned long long)img.delta);
    patch_ret(&img,CRIT_INIT_VA); /* legacy helper wrapper */
    patch_lock_imports(&img);      /* single-thread replay lock shims */
    void *mp=mmap((void*)STATE_PAGE_BASE,STATE_MAP_SIZE,PROT_READ|PROT_WRITE,
                  MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    if(mp==MAP_FAILED){perror("state mmap");return 3;}
    uint8_t *state=(uint8_t*)(uintptr_t)STATE_BASE;
    CtorFn ctor=(CtorFn)sp11_pe_ptr_for_va(&img,CTOR_VA);
    void *ret=ctor(256,48000,2,0,state);
    printf("ctor_ret=%p expected_state=%p match=%s\n",ret,state,ret==state?"YES":"NO");
    printf("header block=%u rate=%u channels=%u slots=%u gate=%u\n",
           q32(state,8),q32(state,12),q32(state,16),q32(state,0xc64),q32(state,0xc6c));
    const size_t offs[]={0x0,0x48,0x50,0x88,0x650,0xc90,0xc98,0xca0,0xca8,0xde8,0x10e0};
    for(size_t i=0;i<sizeof(offs)/sizeof(offs[0]);i++)
        printf("ptr +%04zx = %016llx (rel=%+lld)\n",offs[i],(unsigned long long)q64(state,offs[i]),
               (long long)(q64(state,offs[i])-STATE_BASE));
    printf("ana0 internals: %016llx %016llx %016llx %016llx %016llx\n",
      (unsigned long long)q64(state+0x1620,0), (unsigned long long)q64(state+0x1620,8),
      (unsigned long long)q64(state+0x1620,0x18),(unsigned long long)q64(state+0x1620,0x20),
      (unsigned long long)q64(state+0x1620,0x28));
    printf("ana1 internals: %016llx %016llx %016llx %016llx %016llx\n",
      (unsigned long long)q64(state+0x32a0,0), (unsigned long long)q64(state+0x32a0,8),
      (unsigned long long)q64(state+0x32a0,0x18),(unsigned long long)q64(state+0x32a0,0x20),
      (unsigned long long)q64(state+0x32a0,0x28));
    return 0;
}
