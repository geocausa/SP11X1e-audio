#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <math.h>
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
#define AUX_OFFSET       0x7848ULL
#define BUF_A_OFFSET     0x9904ULL
#define BUF_B_OFFSET     0xA104ULL
#define CTOR_VA          0x18001BFB0ULL
#define ORCH_VA          0x18001F7A8ULL

typedef void *(*CtorFn)(uint32_t,uint32_t,uint32_t,uint32_t,void*);
typedef void (*OrchFn)(void*,void*,void*,void*);
typedef struct { uint64_t channels, stride, format, planes; } AudioDesc;

static void crash_handler(int sig,siginfo_t *si,void *ctxv){
    ucontext_t *uc=(ucontext_t*)ctxv;
#if defined(__aarch64__)
    fprintf(stderr,"CRASH sig=%d addr=%p pc=%p lr=%p x0=%#llx x1=%#llx x2=%#llx x3=%#llx x8=%#llx\n",
      sig,si->si_addr,(void*)uc->uc_mcontext.pc,(void*)uc->uc_mcontext.regs[30],
      (unsigned long long)uc->uc_mcontext.regs[0],(unsigned long long)uc->uc_mcontext.regs[1],
      (unsigned long long)uc->uc_mcontext.regs[2],(unsigned long long)uc->uc_mcontext.regs[3],
      (unsigned long long)uc->uc_mcontext.regs[8]);
#else
    fprintf(stderr,"CRASH sig=%d addr=%p\n",sig,si->si_addr);
#endif
    _Exit(128+sig);
}
static void install_handlers(void){
    struct sigaction sa; memset(&sa,0,sizeof(sa)); sa.sa_sigaction=crash_handler; sa.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&sa,NULL); sigaction(SIGBUS,&sa,NULL); sigaction(SIGILL,&sa,NULL); sigaction(SIGFPE,&sa,NULL);
}
static void win_lock_noop(void *p){(void)p;}
static int win_lock_true(void *p){(void)p;return 1;}
static int win_init_ex_true(void *p,unsigned s,unsigned f){(void)p;(void)s;(void)f;return 1;}
static void patch_iat(const Sp11PeImage *img,uint64_t va,void *fn){*(uintptr_t*)sp11_pe_ptr_for_va(img,va)=(uintptr_t)fn;}
static void patch_runtime(const Sp11PeImage *img){
    patch_iat(img,0x1801070E0ULL,(void*)win_lock_noop);
    patch_iat(img,0x1801070E8ULL,(void*)win_lock_noop);
    patch_iat(img,0x180107190ULL,(void*)win_lock_noop);
    patch_iat(img,0x180107198ULL,(void*)win_lock_true);
    patch_iat(img,0x180107248ULL,(void*)win_lock_noop);
    patch_iat(img,0x180107250ULL,(void*)win_init_ex_true);
}
static int read_file_exact(const char *path,void *dst,size_t n){
    FILE *f=fopen(path,"rb"); if(!f){perror(path);return -1;}
    size_t got=fread(dst,1,n,f); int extra=fgetc(f); fclose(f);
    if(got!=n||extra!=EOF){fprintf(stderr,"bad size %s got=%zu expected=%zu\n",path,got,n);return -1;} return 0;
}
static uint32_t rd32(const uint8_t *p,size_t o){uint32_t v;memcpy(&v,p+o,4);return v;}
static uint64_t rd64(const uint8_t *p,size_t o){uint64_t v;memcpy(&v,p+o,8);return v;}
static size_t diff_count(const uint8_t *a,const uint8_t *b,size_t n,size_t *first){
    size_t c=0; *first=(size_t)-1; for(size_t i=0;i<n;i++) if(a[i]!=b[i]){if(*first==(size_t)-1)*first=i;c++;} return c;
}
static double rms(const float *p,size_t n){double s=0;for(size_t i=0;i<n;i++)s+=(double)p[i]*p[i];return sqrt(s/n);}
static void fill_tone(float *p,size_t frames,float amp){for(size_t i=0;i<frames;i++){float v=amp*sinf((float)(2.0*M_PI*997.0*i/48000.0));p[2*i]=v;p[2*i+1]=v;}}

int main(int argc,char **argv){
    if(argc<5){fprintf(stderr,"usage: %s DLL entry_state entry_aux return_state [return_aux] [A|B|zero]\n",argv[0]);return 2;}
    const char *dll=argv[1],*entry_state=argv[2],*entry_aux=argv[3],*return_state=argv[4];
    const char *return_aux=argc>5?argv[5]:NULL; const char *mode=argc>6?argv[6]:"A";
    install_handlers();
    Sp11PeImage img; int rc=sp11_pe_load_at(&img,dll,DLL_RUNTIME_BASE);
    if(rc){fprintf(stderr,"fixed PE load failed rc=%d\n",rc);return 3;} patch_runtime(&img);
    void *mp=mmap((void*)STATE_PAGE_BASE,STATE_MAP_SIZE,PROT_READ|PROT_WRITE,
      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    if(mp==MAP_FAILED){perror("state mmap");return 4;}
    uint8_t *state=(uint8_t*)(uintptr_t)STATE_BASE; uint8_t *aux=state+AUX_OFFSET;
    CtorFn ctor=(CtorFn)sp11_pe_ptr_for_va(&img,CTOR_VA);
    if(ctor(256,48000,2,0,state)!=state){fprintf(stderr,"constructor returned unexpected pointer\n");return 5;}

    /* Validate deterministic constructor geometry against live-capture pointers before overlay. */
    const size_t ptr_offs[]={0x48,0x50,0x88,0x650,0xc90,0xc98,0xca0,0xca8,0xde8,0x10e0};
    uint8_t captured[0x4000]; if(read_file_exact(entry_state,captured,sizeof(captured)))return 6;
    int geom_ok=1; for(size_t i=0;i<sizeof(ptr_offs)/sizeof(ptr_offs[0]);i++)
      if(rd64(state,ptr_offs[i])!=rd64(captured,ptr_offs[i])) geom_ok=0;
    printf("constructor_geometry=%s\n",geom_ok?"MATCH":"MISMATCH");

    memcpy(state,captured,sizeof(captured));
    if(read_file_exact(entry_aux,aux,0x1000))return 7;
    uint8_t before_state[0x4000],before_aux[0x1000]; memcpy(before_state,state,sizeof(before_state));memcpy(before_aux,aux,sizeof(before_aux));

    float *a=(float*)(state+BUF_A_OFFSET), *b=(float*)(state+BUF_B_OFFSET); memset(a,0,0x800);memset(b,0,0x800);
    if(strcmp(mode,"A")==0)fill_tone(a,256,0.05f); else if(strcmp(mode,"B")==0)fill_tone(b,256,0.05f);
    uint64_t pa[2]={(uint64_t)(uintptr_t)a,(uint64_t)(uintptr_t)(a+1)};
    uint64_t pb[2]={(uint64_t)(uintptr_t)b,(uint64_t)(uintptr_t)(b+1)};
    AudioDesc da={2,2,7,(uint64_t)(uintptr_t)pa}, db={2,2,7,(uint64_t)(uintptr_t)pb};
    printf("pre selector=%u gate=%u A_rms=%.9f B_rms=%.9f mode=%s\n",rd32(state,0x1630),rd32(state,0xc6c),rms(a,512),rms(b,512),mode);
    fflush(stdout);
    OrchFn orch=(OrchFn)sp11_pe_ptr_for_va(&img,ORCH_VA); orch(state,&da,&db,aux);
    printf("post selector=%u A_rms=%.9f B_rms=%.9f\n",rd32(state,0x1630),rms(a,512),rms(b,512));
    size_t firsts,firsta; size_t ds=diff_count(before_state,state,0x4000,&firsts), daxc=diff_count(before_aux,aux,0x1000,&firsta);
    printf("mutation main_changed=%zu first=%#zx aux_changed=%zu first=%#zx\n",ds,firsts,daxc,firsta);

    uint8_t win_state[0x4000]; if(read_file_exact(return_state,win_state,sizeof(win_state)))return 8;
    size_t firstw; size_t dw=diff_count(win_state,state,0x4000,&firstw);
    printf("vs_windows_return main_diff=%zu/%zu first=%#zx exact=%s\n",dw,sizeof(win_state),firstw,dw?"NO":"YES");
    if(return_aux){uint8_t win_aux[0x1000]; if(read_file_exact(return_aux,win_aux,sizeof(win_aux)))return 9; size_t fw;size_t d=diff_count(win_aux,aux,0x1000,&fw);printf("vs_windows_return aux_diff=%zu/%zu first=%#zx exact=%s\n",d,sizeof(win_aux),fw,d?"NO":"YES");}
    return 0;
}
