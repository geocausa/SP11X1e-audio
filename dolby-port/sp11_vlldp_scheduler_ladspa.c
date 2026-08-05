/* Full live VLLDP150 Windows path: exact outer scheduler + exact inner object
 * methods + exact FUN_18001f7a8 core.  Only Windows OS lock imports and the
 * C++ base-object construction plumbing are replaced; all audio processing
 * and state-machine code executes from DolbyAPOvlldp150.dll. */
#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <ladspa.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INNER_VTABLE_VA 0x18010B9A8ULL
#define CORE_CTOR_VA     0x18001BFB0ULL
#define PID5_VA          0x18001BC80ULL
#define PID17_VA         0x18001CDD0ULL
#define PID22_VA         0x18001E5C8ULL
#define PID31_VA         0x18001EE68ULL
#define AO_ENABLE_VA     0x18001B8E0ULL
#define AO_GAINS_VA      0x18001B938ULL
#define REG_ISO_VA       0x18001E420ULL
#define REG_TUNE_VA      0x18001E670ULL
#define REG_SLOPE_VA     0x18001E3B0ULL
#define REG_OVERDRIVE_VA 0x18001E510ULL
#define REG_SPKDIST_VA   0x18001E570ULL
#define REG_TIMBRE_VA    0x18001E810ULL
#define TARGET_POWER_VA  0x18001F1B0ULL
#define PEAK_LEVEL_VA    0x18001D100ULL
#define POSTGAIN_VA      0x18001D170ULL
#define SYSTEM_GAIN_VA   0x18001F150ULL
#define NOISE_GATE_EN_VA 0x18001D010ULL
#define NOISE_GATE_TH_VA 0x18001D080ULL
#define APPLY_VA         0x18001D280ULL
#define SCHED_INIT_VA    0x1800ED2C0ULL
#define SCHED_RUN_VA     0x1800ED348ULL
#define CORE_ARENA_SIZE  0x20000U
#define SCHED_SIZE        0x12C200U
#define INNER_SIZE        0x200U
#define SP11_SIG          0x41435053U
#define RT_CHUNK_FRAMES    4096U

#ifndef SP11_VLLDP_DEFAULT_DLL
#define SP11_VLLDP_DEFAULT_DLL "/home/geoca/Documents/SP11-PROJECT/04-dolby-re-work/dolby-port/dll/DolbyAPOvlldp150.dll"
#endif

typedef void *(*CoreCtorFn)(uint32_t,uint32_t,uint32_t,uint32_t,void*);
typedef void (*Pid5Fn)(void*,const uint32_t*);
typedef void (*Pid17Fn)(void*,uint32_t,const int32_t *const*);
typedef void (*Pid22Fn)(void*,uint32_t,const int32_t*);
typedef void (*Pid31Fn)(void*,const int32_t*);
typedef void (*ScalarFn)(void*,int32_t);
typedef void (*ArrayCountFn)(void*,const int32_t*,uint32_t);
typedef void (*Array20Fn)(void*,const int32_t*);
typedef void (*ArrayPair20Fn)(void*,const int32_t*,const int32_t*);
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

enum { PORT_IN_L,PORT_IN_R,PORT_OUT_L,PORT_OUT_R,PORT_BYPASS,PORT_COUNT };

typedef struct {
    LADSPA_Data *ports[PORT_COUNT];
    Sp11PeImage img;
    int img_loaded,ready,was_bypassed;
    void *core_arena,*sched,*inner;
    void *inner_in,*inner_out;
    uint32_t fmt[8];
    float *inter_in,*inter_out;
    CoreCtorFn core_ctor; Pid5Fn pid5; Pid17Fn pid17; Pid22Fn pid22; Pid31Fn pid31;
    ApplyFn apply; SchedInitFn sched_init; SchedRunFn sched_run;
} Inst;

static void lock_noop(void*p){(void)p;} static int lock_true(void*p){(void)p;return 1;}
static int initex_true(void*p,unsigned a,unsigned b){(void)p;(void)a;(void)b;return 1;}
static void piat(Sp11PeImage*i,uint64_t va,void*f){*(uintptr_t*)sp11_pe_ptr_for_va(i,va)=(uintptr_t)f;}
static void patch_runtime(Sp11PeImage*i){
 piat(i,0x1801070E0ULL,lock_noop);piat(i,0x1801070E8ULL,lock_noop);piat(i,0x180107190ULL,lock_noop);
 piat(i,0x180107198ULL,lock_true);piat(i,0x180107248ULL,lock_noop);piat(i,0x180107250ULL,initex_true);
}
static void w32(void*p,size_t o,uint32_t v){memcpy((char*)p+o,&v,4);} static void w64(void*p,size_t o,uint64_t v){memcpy((char*)p+o,&v,8);} static uint64_t r64(void*p,size_t o){uint64_t v;memcpy(&v,(char*)p+o,8);return v;}
static const char*dll_path(void){const char*p=getenv("SP11_VLLDP_DLL");return p&&*p?p:SP11_VLLDP_DEFAULT_DLL;}

static int reset_core(Inst*p){
 memset(p->core_arena,0,CORE_ARENA_SIZE); memset(p->sched,0,SCHED_SIZE); memset(p->inner,0,INNER_SIZE);
 uint8_t *core=p->core_ctor(256,48000,2,0,p->core_arena); if(!core)return -1;
 uint32_t empty[2]={0,0}; int32_t g0[6]={20,0,32767,10,20,0}; const int32_t*gp[1]={g0};
 static const int32_t ao[40]={
   -16,18,16,30,16,-32,-16,-32,-16,-32,-48,-62,-64,-64,-16,-16,-16,16,80,48,
   0,32,32,45,16,0,-16,-16,-16,0,-32,-38,-48,-48,0,0,0,32,96,64};
 static const int32_t high[20]={-74,-112,-192,-237,-238,-226,-157,0,0,0,0,0,0,0,0,0,0,0,0,0};
 static const int32_t low[20]={-266,-304,-384,-429,-430,-418,-349,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192};
 static const int32_t isolated[20]={1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0};
 int32_t stress[8]={216,216,0,0,0,0,0,0},bass[5]={0};
 p->pid5(core,empty);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,AO_ENABLE_VA))(core,1);
 ((ArrayCountFn)sp11_pe_ptr_for_va(&p->img,AO_GAINS_VA))(core,ao,40);
 p->pid17(core,1,gp);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,TARGET_POWER_VA))(core,-80);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,PEAK_LEVEL_VA))(core,0);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,POSTGAIN_VA))(core,0);
 p->pid22(core,8,stress);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,REG_SLOPE_VA))(core,14);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,REG_OVERDRIVE_VA))(core,0);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,REG_TIMBRE_VA))(core,12);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,REG_SPKDIST_VA))(core,1);
 ((ArrayPair20Fn)sp11_pe_ptr_for_va(&p->img,REG_TUNE_VA))(core,high,low);
 ((Array20Fn)sp11_pe_ptr_for_va(&p->img,REG_ISO_VA))(core,isolated);
 p->pid31(core,bass);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,SYSTEM_GAIN_VA))(core,0);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,NOISE_GATE_EN_VA))(core,0);
 ((ScalarFn)sp11_pe_ptr_for_va(&p->img,NOISE_GATE_TH_VA))(core,-1440);
 p->apply(core,2);
 uint64_t aux_owner=r64(core,0xca0); if(!aux_owner)return -2; void *aux=(void*)(uintptr_t)(aux_owner+8);
 p->fmt[0]=48000;p->fmt[1]=2;p->fmt[2]=2;p->fmt[3]=2;
 w64(p->inner,0x00,(uintptr_t)sp11_pe_ptr_for_va(&p->img,INNER_VTABLE_VA));
 w64(p->inner,0x08,(uintptr_t)p->fmt);
 /* Exact inner InitLibrary normally owns these staging buffers.  They are only
    used by the DLL's original accumulator, so allocate their exact live size. */
 if(!p->inner_in || !p->inner_out)return -3;
 memset(p->inner_in,0,0x800);memset(p->inner_out,0,0x800);
 w64(p->inner,0x10,(uintptr_t)p->inner_in);w64(p->inner,0x18,(uintptr_t)p->inner_out);w32(p->inner,0x20,0);
 w64(p->inner,0x28,(uintptr_t)core);w64(p->inner,0x30,(uintptr_t)aux);w32(p->inner,0x38,176);w32(p->inner,0x3c,256);
 w32(p->inner,0x40,2);w32(p->inner,0x50,0);w32(p->inner,0x54,0);w32(p->inner,0xa8,2);w64(p->inner,0x158,(uintptr_t)core);w32(p->inner,0x160,0);
 w32(p->sched,0,3);
 if(!p->sched_init(p->sched,p->inner,432,1,1024,2,2,0))return -4;
 p->ready=1; return 0;
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor*d,unsigned long rate){
 (void)d;if(rate!=48000)return NULL;Inst*p=calloc(1,sizeof(*p));if(!p)return NULL;
 if(sp11_pe_load(&p->img,dll_path())){free(p);return NULL;}p->img_loaded=1;patch_runtime(&p->img);
 p->core_ctor=(CoreCtorFn)sp11_pe_ptr_for_va(&p->img,CORE_CTOR_VA);p->pid5=(Pid5Fn)sp11_pe_ptr_for_va(&p->img,PID5_VA);p->pid17=(Pid17Fn)sp11_pe_ptr_for_va(&p->img,PID17_VA);p->pid22=(Pid22Fn)sp11_pe_ptr_for_va(&p->img,PID22_VA);p->pid31=(Pid31Fn)sp11_pe_ptr_for_va(&p->img,PID31_VA);p->apply=(ApplyFn)sp11_pe_ptr_for_va(&p->img,APPLY_VA);p->sched_init=(SchedInitFn)sp11_pe_ptr_for_va(&p->img,SCHED_INIT_VA);p->sched_run=(SchedRunFn)sp11_pe_ptr_for_va(&p->img,SCHED_RUN_VA);
 if(posix_memalign(&p->core_arena,64,CORE_ARENA_SIZE)||posix_memalign(&p->sched,64,SCHED_SIZE)||posix_memalign(&p->inner,64,INNER_SIZE)||posix_memalign(&p->inner_in,64,0x800)||posix_memalign(&p->inner_out,64,0x800)||posix_memalign((void**)&p->inter_in,64,RT_CHUNK_FRAMES*2*sizeof(float))||posix_memalign((void**)&p->inter_out,64,RT_CHUNK_FRAMES*2*sizeof(float))){if(p->img_loaded)sp11_pe_unload(&p->img);free(p->core_arena);free(p->sched);free(p->inner);free(p->inner_in);free(p->inner_out);free(p->inter_in);free(p->inter_out);free(p);return NULL;}
 if(reset_core(p)){sp11_pe_unload(&p->img);free(p->core_arena);free(p->sched);free(p->inner);free(p->inner_in);free(p->inner_out);free(p->inter_in);free(p->inter_out);free(p);return NULL;}return p;
}
static void connect_port(LADSPA_Handle h,unsigned long port,LADSPA_Data*d){Inst*p=h;if(port<PORT_COUNT)p->ports[port]=d;}
static void activate(LADSPA_Handle h){Inst*p=h;p->ready=reset_core(p)==0;p->was_bypassed=0;}
static void run(LADSPA_Handle h,unsigned long n){
 Inst*p=h;const float*il=p->ports[0],*ir=p->ports[1];float*ol=p->ports[2],*or=p->ports[3];if(!il||!ir||!ol||!or)return;
 int bypass=!p->ready||(p->ports[4]&&*p->ports[4]>.5f);if(bypass){for(unsigned long i=0;i<n;i++){ol[i]=il[i];or[i]=ir[i];}p->was_bypassed=1;return;}
 if(p->was_bypassed){if(reset_core(p)){p->ready=0;for(unsigned long i=0;i<n;i++){ol[i]=il[i];or[i]=ir[i];}return;}p->was_bypassed=0;}
 unsigned long pos=0;
 while(pos<n){
  uint32_t take=(uint32_t)((n-pos)>RT_CHUNK_FRAMES?RT_CHUNK_FRAMES:(n-pos));
  for(uint32_t i=0;i<take;i++){p->inter_in[2*i]=il[pos+i];p->inter_in[2*i+1]=ir[pos+i];p->inter_out[2*i]=p->inter_out[2*i+1]=0;}
  ConnProp ip={p->inter_in,take,1,SP11_SIG,0},op={p->inter_out,take,0,SP11_SIG,0};ConnProp *ipa=&ip,*opa=&op;
  p->sched_run(p->sched,1,&ipa,1,&opa,NULL);
  if(op.flag==2){memset(ol+pos,0,take*sizeof(float));memset(or+pos,0,take*sizeof(float));}
  else for(uint32_t i=0;i<take;i++){ol[pos+i]=p->inter_out[2*i];or[pos+i]=p->inter_out[2*i+1];}
  pos+=take;
 }
}
static void cleanup(LADSPA_Handle h){Inst*p=h;if(!p)return;free(p->inner_in);free(p->inner_out);free(p->inter_in);free(p->inter_out);free(p->core_arena);free(p->sched);free(p->inner);if(p->img_loaded)sp11_pe_unload(&p->img);free(p);}
static LADSPA_PortDescriptor pd[PORT_COUNT];static const char*pn[PORT_COUNT];static LADSPA_PortRangeHint ph[PORT_COUNT];static LADSPA_Descriptor desc;
const LADSPA_Descriptor*ladspa_descriptor(unsigned long i){if(i)return NULL;pd[0]=pd[1]=LADSPA_PORT_INPUT|LADSPA_PORT_AUDIO;pd[2]=pd[3]=LADSPA_PORT_OUTPUT|LADSPA_PORT_AUDIO;pd[4]=LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL;pn[0]="Input L";pn[1]="Input R";pn[2]="Output L";pn[3]="Output R";pn[4]="Bypass";memset(ph,0,sizeof(ph));ph[4].HintDescriptor=LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_TOGGLED|LADSPA_HINT_DEFAULT_0;ph[4].UpperBound=1;memset(&desc,0,sizeof(desc));desc.UniqueID=0x53503153;desc.Label="sp11_vlldp_scheduler";desc.Name="SP11 VLLDP Exact Windows Scheduler";desc.Maker="sp11 re project";desc.Copyright="research bridge; original DLL supplied separately";desc.PortCount=PORT_COUNT;desc.PortDescriptors=pd;desc.PortNames=pn;desc.PortRangeHints=ph;desc.instantiate=instantiate;desc.connect_port=connect_port;desc.activate=activate;desc.run=run;desc.cleanup=cleanup;return &desc;}
