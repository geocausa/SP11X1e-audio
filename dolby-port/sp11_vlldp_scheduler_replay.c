#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#define INNER_VTABLE_VA 0x18010B9A8ULL
#define CORE_CTOR_VA  0x18001BFB0ULL
#define PID5_VA       0x18001BC80ULL
#define PID17_VA      0x18001CDD0ULL
#define PID22_VA      0x18001E5C8ULL
#define PID31_VA      0x18001EE68ULL
#define APPLY_VA      0x18001D280ULL
#define SCHED_INIT_VA 0x1800ED2C0ULL
#define SCHED_RUN_VA  0x1800ED348ULL
#define CORE_ARENA_SIZE 0x20000U
#define SCHED_SIZE     0x12C200U
#define INNER_SIZE     0x200U
#define SP11_SIG       0x41435053U

typedef void *(*CoreCtorFn)(uint32_t,uint32_t,uint32_t,uint32_t,void*);
typedef void (*Pid5Fn)(void*,const uint32_t*);
typedef void (*Pid17Fn)(void*,uint32_t,const int32_t *const*);
typedef void (*Pid22Fn)(void*,uint32_t,const int32_t*);
typedef void (*Pid31Fn)(void*,const int32_t*);
typedef void (*ApplyFn)(void*,uint32_t);
typedef int (*SchedInitFn)(void*,void*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*SchedRunFn)(void*,uint32_t,void*,uint32_t,void*,void*);

typedef struct {
    float *buffer;
    uint32_t frames;
    uint32_t flag;
    uint32_t signature;
    uint32_t reserved;
} ConnProp;

static void crash_handler(int sig,siginfo_t *si,void *ctxv){
    ucontext_t *uc=(ucontext_t*)ctxv;
#if defined(__aarch64__)
    fprintf(stderr,"CRASH sig=%d addr=%p pc=%p lr=%p x0=%#llx x1=%#llx x2=%#llx x3=%#llx x4=%#llx x8=%#llx x19=%#llx x20=%#llx\n",
      sig,si->si_addr,(void*)uc->uc_mcontext.pc,(void*)uc->uc_mcontext.regs[30],
      (unsigned long long)uc->uc_mcontext.regs[0],(unsigned long long)uc->uc_mcontext.regs[1],
      (unsigned long long)uc->uc_mcontext.regs[2],(unsigned long long)uc->uc_mcontext.regs[3],
      (unsigned long long)uc->uc_mcontext.regs[4],(unsigned long long)uc->uc_mcontext.regs[8],
      (unsigned long long)uc->uc_mcontext.regs[19],(unsigned long long)uc->uc_mcontext.regs[20]);
#else
    fprintf(stderr,"CRASH sig=%d addr=%p\n",sig,si->si_addr);
#endif
    _Exit(128+sig);
}
static void handlers(void){struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_sigaction=crash_handler;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,0);sigaction(SIGBUS,&sa,0);sigaction(SIGILL,&sa,0);sigaction(SIGFPE,&sa,0);}
static void lock_noop(void*p){(void)p;} static int lock_true(void*p){(void)p;return 1;} static int initex_true(void*p,unsigned a,unsigned b){(void)p;(void)a;(void)b;return 1;}
static void piat(Sp11PeImage*i,uint64_t va,void*f){*(uintptr_t*)sp11_pe_ptr_for_va(i,va)=(uintptr_t)f;}
static void patch_runtime(Sp11PeImage*i){piat(i,0x1801070E0ULL,lock_noop);piat(i,0x1801070E8ULL,lock_noop);piat(i,0x180107190ULL,lock_noop);piat(i,0x180107198ULL,lock_true);piat(i,0x180107248ULL,lock_noop);piat(i,0x180107250ULL,initex_true);}
static void wr32(void*p,size_t o,uint32_t v){memcpy((uint8_t*)p+o,&v,4);} static void wr64(void*p,size_t o,uint64_t v){memcpy((uint8_t*)p+o,&v,8);} static uint32_t rd32(void*p,size_t o){uint32_t v;memcpy(&v,(uint8_t*)p+o,4);return v;} static uint64_t rd64(void*p,size_t o){uint64_t v;memcpy(&v,(uint8_t*)p+o,8);return v;}
static double rms(const float*p,size_t n){double s=0;for(size_t i=0;i<n;i++)s+=(double)p[i]*p[i];return sqrt(s/n);}

int main(int argc,char**argv){
 const char*dll=argc>1?argv[1]:"dll/DolbyAPOvlldp150.dll";int blocks=argc>2?atoi(argv[2]):32;handlers();
 Sp11PeImage img;if(sp11_pe_load(&img,dll)){fprintf(stderr,"pe load fail\n");return 2;}patch_runtime(&img);
 void *core_arena=0,*sched=0,*inner=0,*ibuf=0,*obuf=0;
 if(posix_memalign(&core_arena,64,CORE_ARENA_SIZE)||posix_memalign(&sched,64,SCHED_SIZE)||posix_memalign(&inner,64,INNER_SIZE)||posix_memalign(&ibuf,64,0x800)||posix_memalign(&obuf,64,0x800))return 3;
 memset(core_arena,0,CORE_ARENA_SIZE);memset(sched,0,SCHED_SIZE);memset(inner,0,INNER_SIZE);memset(ibuf,0,0x800);memset(obuf,0,0x800);
 wr64(inner,0x00,(uintptr_t)sp11_pe_ptr_for_va(&img,INNER_VTABLE_VA));
 uint32_t *fmt=calloc(8,sizeof(uint32_t));fmt[0]=48000;fmt[1]=2;fmt[2]=2;fmt[3]=2;
 wr64(inner,0x08,(uintptr_t)fmt);wr64(inner,0x10,(uintptr_t)ibuf);wr64(inner,0x18,(uintptr_t)obuf);wr32(inner,0x20,0);
 uint8_t *core=((CoreCtorFn)sp11_pe_ptr_for_va(&img,CORE_CTOR_VA))(256,48000,2,0,core_arena);if(!core){fprintf(stderr,"core ctor fail\n");return 4;}
 uint32_t empty[2]={0,0};int32_t g0[6]={20,0,32767,10,20,0};const int32_t*gp[1]={g0};int32_t stress[8]={216,216,0,0,0,0,0,0};int32_t bass[5]={0};
 ((Pid5Fn)sp11_pe_ptr_for_va(&img,PID5_VA))(core,empty);((Pid17Fn)sp11_pe_ptr_for_va(&img,PID17_VA))(core,1,gp);((Pid22Fn)sp11_pe_ptr_for_va(&img,PID22_VA))(core,8,stress);((Pid31Fn)sp11_pe_ptr_for_va(&img,PID31_VA))(core,bass);((ApplyFn)sp11_pe_ptr_for_va(&img,APPLY_VA))(core,2);
 uint64_t aux_owner=rd64(core,0xca0);void*aux=(void*)(uintptr_t)(aux_owner+8);
 wr64(inner,0x28,(uintptr_t)core);wr64(inner,0x30,(uintptr_t)aux);wr32(inner,0x38,176);wr32(inner,0x3c,256);wr32(inner,0x40,2);wr32(inner,0x50,0);wr32(inner,0x54,0);wr32(inner,0xa8,2);wr64(inner,0x158,(uintptr_t)core);wr32(inner,0x160,0);
 wr32(sched,0,3); /* live fresh active-graph scheduler mode */
 SchedInitFn sinit=(SchedInitFn)sp11_pe_ptr_for_va(&img,SCHED_INIT_VA);
 int ok=sinit(sched,inner,432,1,1024,2,2,0);
 printf("sched_init=%d mode=%u size=%u limit=%u phase=%u tail30=%u,%u,%u inner=%p tail48=%u\n",ok,rd32(sched,0),rd32(sched,4),rd32(sched,8),rd32(sched,12),rd32(sched,0x12c030),rd32(sched,0x12c034),rd32(sched,0x12c038),(void*)(uintptr_t)rd64(sched,0x12c040),rd32(sched,0x12c048));fflush(stdout);
 if(!ok)return 5;
 SchedRunFn run=(SchedRunFn)sp11_pe_ptr_for_va(&img,SCHED_RUN_VA);
 float *in=aligned_alloc(64,480*2*sizeof(float)),*out=aligned_alloc(64,480*2*sizeof(float));if(!in||!out)return 6;
 ConnProp ip={in,480,1,SP11_SIG,0},op={out,480,0,SP11_SIG,0};ConnProp *ipa=&ip,*opa=&op;
 for(int b=0;b<blocks;b++){
   for(int i=0;i<480;i++){double t=(b*480+i)/48000.0;in[2*i]=.03f*sinf(2*M_PI*997*t)+.01f*sinf(2*M_PI*113*t);in[2*i+1]=.025f*sinf(2*M_PI*701*t);out[2*i]=out[2*i+1]=0;}
   ip.frames=480;ip.flag=1;op.frames=480;op.flag=0;
   run(sched,1,&ipa,1,&opa,0);
   int bad=0;for(int i=0;i<960;i++)if(!isfinite(out[i]))bad++;
   printf("b=%02d out_flag=%u out_frames=%u rms=%.9f bad=%d phase=%u fill=%u\n",b,op.flag,op.frames,rms(out,960),bad,rd32(sched,12),rd32(inner,0x20));fflush(stdout);
   if(bad)return 7;
 }
 return 0;
}
