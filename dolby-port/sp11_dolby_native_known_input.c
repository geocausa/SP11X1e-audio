#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include "/home/geoca/Documents/SP11-PROJECT/01-audio/dolby-port/sp11_vlldp_pe_loader.h"

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
static void patch_windows_runtime(Sp11PeImage *img){
    patch_ptr(img,0x1800e9400ULL,(void*)cs_void); patch_ptr(img,0x1800e9408ULL,(void*)cs_void);
    patch_ptr(img,0x1800e9410ULL,(void*)cs_void); patch_ptr(img,0x1800e9438ULL,(void*)cs_void);
    patch_ptr(img,0x1800e9440ULL,(void*)cs_try); patch_ret(img,0x1800010c0ULL);
}

typedef struct {
    uint32_t sample_rate; int32_t mi_process_disable; int32_t virtual_bass_process_enable; int32_t mode;
    uint32_t max_num_objects; uint32_t max_num_bypass_objects; void *room_coefs; void *pca_coefs;
    int32_t dynamic_speaker_optimization_enable; uint32_t reserved;
} DapvrInit;
typedef struct { uint32_t channels,pad0; uint64_t stride; uint32_t format,pad1; float **planes; } AudioDesc;
typedef struct { uint64_t a,b; uint32_t c,pad; } DapResult;

typedef long long (*dap_size_fn)(DapvrInit*);
typedef void *(*dap_ctor_fn)(DapvrInit*,void*);
typedef void (*dap_reset_ch_fn)(void*,uint32_t);
typedef DapResult (*dap_speaker_fn)(void*,AudioDesc*,void*);
typedef uint64_t (*dap_config_fn)(void*,int,void*,void*,uint32_t,void*,void*,void*,uint32_t,void*,void*,uint32_t);
typedef void (*set_i_fn)(void*,int);
typedef void (*dap_reg_tune_fn)(void*,int,const int*,const int*,const int*,const int*);

typedef struct {
    Sp11PeImage img;
    dap_size_fn core_size,scratch_size; dap_ctor_fn ctor; dap_reset_ch_fn reset_ch; dap_speaker_fn speaker; dap_config_fn configure;
    set_i_fn lvl_enable,lvl_amount,lvl_in_target,lvl_out_target,lvl_drc;
    set_i_fn reg_enable,reg_overdrive,reg_timbre,reg_relax,reg_spkdist;
    dap_reg_tune_fn reg_tuning;
} DapApi;

typedef long long (*v_size_fn)(uint32_t,uint32_t,uint32_t,uint32_t);
typedef long long (*v_scratch_fn)(uint32_t,uint32_t,uint32_t);
typedef void *(*v_ctor_fn)(uint32_t,uint32_t,uint32_t,uint32_t,void*);
typedef void (*v_reset_fn)(void*,void*);
typedef void (*v_process_fn)(void*,void*,void*,void*);
typedef void (*v_dtor_fn)(void*);
typedef void (*v_set_arr_fn)(void*,const int*,uint32_t);
typedef void (*v_set_thr_fn)(void*,const int*,const int*,int);
typedef void (*v_set_stress_fn)(void*,uint32_t,const int*);

typedef struct {
    Sp11PeImage img;
    v_size_fn core_size; v_scratch_fn scratch_size; v_ctor_fn ctor; v_reset_fn reset; v_process_fn process; v_dtor_fn dtor;
    set_i_fn ao_enable,reg_timbre,reg_slope,reg_speaker_dist,peak,target_power,system_gain,postgain,mb_enable;
    v_set_arr_fn ao_gains,reg_isolated; v_set_thr_fn reg_thresholds; v_set_stress_fn reg_stress;
} VApi;

typedef struct { void *arena,*scratch,*core; size_t asz,ssz; } DapInst;
typedef struct { void *arena,*scratch,*core; size_t asz,ssz; } VInst;

static const int CENTERS[20]={47,141,234,328,469,656,844,1031,1313,1688,2250,3000,3750,4688,5813,7125,9000,11250,13875,19688};
static const int DAP_HI[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int DAP_LO[20]={-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192};
static const int DAP_ISO[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int AO[40]={
 -16,18,16,30,16,-32,-16,-32,-16,-32,-48,-62,-64,-64,-16,-16,-16,16,80,48,
 0,32,32,45,16,0,-16,-16,-16,0,-32,-38,-48,-48,0,0,0,32,96,64};
static const int V_HI[20]={-74,-112,-192,-237,-238,-226,-157,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int V_LO[20]={-266,-304,-384,-429,-430,-418,-349,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192};
static const int V_ISO[20]={1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int V_STRESS[8]={216,216,0,0,0,0,0,0};

#define FP(img,va,type) ((type)sp11_pe_ptr_for_va((img),(va)))
static int dap_open(DapApi *a,const char*dll){memset(a,0,sizeof(*a));if(sp11_pe_load(&a->img,dll))return -1;patch_windows_runtime(&a->img);
 a->core_size=FP(&a->img,0x180047090ULL,dap_size_fn); a->scratch_size=FP(&a->img,0x1800474e8ULL,dap_size_fn); a->ctor=FP(&a->img,0x180046020ULL,dap_ctor_fn); a->reset_ch=FP(&a->img,0x180045678ULL,dap_reset_ch_fn); a->speaker=FP(&a->img,0x18004e7b0ULL,dap_speaker_fn); a->configure=FP(&a->img,0x180046fa0ULL,dap_config_fn);
 a->lvl_enable=FP(&a->img,0x1800451a0ULL,set_i_fn); a->lvl_amount=FP(&a->img,0x1800450a0ULL,set_i_fn); a->lvl_in_target=FP(&a->img,0x1800492e0ULL,set_i_fn); a->lvl_out_target=FP(&a->img,0x180049370ULL,set_i_fn); a->lvl_drc=FP(&a->img,0x180045150ULL,set_i_fn);
 a->reg_enable=FP(&a->img,0x180047800ULL,set_i_fn); a->reg_overdrive=FP(&a->img,0x180047850ULL,set_i_fn); a->reg_timbre=FP(&a->img,0x180047960ULL,set_i_fn); a->reg_relax=FP(&a->img,0x1800478b0ULL,set_i_fn); a->reg_spkdist=FP(&a->img,0x180047910ULL,set_i_fn); a->reg_tuning=FP(&a->img,0x1800479c0ULL,dap_reg_tune_fn); return 0;}
static int v_open(VApi *a,const char*dll){memset(a,0,sizeof(*a));if(sp11_pe_load(&a->img,dll))return -1;patch_windows_runtime(&a->img);
 a->core_size=FP(&a->img,0x1800914c0ULL,v_size_fn); a->scratch_size=FP(&a->img,0x180091550ULL,v_scratch_fn); a->ctor=FP(&a->img,0x1800907d8ULL,v_ctor_fn); a->reset=FP(&a->img,0x180090418ULL,v_reset_fn); a->process=FP(&a->img,0x1800922f8ULL,v_process_fn); a->dtor=FP(&a->img,0x180091cb0ULL,v_dtor_fn);
 a->ao_enable=FP(&a->img,0x1800900e0ULL,set_i_fn); a->ao_gains=FP(&a->img,0x180090120ULL,v_set_arr_fn); a->reg_thresholds=FP(&a->img,0x180091988ULL,v_set_thr_fn); a->reg_isolated=FP(&a->img,0x180091670ULL,v_set_arr_fn); a->reg_timbre=FP(&a->img,0x180091c70ULL,set_i_fn); a->reg_slope=FP(&a->img,0x1800915d0ULL,set_i_fn); a->reg_stress=FP(&a->img,0x180091890ULL,v_set_stress_fn); a->reg_speaker_dist=FP(&a->img,0x180091850ULL,set_i_fn); a->peak=FP(&a->img,0x180091430ULL,set_i_fn); a->target_power=FP(&a->img,0x180092190ULL,set_i_fn); a->system_gain=FP(&a->img,0x180092140ULL,set_i_fn); a->postgain=FP(&a->img,0x180091480ULL,set_i_fn); a->mb_enable=FP(&a->img,0x1800910d0ULL,set_i_fn); return 0;}

static int dap_new(DapApi*a,DapInst*i){memset(i,0,sizeof(*i));DapvrInit init={48000,0,0,0,147,2,NULL,NULL,0,0};long long nc=a->core_size(&init),ns=a->scratch_size(&init);if(nc<=0||ns<=0)return -1;i->asz=(size_t)nc+65536;i->ssz=(size_t)ns+65536;if(posix_memalign(&i->arena,64,i->asz)||posix_memalign(&i->scratch,256,i->ssz+256))return -2;memset(i->arena,0,i->asz);memset(i->scratch,0,i->ssz+256);i->core=a->ctor(&init,i->arena);if(!i->core)return -3;for(uint32_t ch=0;ch<147;ch++)a->reset_ch(i->core,ch);
 a->lvl_enable(i->core,1);a->lvl_amount(i->core,5);a->lvl_in_target(i->core,-320);a->lvl_out_target(i->core,-320);a->lvl_drc(i->core,1);
 a->reg_enable(i->core,1);a->reg_overdrive(i->core,0);a->reg_timbre(i->core,12);a->reg_relax(i->core,96);a->reg_spkdist(i->core,0);a->reg_tuning(i->core,20,CENTERS,DAP_HI,DAP_LO,DAP_ISO);static const int MIX[16]={16384,0,0,16384,11583,11583,8192,8192,16384,0,0,16384,16384,0,0,16384}; ((void(*)(void*,unsigned,unsigned,const int*))sp11_pe_ptr_for_va(&a->img,0x180046dd0ULL))(i->core,11,2,MIX); return 0;}
static int v_new(VApi*a,VInst*i){memset(i,0,sizeof(*i));long long nc=a->core_size(256,48000,2,0),ns=a->scratch_size(256,48000,2);if(nc<=0||ns<=0)return -1;i->asz=(size_t)nc+65536;i->ssz=(size_t)ns+65536;if(posix_memalign(&i->arena,64,i->asz)||posix_memalign(&i->scratch,64,i->ssz))return -2;memset(i->arena,0,i->asz);memset(i->scratch,0,i->ssz);i->core=a->ctor(256,48000,2,0,i->arena);if(!i->core)return -3;a->reset(i->core,NULL);
 a->ao_enable(i->core,1);a->ao_gains(i->core,AO,40);a->mb_enable(i->core,0);a->target_power(i->core,-80);a->peak(i->core,0);a->postgain(i->core,0);a->system_gain(i->core,0);a->reg_thresholds(i->core,V_HI,V_LO,20);a->reg_isolated(i->core,V_ISO,20);a->reg_timbre(i->core,12);a->reg_slope(i->core,14);a->reg_stress(i->core,8,V_STRESS);a->reg_speaker_dist(i->core,0);return 0;}
static void dap_free(DapInst*i){free(i->arena);free(i->scratch);} static void v_free(VApi*a,VInst*i){if(i->core)a->dtor(i->core);free(i->arena);free(i->scratch);}

static int wav_open_pcm16_stereo(const char *path, FILE **fp, uint32_t *rate, uint32_t *frames){
 FILE*f=fopen(path,"rb"); if(!f)return -1; char id[4]; uint32_t sz; if(fread(id,1,4,f)!=4||memcmp(id,"RIFF",4)){fclose(f);return -2;} fread(&sz,4,1,f); if(fread(id,1,4,f)!=4||memcmp(id,"WAVE",4)){fclose(f);return -3;} uint16_t fmt=0,ch=0,bits=0; uint32_t sr=0,data_bytes=0; long data_pos=0;
 while(fread(id,1,4,f)==4 && fread(&sz,4,1,f)==1){ long next=ftell(f)+sz+(sz&1); if(!memcmp(id,"fmt ",4)){fread(&fmt,2,1,f);fread(&ch,2,1,f);fread(&sr,4,1,f);fseek(f,6,SEEK_CUR);fread(&bits,2,1,f);} else if(!memcmp(id,"data",4)){data_bytes=sz;data_pos=ftell(f);break;} fseek(f,next,SEEK_SET); }
 if(fmt!=1||ch!=2||bits!=16||!data_pos){fprintf(stderr,"unsupported wav fmt=%u ch=%u bits=%u\n",fmt,ch,bits);fclose(f);return -4;} *rate=sr;*frames=data_bytes/4;fseek(f,data_pos,SEEK_SET);*fp=f;return 0; }
static void process_known(DapApi*d,VApi*v,const char*inpath,const char*outpath){ enum{N=256};FILE*f=0;uint32_t rate=0,frames=0;if(wav_open_pcm16_stereo(inpath,&f,&rate,&frames)||rate!=48000){fprintf(stderr,"wav open failed\n");exit(3);}FILE*o=fopen(outpath,"wb");if(!o){perror("out");exit(3);}DapInst di;VInst vi;if(dap_new(d,&di)||v_new(v,&vi)){fprintf(stderr,"instance failure\n");exit(3);}float inL[N],inR[N],midL[N],midR[N],inter[2*N];float*ip[2]={inL,inR},*mp[2]={midL,midR};AudioDesc din={2,0,1,7,0,ip},dout={2,0,1,7,0,mp},vd={2,0,1,3,0,mp};d->configure(di.core,1,&din,NULL,0,NULL,NULL,NULL,0,NULL,NULL,0);uint32_t done=0;int16_t pcm[2*N];double peak=0;while(done<frames){uint32_t n=frames-done;if(n>N)n=N;size_t got=fread(pcm,sizeof(int16_t)*2,n,f);if(got!=n){fprintf(stderr,"short read\n");break;}for(uint32_t j=0;j<N;j++){if(j<n){inL[j]=pcm[2*j]/32768.0f;inR[j]=pcm[2*j+1]/32768.0f;}else inL[j]=inR[j]=0;midL[j]=midR[j]=0;}memset(di.scratch,0,di.ssz+256);(void)d->speaker(di.core,&dout,di.scratch);memset(vi.scratch,0,vi.ssz);v->process(vi.core,&vd,&vd,vi.scratch);for(uint32_t j=0;j<n;j++){float l=midL[j],r=midR[j];if(!isfinite(l))l=0;if(!isfinite(r))r=0;inter[2*j]=l;inter[2*j+1]=r;if(fabs(l)>peak)peak=fabs(l);if(fabs(r)>peak)peak=fabs(r);}fwrite(inter,sizeof(float)*2,n,o);done+=n;}fprintf(stderr,"processed frames=%u seconds=%.3f raw_peak=%g (%+.3f dBFS)\n",done,done/(double)rate,peak,peak>0?20*log10(peak):-999.0);fclose(f);fclose(o);dap_free(&di);v_free(v,&vi);}
int main(void){install_signals();const char*dll="/home/geoca/Documents/SP11-PROJECT/00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/dolby/dolby-qualcomm-dissection-local/runtime-live/DolbyAudioProcessing.dll";const char*in="/home/geoca/Documents/SP11-PROJECT/00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/dolby/windows-loopback-captures/sp11-known-input-stimulus-48k.wav";DapApi d;VApi v;if(dap_open(&d,dll)||v_open(&v,dll))return 2;process_known(&d,&v,in,"/tmp/native_known_output.f32");sp11_pe_unload(&d.img);sp11_pe_unload(&v.img);return 0;}
