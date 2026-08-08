#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include "sp11_vlldp_pe_loader.h"

static void sig_handler(int sig, siginfo_t *si, void *vp) {
    ucontext_t *uc=(ucontext_t*)vp;
#if defined(__aarch64__)
    fprintf(stderr,"SIGNAL %d addr=%p pc=0x%llx sp=0x%llx\n",sig,si->si_addr,
      (unsigned long long)uc->uc_mcontext.pc,(unsigned long long)uc->uc_mcontext.sp);
#else
    fprintf(stderr,"SIGNAL %d addr=%p\n",sig,si->si_addr);
#endif
    _exit(128+sig);
}
static void install_signals(void){struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_sigaction=sig_handler;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,0);sigaction(SIGILL,&sa,0);sigaction(SIGBUS,&sa,0);sigaction(SIGABRT,&sa,0);}
static void cs_void(void*p){(void)p;} static int cs_try(void*p){(void)p;return 1;}
static void patch_ptr(Sp11PeImage *img,uint64_t va,void*fn){*(uintptr_t*)sp11_pe_ptr_for_va(img,va)=(uintptr_t)fn;}
static void patch_ret(Sp11PeImage *img,uint64_t va){uint32_t*p=(uint32_t*)sp11_pe_ptr_for_va(img,va);*p=0xd65f03c0u;__builtin___clear_cache((char*)p,(char*)p+4);}

typedef struct {
    uint32_t sample_rate;
    int32_t mi_process_disable;
    int32_t virtual_bass_process_enable;
    int32_t mode;
    uint32_t max_num_objects;
    uint32_t max_num_bypass_objects;
    void *room_coefs;
    void *pca_coefs;
    int32_t dynamic_speaker_optimization_enable;
    uint32_t reserved;
} DapvrInit;

typedef struct {
    uint32_t channels;
    uint32_t pad0;
    uint64_t stride;
    uint32_t format;
    uint32_t pad1;
    float **planes;
} AudioDesc;

typedef long long (*size_fn)(DapvrInit*);
typedef void *(*ctor_fn)(DapvrInit*, void*);
typedef void (*reset_channel_fn)(void*,uint32_t);
typedef void (*set_i_fn)(void*,int);
typedef void (*set_arr_fn)(void*,const int*,uint32_t);

typedef struct { uint64_t a,b; uint32_t c; uint32_t pad; } Result;
typedef Result (*pcm_fn)(void *core, AudioDesc *in, AudioDesc *out, void *scratch);
typedef Result (*speaker_fn)(void *core, AudioDesc *in, void *scratch);
typedef uint64_t (*configure_fn)(void*,int,void*,void*,uint32_t,void*,void*,void*,uint32_t,void*,void*,uint32_t);

int main(int argc,char **argv){
    install_signals();
    const char*dll=(argc>1)?argv[1]:"/usr/lib/sp11-dolby/DolbyAudioProcessing.dll";
    Sp11PeImage img; if(sp11_pe_load(&img,dll)){fprintf(stderr,"load failed\n");return 2;}
    fprintf(stderr,"mapped=%p size=0x%zx delta=%lld\n",img.base,img.size,(long long)img.delta);
    patch_ptr(&img,0x1800e9400ULL,(void*)cs_void); patch_ptr(&img,0x1800e9408ULL,(void*)cs_void); patch_ptr(&img,0x1800e9410ULL,(void*)cs_void); patch_ptr(&img,0x1800e9438ULL,(void*)cs_void); patch_ptr(&img,0x1800e9440ULL,(void*)cs_try); patch_ret(&img,0x1800010c0ULL);
    size_fn core_size=(size_fn)sp11_pe_ptr_for_va(&img,0x180047090ULL);
    size_fn scratch_size=(size_fn)sp11_pe_ptr_for_va(&img,0x1800474e8ULL);
    ctor_fn ctor=(ctor_fn)sp11_pe_ptr_for_va(&img,0x180046020ULL);
    pcm_fn pcm=(pcm_fn)sp11_pe_ptr_for_va(&img,0x180061698ULL);
    speaker_fn speaker=(speaker_fn)sp11_pe_ptr_for_va(&img,0x18004e7b0ULL);
    configure_fn configure=(configure_fn)sp11_pe_ptr_for_va(&img,0x180046fa0ULL);
    reset_channel_fn reset_channel=(reset_channel_fn)sp11_pe_ptr_for_va(&img,0x180045678ULL);
    int init_mode=(argc>2)?atoi(argv[2]):0;
    DapvrInit init={48000,0,0,init_mode,147,2,NULL,NULL,0,0};
    long long nc=core_size(&init), ns=scratch_size(&init);
    fprintf(stderr,"DAPVR sizes core=%lld scratch=%lld init_bytes=%zu\n",nc,ns,sizeof(init));
    if(nc<=0||ns<=0||nc>(1LL<<30)||ns>(1LL<<30)){fprintf(stderr,"bad sizes\n");return 3;}
    size_t asz=(size_t)nc+65536, ssz=(size_t)ns+65536;
    void *arena=0,*scratch_raw=0; if(posix_memalign(&arena,64,asz)||posix_memalign(&scratch_raw,256,ssz+256)){perror("alloc");return 4;}
    memset(arena,0,asz); memset(scratch_raw,0,ssz+256);
    void *core=ctor(&init,arena); fprintf(stderr,"core=%p arena=%p off=%lld\n",core,arena,(long long)((uint8_t*)core-(uint8_t*)arena)); if(!core)return 5;
    for(uint32_t ch=0; ch<147; ++ch) reset_channel(core,ch);
    fprintf(stderr,"reset 147 channels ok\n");
    void *q=*(void**)((uint8_t*)core+0x13c8);
    fprintf(stderr,"queue@13c8=%p\n",q);
    if(q){ uint64_t *qq=(uint64_t*)q; fprintf(stderr,"queue q0=%016llx q1=%016llx q2=%016llx q3=%016llx\n",(unsigned long long)qq[0],(unsigned long long)qq[1],(unsigned long long)qq[2],(unsigned long long)qq[3]); }
    void *fmt30=*(void**)((uint8_t*)core+0x30); if(fmt30){uint32_t *f=(uint32_t*)fmt30; fprintf(stderr,"fmt30=%p words=%u,%u,%u,%u q28=%llx q30=%llx\n",fmt30,f[0],f[1],f[2],f[3],(unsigned long long)*(uint64_t*)((uint8_t*)fmt30+0x28),(unsigned long long)*(uint64_t*)((uint8_t*)fmt30+0x30));}
    fprintf(stderr,"child36b8=%p core_c0=%u core_c4=%u core_118=%u\n",*(void**)((uint8_t*)core+0x36b8),*(uint32_t*)((uint8_t*)core+0xc0),*(uint32_t*)((uint8_t*)core+0xc4),*(uint32_t*)((uint8_t*)core+0x118));
    fprintf(stderr,"core sr=%u max=%u active=%u mode=%u flags c0=%u c4=%u\n",
       *(uint32_t*)((uint8_t*)core+0xc),*(uint32_t*)((uint8_t*)core+0x10),*(uint32_t*)((uint8_t*)core+0x14),*(uint32_t*)((uint8_t*)core+0x18),*(uint32_t*)((uint8_t*)core+0xc0),*(uint32_t*)((uint8_t*)core+0xc4));
    enum{N=256}; float Lin[N],Rin[N],Lout[N],Rout[N]; float *ip[2]={Lin,Rin},*op[2]={Lout,Rout};
    AudioDesc in={2,0,1,7,0,ip}, out={2,0,1,7,0,op};
    uint64_t crc=configure(core,1,&in,NULL,0,NULL,NULL,NULL,0,NULL,NULL,0);
    fprintf(stderr,"configure_rc=%llu core_c4=%u core_d0=%p core_110=%u core_11c=%u\n",(unsigned long long)crc,*(uint32_t*)((uint8_t*)core+0xc4),*(void**)((uint8_t*)core+0xd0),*(uint32_t*)((uint8_t*)core+0x110),*(uint32_t*)((uint8_t*)core+0x11c));
    double phase=0,step=2*M_PI*1000.0/48000.0;
    for(int b=0;b<20;b++){
      for(int i=0;i<N;i++){float x=.1f*sinf((float)phase);phase+=step;Lin[i]=Rin[i]=x;Lout[i]=Rout[i]=0;}
      memset(scratch_raw,0,ssz+256); Result r=speaker(core,&out,scratch_raw);
      double ss=0,pk=0;int finite=1;for(int i=0;i<N;i++){double y=Lout[i];if(!isfinite(y))finite=0;ss+=y*y;if(fabs(y)>pk)pk=fabs(y);} fprintf(stderr,"b%02d result=%llx/%llx/%u rms=%g peak=%g first=%g finite=%d\n",b,(unsigned long long)r.a,(unsigned long long)r.b,r.c,sqrt(ss/N),pk,Lout[0],finite);
    }
    free(arena);free(scratch_raw);sp11_pe_unload(&img);return 0;
}
