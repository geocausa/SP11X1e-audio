/*
 * SP11 exact live Dolby chain bridge.
 *
 * Proven Aug-8 sample dependency on SP11: DolbyApoVr -> DolbyAPOvlldp150.
 * Earlier hardware traps observed VLLDP then VR callback invocation order, but
 * did not establish buffer ownership. Full-memory buffer provenance and exact
 * captured-state replays prove VR output feeds VLLDP input.
 * Both stages execute the original shipped ARM64 PE code.  Linux replaces
 * only small Windows runtime/locking/resource plumbing.  The audio callback
 * performs no dynamic allocation and slices arbitrary host buffers into a
 * fixed preallocated realtime work buffer.
 */
#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include "sp11_audioeng_limiter.h"
#include <ladspa.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SP11_VR_OUTER_NO_MAIN
#include "sp11_vr_outer_probe.c"

#define VL_INNER_VTABLE_VA 0x18010B9A8ULL
#define VL_CORE_CTOR_VA     0x18001BFB0ULL
#define VL_PID5_VA          0x18001BC80ULL
#define VL_PID17_VA         0x18001CDD0ULL
#define VL_PID22_VA         0x18001E5C8ULL
#define VL_PID31_VA         0x18001EE68ULL
#define VL_COMP_SLOW_MIX_VA 0x18001CB20ULL
#define VL_COMP_DEVIATION_VA 0x18001CB90ULL
#define VL_COMP_SLOW_EN_VA  0x18001CC50ULL
#define VL_AO_ENABLE_VA     0x18001B8E0ULL
#define VL_AO_GAINS_VA      0x18001B938ULL
#define VL_REG_ISO_VA       0x18001E420ULL
#define VL_REG_TUNE_VA      0x18001E670ULL
#define VL_REG_SLOPE_VA     0x18001E3B0ULL
#define VL_REG_OVERDRIVE_VA 0x18001E510ULL
#define VL_REG_SPKDIST_VA   0x18001E570ULL
#define VL_REG_TIMBRE_VA    0x18001E810ULL
#define VL_TARGET_POWER_VA  0x18001F1B0ULL
#define VL_PEAK_LEVEL_VA    0x18001D100ULL
#define VL_POSTGAIN_VA      0x18001D170ULL
#define VL_SYSTEM_GAIN_VA   0x18001F150ULL
#define VL_NOISE_GATE_EN_VA 0x18001D010ULL
#define VL_NOISE_GATE_TH_VA 0x18001D080ULL
#define VL_APPLY_VA         0x18001D280ULL
#define VL_SCHED_INIT_VA    0x1800ED2C0ULL
#define VL_SCHED_RUN_VA     0x1800ED348ULL
#define VL_CORE_ARENA_SIZE  0x20000U
#define VL_SCHED_SIZE       0x12C200U
#define VL_INNER_SIZE       0x200U
#define SP11_SIG             0x41435053U
#define RT_CHUNK_FRAMES      4096U
#define VR_DEINIT_VA         0x1800DCBD0ULL
/* DolbyApoVr dap_vr_state_s scalar handlers recovered from the DLL's own
 * 39-property dispatch table. They operate on inner->core at +0x130. */
#define VR_H_LEVELER_ENABLE   0x18003D110ULL
#define VR_H_LEVELER_AMOUNT   0x18003D1D0ULL
#define VR_H_LEVELER_IN       0x18003C770ULL
#define VR_H_LEVELER_OUT      0x18003C6D0ULL
#define VR_H_LEVELER_DRC      0x18003D170ULL
#define VR_H_REG_ENABLE       0x1800333D0ULL
#define VR_H_REG_SPKDIST      0x180033370ULL
#define VR_H_REG_OVERDRIVE    0x180033220ULL
#define VR_H_REG_RELAX        0x180033300ULL
#define VR_H_REG_TIMBRE       0x180033290ULL
#define VR_H_DIALOG_ENABLE    0x180032540ULL
#define VR_H_DIALOG_AMOUNT    0x1800325A0ULL
#define VR_H_DIALOG_DUCK      0x180032610ULL
#define VR_H_IEQ_ENABLE       0x18003D0B0ULL
#define VR_H_IEQ_AMOUNT       0x18003D240ULL
#define VR_H_GEQ_ENABLE       0x180032780ULL
#define VR_H_MI_DIALOG        0x18003CB20ULL
#define VR_H_MI_LEVELER       0x18003CAC0ULL
#define VR_H_MI_IEQ           0x18003CA60ULL
#define VR_H_MI_SURR_COMP     0x18003CB80ULL
#define VR_H_MI_VIRT          0x18003CBE0ULL
#define VR_H_SURROUND_BOOST   0x1800326A0ULL
#define VR_H_SURROUND_DEC     0x180032720ULL
#define VR_H_VIRT_FRONT       0x18003C860ULL
#define VR_H_VIRT_HEIGHT      0x18003C940ULL
#define VR_H_VIRT_SURROUND    0x18003C8D0ULL
#define VR_H_VOLMAX_BOOST     0x18003C450ULL
#define VR_APPLY_RUNTIME_VA    0x18000BA58ULL
#define VR_RUNTIME_BLOB_SIZE   0xD00u

#ifndef SP11_CHAIN_DEFAULT_VLLDP_DLL
#define SP11_CHAIN_DEFAULT_VLLDP_DLL "/usr/lib/sp11-dolby/DolbyAPOvlldp150.dll"
#endif
#ifndef SP11_CHAIN_DEFAULT_VR_DLL
#define SP11_CHAIN_DEFAULT_VR_DLL "/usr/lib/sp11-dolby/DolbyAPOVR.dll"
#endif

typedef void *(*VlCoreCtorFn)(uint32_t,uint32_t,uint32_t,uint32_t,void*);
typedef void (*VlPid5Fn)(void*,const uint32_t*);
typedef void (*VlPid17Fn)(void*,uint32_t,const int32_t *const*);
typedef void (*VlPid22Fn)(void*,uint32_t,const int32_t*);
typedef void (*VlPid31Fn)(void*,const int32_t*);
typedef void (*VlScalarFn)(void*,int32_t);
typedef void (*VlArrayCountFn)(void*,const int32_t*,uint32_t);
typedef void (*VlArray20Fn)(void*,const int32_t*);
typedef void (*VlArrayPair20Fn)(void*,const int32_t*,const int32_t*);
typedef void (*VlApplyFn)(void*,uint32_t);
typedef int (*VlSchedInitFn)(void*,void*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*VlSchedRunFn)(void*,uint32_t,void*,uint32_t,void*,void*);
typedef void (*VrDeinitFn)(void*);

typedef struct {
    float *buffer;
    uint32_t frames;
    uint32_t flag;
    uint32_t signature;
    uint32_t reserved;
} VlConnProp;

enum { PORT_IN_L,PORT_IN_R,PORT_OUT_L,PORT_OUT_R,PORT_BYPASS,PORT_PROFILE,PORT_COUNT };

typedef enum {
    CHAIN_PROFILE_DYNAMIC=0, CHAIN_PROFILE_MOVIE, CHAIN_PROFILE_MUSIC,
    CHAIN_PROFILE_GAME, CHAIN_PROFILE_VOICE, CHAIN_PROFILE_ONLINECOURSE,
    CHAIN_PROFILE_PERSONALIZE, CHAIN_PROFILE_COUNT
} ChainProfile;

typedef struct {
    const char *name;
    int leveler_enable,leveler_amount,dialog_enable,dialog_amount;
    int ieq_enable,ieq_amount,mi_steering,surround_boost,surround_decoder;
    int virt_front,virt_height,virt_surround,volmax_boost,output_mode;
    int movie_music_vlldp;
    const int32_t *ieq_curve;
} ChainProfileCfg;

/* VLLDP multiband-compressor profile payloads recovered from the shipped SP11
 * tuning.  Dynamic-family profiles use one group; Movie/Music use four. */
static const int32_t vl_gd[6]={20,0,32767,10,20,0};
static const int32_t vl_gm0[6]={2,-256,12980,3,20,64};
static const int32_t vl_gm1[6]={7,-160,16366,10,20,64};
static const int32_t vl_gm2[6]={16,0,32767,10,20,0};
static const int32_t *const vl_gp_dynamic[1]={vl_gd};
static const int32_t *const vl_gp_movie_music[4]={vl_gm0,vl_gm1,vl_gm2,vl_gd};

static const int32_t vr_centers[20]={47,141,234,328,469,656,844,1031,1313,1688,2250,3000,3750,4688,5813,7125,9000,11250,13875,19688};
static const int32_t vr_ieq_balanced[20]={157,167,218,218,203,188,192,192,205,213,218,209,193,159,134,97,71,22,-90,-283};
static const int32_t vr_ieq_warm[20]={114,146,183,169,170,128,103,90,98,126,127,140,96,85,80,66,38,-32,-132,-275};
static const int32_t vr_reg_lo[20]={-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192};
static const int32_t vr_reg_hi[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t vr_reg_iso[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t vr_mix[16]={16384,0,0,16384,11583,11583,8192,8192,16384,0,0,16384,16384,0,0,16384};
static const ChainProfileCfg chain_profiles[CHAIN_PROFILE_COUNT]={
    [CHAIN_PROFILE_DYNAMIC]={"dynamic",1,5,1,5,1,10,1,96,1,10,10,10,96,11,0,vr_ieq_balanced},
    [CHAIN_PROFILE_MOVIE]={"movie",1,0,1,2,0,6,0,72,1,16,10,16,104,11,1,vr_ieq_warm},
    [CHAIN_PROFILE_MUSIC]={"music",1,0,0,5,0,6,0,24,0,10,10,10,96,1,1,vr_ieq_warm},
    [CHAIN_PROFILE_GAME]={"game",1,0,0,7,0,10,0,0,1,10,10,10,96,11,0,vr_ieq_balanced},
    [CHAIN_PROFILE_VOICE]={"voice",0,0,1,8,0,10,0,0,0,10,10,10,96,1,0,vr_ieq_balanced},
    [CHAIN_PROFILE_ONLINECOURSE]={"onlinecourse",1,0,1,5,0,10,0,0,0,10,10,10,64,1,0,vr_ieq_balanced},
    [CHAIN_PROFILE_PERSONALIZE]={"personalize",1,3,1,10,0,10,0,48,1,10,10,10,96,11,0,vr_ieq_balanced},
};

/* The shipped msft_atmos operator policy sets bypass_stereo_virtualizer=true
 * for every SP11 profile. DAX3API writes that per-profile boolean to endpoint
 * PROPERTYKEY {dc827e12-807b-4fbb-8e3c-6c62981dd3c9},1. The original
 * DolbyAPOVR LibWrapperDap2::UpdatePropertyKeys reads it and, for a 2-channel
 * stream, clears speaker_virtualizer_enable before LibWrapperVr computes the
 * final output mode. LibWrapperDap2::vfunction25 therefore returns mode 1 for
 * ordinary stereo even when a profile's raw XML output_mode requests 11.
 *
 * This LADSPA endpoint is strictly two-channel, so its Windows-equivalent
 * effective mode is always 1. Keep the raw profile output_mode above as the
 * recovered tuning contract; do not feed it directly to the core unless this
 * bridge gains a non-stereo path with the wrapper policy reproduced there. */
static uint32_t vr_effective_stereo_output_mode(const ChainProfileCfg *pc){
    (void)pc;
    return 1u;
}

typedef struct {
    LADSPA_Data *ports[PORT_COUNT];
    Sp11PeImage vl_img,vr_img;
    int vl_loaded,vr_loaded,ready;
    ChainProfile profile;
    int last_profile_request;
    int current_postgain;
    int last_postgain_request;
    int profile_control_fd;
    volatile uint8_t *profile_control;
    char profile_control_path[384];

    void *vl_core_arena,*vl_sched,*vl_inner,*vl_inner_in,*vl_inner_out;
    uint32_t vl_fmt[8];
    VlCoreCtorFn vl_core_ctor;
    VlPid5Fn vl_pid5; VlPid17Fn vl_pid17; VlPid22Fn vl_pid22; VlPid31Fn vl_pid31;
    VlApplyFn vl_apply; VlSchedInitFn vl_sched_init; VlSchedRunFn vl_sched_run;

    uint8_t *vr_outer;
    uint8_t vr_cfg[0x60] __attribute__((aligned(16)));
    uint8_t vr_resource[VR_RESOURCE_SIZE] __attribute__((aligned(16)));
    OuterHotFn vr_hot;
    VrDeinitFn vr_deinit;
    int vr_initialized;

    float *buf_a,*buf_b;
    Sp11AudioEngLimiter audioeng_limiter;
} ChainInst;

static void vl_lock_noop(void*p){(void)p;}
static int vl_lock_true(void*p){(void)p;return 1;}
static int vl_initex_true(void*p,unsigned a,unsigned b){(void)p;(void)a;(void)b;return 1;}
static void vl_piat(Sp11PeImage*i,uint64_t va,void*f){*(uintptr_t*)sp11_pe_ptr_for_va(i,va)=(uintptr_t)f;}
static void vl_patch_runtime(Sp11PeImage*i){
    vl_piat(i,0x1801070E0ULL,vl_lock_noop);vl_piat(i,0x1801070E8ULL,vl_lock_noop);
    vl_piat(i,0x180107190ULL,vl_lock_noop);vl_piat(i,0x180107198ULL,vl_lock_true);
    vl_piat(i,0x180107248ULL,vl_lock_noop);vl_piat(i,0x180107250ULL,vl_initex_true);
}
static void vl_w32(void*p,size_t o,uint32_t v){memcpy((char*)p+o,&v,4);}
static void vl_w64(void*p,size_t o,uint64_t v){memcpy((char*)p+o,&v,8);}
static uint64_t vl_r64(void*p,size_t o){uint64_t v;memcpy(&v,(char*)p+o,8);return v;}

static const char *chain_vlldp_path(void){
    const char *p=getenv("SP11_VLLDP_DLL"); return p&&*p?p:SP11_CHAIN_DEFAULT_VLLDP_DLL;
}
static const char *chain_vr_path(void){
    const char *p=getenv("SP11_VR_DLL"); return p&&*p?p:SP11_CHAIN_DEFAULT_VR_DLL;
}
static ChainProfile chain_profile_from_env(void){
    const char *v=getenv("SP11_DOLBY_PROFILE");
    if(!v || !*v || !strcasecmp(v,"dynamic")) return CHAIN_PROFILE_DYNAMIC;
    if(!strcasecmp(v,"movie")) return CHAIN_PROFILE_MOVIE;
    if(!strcasecmp(v,"music")) return CHAIN_PROFILE_MUSIC;
    if(!strcasecmp(v,"game") || !strcasecmp(v,"gaming")) return CHAIN_PROFILE_GAME;
    if(!strcasecmp(v,"voice")) return CHAIN_PROFILE_VOICE;
    if(!strcasecmp(v,"onlinecourse") || !strcasecmp(v,"online-course") || !strcasecmp(v,"course")) return CHAIN_PROFILE_ONLINECOURSE;
    if(!strcasecmp(v,"personalize") || !strcasecmp(v,"personalized") || !strncasecmp(v,"custom",6)) return CHAIN_PROFILE_PERSONALIZE;
    fprintf(stderr,"sp11-dolby: unknown SP11_DOLBY_PROFILE='%s'; using dynamic\n",v);
    return CHAIN_PROFILE_DYNAMIC;
}

static void vl_apply_profile_fields(ChainInst *p,void *core,const ChainProfileCfg *pc){
    if(pc->movie_music_vlldp) p->vl_pid17(core,4,vl_gp_movie_music);
    else p->vl_pid17(core,1,vl_gp_dynamic);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_COMP_DEVIATION_VA))(core,pc->movie_music_vlldp?96:0);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_COMP_SLOW_EN_VA))(core,pc->movie_music_vlldp?1:0);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_COMP_SLOW_MIX_VA))(core,pc->movie_music_vlldp?103:256);
}

static int vl_reset(ChainInst *p){
    const ChainProfileCfg *pc=&chain_profiles[p->profile];
    memset(p->vl_core_arena,0,VL_CORE_ARENA_SIZE); memset(p->vl_sched,0,VL_SCHED_SIZE);
    memset(p->vl_inner,0,VL_INNER_SIZE); memset(p->vl_inner_in,0,0x800); memset(p->vl_inner_out,0,0x800);
    uint8_t *core=p->vl_core_ctor(256,48000,2,0,p->vl_core_arena); if(!core)return -1;
    uint32_t empty[2]={0,0};
    static const int32_t ao[40]={
      -16,18,16,30,16,-32,-16,-32,-16,-32,-48,-62,-64,-64,-16,-16,-16,16,80,48,
      0,32,32,45,16,0,-16,-16,-16,0,-32,-38,-48,-48,0,0,0,32,96,64};
    static const int32_t high[20]={-74,-112,-192,-237,-238,-226,-157,0,0,0,0,0,0,0,0,0,0,0,0,0};
    static const int32_t low[20]={-266,-304,-384,-429,-430,-418,-349,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192};
    static const int32_t isolated[20]={1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0};
    int32_t stress[8]={216,216,0,0,0,0,0,0},bass[5]={0};
    p->vl_pid5(core,empty);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_AO_ENABLE_VA))(core,1);
    ((VlArrayCountFn)sp11_pe_ptr_for_va(&p->vl_img,VL_AO_GAINS_VA))(core,ao,40);
    vl_apply_profile_fields(p,core,pc);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_TARGET_POWER_VA))(core,-80);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_PEAK_LEVEL_VA))(core,0);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_POSTGAIN_VA))(core,p->current_postgain);
    p->vl_pid22(core,8,stress);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_REG_SLOPE_VA))(core,14);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_REG_OVERDRIVE_VA))(core,0);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_REG_TIMBRE_VA))(core,12);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_REG_SPKDIST_VA))(core,1);
    ((VlArrayPair20Fn)sp11_pe_ptr_for_va(&p->vl_img,VL_REG_TUNE_VA))(core,high,low);
    ((VlArray20Fn)sp11_pe_ptr_for_va(&p->vl_img,VL_REG_ISO_VA))(core,isolated);
    p->vl_pid31(core,bass);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_SYSTEM_GAIN_VA))(core,0);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_NOISE_GATE_EN_VA))(core,0);
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_NOISE_GATE_TH_VA))(core,-1440);
    p->vl_apply(core,2);
    uint64_t aux_owner=vl_r64(core,0xca0); if(!aux_owner)return -2; void *aux=(void*)(uintptr_t)(aux_owner+8);
    p->vl_fmt[0]=48000;p->vl_fmt[1]=2;p->vl_fmt[2]=2;p->vl_fmt[3]=2;
    vl_w64(p->vl_inner,0x00,(uintptr_t)sp11_pe_ptr_for_va(&p->vl_img,VL_INNER_VTABLE_VA));
    vl_w64(p->vl_inner,0x08,(uintptr_t)p->vl_fmt);vl_w64(p->vl_inner,0x10,(uintptr_t)p->vl_inner_in);
    vl_w64(p->vl_inner,0x18,(uintptr_t)p->vl_inner_out);vl_w32(p->vl_inner,0x20,0);
    vl_w64(p->vl_inner,0x28,(uintptr_t)core);vl_w64(p->vl_inner,0x30,(uintptr_t)aux);
    vl_w32(p->vl_inner,0x38,176);vl_w32(p->vl_inner,0x3c,256);vl_w32(p->vl_inner,0x40,2);
    vl_w32(p->vl_inner,0x50,0);vl_w32(p->vl_inner,0x54,0);vl_w32(p->vl_inner,0xa8,2);
    vl_w64(p->vl_inner,0x158,(uintptr_t)core);vl_w32(p->vl_inner,0x160,0);
    vl_w32(p->vl_sched,0,3);
    return p->vl_sched_init(p->vl_sched,p->vl_inner,432,1,1024,2,2,0)?0:-3;
}

typedef void (*VrScalarHandlerFn)(void*,int);
static void vr_scalar(ChainInst *p,void *core,uint64_t va,int value){
    ((VrScalarHandlerFn)sp11_pe_ptr_for_va(&p->vr_img,va))(core,value);
}
typedef void (*VrOutputModeFn)(void*,uint32_t,uint32_t,const int32_t*);
typedef int (*VrBandGridFn)(void*,void*,uint32_t,const int32_t*,uint32_t);
typedef int (*VrBandTargetFn)(void*,void*,const int32_t*,int32_t,int32_t);
typedef void (*VrRegTuneFn)(void*,uint32_t,const int32_t*,const int32_t*,const int32_t*,const int32_t*);
#define VR_OUTPUT_MODE_VA      0x180032320ULL
#define VR_BAND_GRID_VA        0x18004C560ULL
#define VR_BAND_TARGET_VA      0x18004C8E8ULL
#define VR_REG_TUNING_VA       0x1800463C0ULL
static int vr_parse_geq(int32_t target[20]){
    const char *v=getenv("SP11_DOLBY_GEQ");
    if(!v || !*v || !strcasecmp(v,"off") || !strcasecmp(v,"flat")) return 0;
    for(int i=0;i<20;i++){
        char *end=NULL; long x=strtol(v,&end,10);
        if(end==v || x < -192 || x > 192) return -1;
        target[i]=(int32_t)x;
        if(i<19){ if(*end!=',') return -1; v=end+1; }
        else { while(*end==' '||*end=='\t')end++; if(*end!='\0')return -1; }
    }
    return 1;
}

static int vr_apply_geq(ChainInst *p,void *core){
    int32_t target[20]; int have=vr_parse_geq(target);
    if(have<0){fprintf(stderr,"sp11-dolby: invalid SP11_DOLBY_GEQ; GEQ disabled\n");vr_scalar(p,core,VR_H_GEQ_ENABLE,0);return 0;}
    if(!have){vr_scalar(p,core,VR_H_GEQ_ENABLE,0);return 0;}
    void *layout=(void*)(uintptr_t)q(core,0x28); if(!layout)return -2;
    void *freqmap=(void*)(uintptr_t)q(layout,0x48); uint32_t nmap=d(layout,0x0c);
    if(!freqmap || !nmap)return -3;
    int gr=((VrBandGridFn)sp11_pe_ptr_for_va(&p->vr_img,VR_BAND_GRID_VA))((uint8_t*)core+0xaa4,freqmap,nmap,vr_centers,20);
    if(gr!=2){
        int tr=((VrBandTargetFn)sp11_pe_ptr_for_va(&p->vr_img,VR_BAND_TARGET_VA))((uint8_t*)core+0xaa4,(uint8_t*)core+0xa50,target,-576,576);
        if(tr)wd(core,0xaa0,1);
    }
    if(d(core,0xaa0))wd(core,0x1278,1);
    vr_scalar(p,core,VR_H_GEQ_ENABLE,1);
    return 0;
}

static int vr_apply_ieq_curve(ChainInst *p,void *core,const int32_t *curve){
    void *layout=(void*)(uintptr_t)q(core,0x28); if(!layout)return -1;
    void *freqmap=(void*)(uintptr_t)q(layout,0x48); uint32_t nmap=d(layout,0x0c);
    if(!freqmap || !nmap)return -2;
    int gr=((VrBandGridFn)sp11_pe_ptr_for_va(&p->vr_img,VR_BAND_GRID_VA))((uint8_t*)core+0x754,freqmap,nmap,vr_centers,20);
    if(gr!=2){
        int tr=((VrBandTargetFn)sp11_pe_ptr_for_va(&p->vr_img,VR_BAND_TARGET_VA))((uint8_t*)core+0x754,(uint8_t*)core+0x704,curve,-480,480);
        if(tr)wd(core,0x6fc,1);
    }
    if(d(core,0x6fc))wd(core,0x1278,1);
    return 0;
}

static int vr_apply_profile_complex(ChainInst *p,void *core,const ChainProfileCfg *pc){
    const char *disable=getenv("SP11_VR_COMPLEX_PROFILE");
    if(disable && (!strcmp(disable,"0") || !strcasecmp(disable,"off") || !strcasecmp(disable,"false"))) return 0;
    const char *parts=getenv("SP11_VR_COMPLEX_PARTS");
    int do_output=!parts || strstr(parts,"output");
    int do_ieq=!parts || strstr(parts,"ieq");
    int do_reg=!parts || strstr(parts,"reg");
    if(do_output) ((VrOutputModeFn)sp11_pe_ptr_for_va(&p->vr_img,VR_OUTPUT_MODE_VA))(core,vr_effective_stereo_output_mode(pc),2,vr_mix);
    if(do_ieq && vr_apply_ieq_curve(p,core,pc->ieq_curve))return -1;
    if(do_reg){
        ((VrRegTuneFn)sp11_pe_ptr_for_va(&p->vr_img,VR_REG_TUNING_VA))((uint8_t*)core+0xdc0,20,vr_centers,vr_reg_lo,vr_reg_hi,vr_reg_iso);
        if(d(core,0xdcc))wd(core,0x1278,1);
    }
    return 0;
}

static int vr_apply_profile(ChainInst *p,uint8_t *inner){
    const char *disable=getenv("SP11_VR_DYNAMIC_PROFILE"); /* legacy debug switch */
    if(disable && (!strcmp(disable,"0") || !strcasecmp(disable,"off") || !strcasecmp(disable,"false"))) return 0;
    const ChainProfileCfg *pc=&chain_profiles[p->profile];
    void *core=(void*)(uintptr_t)q(inner,0x130); if(!core)return -1;
    vr_scalar(p,core,VR_H_LEVELER_ENABLE,pc->leveler_enable);
    vr_scalar(p,core,VR_H_LEVELER_AMOUNT,pc->leveler_amount);
    vr_scalar(p,core,VR_H_LEVELER_IN,-320);
    vr_scalar(p,core,VR_H_LEVELER_OUT,-320);
    vr_scalar(p,core,VR_H_LEVELER_DRC,1);
    vr_scalar(p,core,VR_H_REG_ENABLE,1);
    vr_scalar(p,core,VR_H_REG_SPKDIST,0);
    vr_scalar(p,core,VR_H_REG_OVERDRIVE,0);
    vr_scalar(p,core,VR_H_REG_RELAX,96);
    vr_scalar(p,core,VR_H_REG_TIMBRE,12);
    vr_scalar(p,core,VR_H_DIALOG_ENABLE,pc->dialog_enable);
    vr_scalar(p,core,VR_H_DIALOG_AMOUNT,pc->dialog_amount);
    vr_scalar(p,core,VR_H_DIALOG_DUCK,0);
    vr_scalar(p,core,VR_H_IEQ_ENABLE,pc->ieq_enable);
    vr_scalar(p,core,VR_H_IEQ_AMOUNT,pc->ieq_amount);
    if(p->profile==CHAIN_PROFILE_PERSONALIZE){ if(vr_apply_geq(p,core)<0)return -2; }
    else vr_scalar(p,core,VR_H_GEQ_ENABLE,0);
    vr_scalar(p,core,VR_H_MI_DIALOG,pc->mi_steering);
    vr_scalar(p,core,VR_H_MI_LEVELER,pc->mi_steering);
    vr_scalar(p,core,VR_H_MI_IEQ,pc->mi_steering);
    vr_scalar(p,core,VR_H_MI_SURR_COMP,pc->mi_steering);
    vr_scalar(p,core,VR_H_MI_VIRT,pc->mi_steering);
    vr_scalar(p,core,VR_H_SURROUND_BOOST,pc->surround_boost);
    vr_scalar(p,core,VR_H_SURROUND_DEC,pc->surround_decoder);
    vr_scalar(p,core,VR_H_VIRT_FRONT,pc->virt_front);
    vr_scalar(p,core,VR_H_VIRT_HEIGHT,pc->virt_height);
    vr_scalar(p,core,VR_H_VIRT_SURROUND,pc->virt_surround);
    vr_scalar(p,core,VR_H_VOLMAX_BOOST,pc->volmax_boost);
    return vr_apply_profile_complex(p,core,pc);
}

static int vr_retarget_profile(ChainInst *p,void *core,ChainProfile old_profile,ChainProfile new_profile){
    const char *disable=getenv("SP11_VR_DYNAMIC_PROFILE");
    if(disable && (!strcmp(disable,"0") || !strcasecmp(disable,"off") || !strcasecmp(disable,"false"))) return 0;
    const ChainProfileCfg *old=&chain_profiles[old_profile],*pc=&chain_profiles[new_profile];
#define VR_SCALAR_CHANGED(field,handler) do{if(old->field!=pc->field)vr_scalar(p,core,handler,pc->field);}while(0)
    VR_SCALAR_CHANGED(leveler_enable,VR_H_LEVELER_ENABLE);
    VR_SCALAR_CHANGED(leveler_amount,VR_H_LEVELER_AMOUNT);
    VR_SCALAR_CHANGED(dialog_enable,VR_H_DIALOG_ENABLE);
    VR_SCALAR_CHANGED(dialog_amount,VR_H_DIALOG_AMOUNT);
    VR_SCALAR_CHANGED(ieq_enable,VR_H_IEQ_ENABLE);
    VR_SCALAR_CHANGED(ieq_amount,VR_H_IEQ_AMOUNT);
    if(old->mi_steering!=pc->mi_steering){
        vr_scalar(p,core,VR_H_MI_DIALOG,pc->mi_steering);
        vr_scalar(p,core,VR_H_MI_LEVELER,pc->mi_steering);
        vr_scalar(p,core,VR_H_MI_IEQ,pc->mi_steering);
        vr_scalar(p,core,VR_H_MI_SURR_COMP,pc->mi_steering);
        vr_scalar(p,core,VR_H_MI_VIRT,pc->mi_steering);
    }
    VR_SCALAR_CHANGED(surround_boost,VR_H_SURROUND_BOOST);
    VR_SCALAR_CHANGED(surround_decoder,VR_H_SURROUND_DEC);
    VR_SCALAR_CHANGED(virt_front,VR_H_VIRT_FRONT);
    VR_SCALAR_CHANGED(virt_height,VR_H_VIRT_HEIGHT);
    VR_SCALAR_CHANGED(virt_surround,VR_H_VIRT_SURROUND);
    VR_SCALAR_CHANGED(volmax_boost,VR_H_VOLMAX_BOOST);
#undef VR_SCALAR_CHANGED

    if(old_profile!=CHAIN_PROFILE_PERSONALIZE && new_profile==CHAIN_PROFILE_PERSONALIZE){
        if(vr_apply_geq(p,core)<0)return -2;
    } else if(old_profile==CHAIN_PROFILE_PERSONALIZE && new_profile!=CHAIN_PROFILE_PERSONALIZE){
        vr_scalar(p,core,VR_H_GEQ_ENABLE,0);
    }

    const char *complex_disable=getenv("SP11_VR_COMPLEX_PROFILE");
    if(!(complex_disable && (!strcmp(complex_disable,"0") || !strcasecmp(complex_disable,"off") || !strcasecmp(complex_disable,"false")))){
        const char *parts=getenv("SP11_VR_COMPLEX_PARTS");
        uint32_t old_mode=vr_effective_stereo_output_mode(old),new_mode=vr_effective_stereo_output_mode(pc);
        if(old_mode!=new_mode && (!parts || strstr(parts,"output")))
            ((VrOutputModeFn)sp11_pe_ptr_for_va(&p->vr_img,VR_OUTPUT_MODE_VA))(core,new_mode,2,vr_mix);
        if(old->ieq_curve!=pc->ieq_curve && (!parts || strstr(parts,"ieq")) && vr_apply_ieq_curve(p,core,pc->ieq_curve))return -3;
    }
    return 0;
}

static int chain_apply_profile_inplace(ChainInst *p,ChainProfile next){
    if(!p || !p->ready || next<0 || next>=CHAIN_PROFILE_COUNT)return -1;
    ChainProfile old=p->profile; if(old==next)return 0;
    uint8_t *vr_inner=(uint8_t*)(uintptr_t)q(p->vr_outer,VR_INNER_PTR_OFF);
    void *vr_core=vr_inner?(void*)(uintptr_t)q(vr_inner,0x130):NULL;
    void *vl_core=(void*)(uintptr_t)vl_r64(p->vl_inner,0x28);
    if(!vr_core || !vl_core)return -2;
    if(vr_retarget_profile(p,vr_core,old,next))return -3;
    const ChainProfileCfg *oldpc=&chain_profiles[old],*newpc=&chain_profiles[next];
    if(oldpc->movie_music_vlldp!=newpc->movie_music_vlldp){
        vl_apply_profile_fields(p,vl_core,newpc);
        p->vl_apply(vl_core,2);
    }
    p->profile=next;
    return 0;
}

#define PROFILE_CONTROL_BYTES          12u
#define PROFILE_CONTROL_NONE           0u
#define POSTGAIN_CONTROL_REQUEST_OFF   4u
#define POSTGAIN_CONTROL_ACK_OFF       8u
#define POSTGAIN_CONTROL_NONE          INT32_MIN
#define POSTGAIN_CONTROL_MIN          -1200
#define POSTGAIN_CONTROL_MAX              0

static int chain_profile_code_from_port(const LADSPA_Data *v){
    if(!v || *v!=*v)return PROFILE_CONTROL_NONE;
    int code=(int)(*v+.5f);
    return code>=1 && code<=CHAIN_PROFILE_COUNT?code:(int)PROFILE_CONTROL_NONE;
}

static int chain_profile_control_open(ChainInst *p){
    const char *override=getenv("SP11_DOLBY_CONTROL_PATH");
    if(override && *override){
        if(!strcasecmp(override,"off") || !strcasecmp(override,"none") || !strcmp(override,"0"))return 1;
        if(snprintf(p->profile_control_path,sizeof(p->profile_control_path),"%s",override)>=(int)sizeof(p->profile_control_path))return -1;
    } else {
        const char *runtime=getenv("XDG_RUNTIME_DIR");
        if(runtime && *runtime){
            if(snprintf(p->profile_control_path,sizeof(p->profile_control_path),"%s/sp11-dolby-profile.control",runtime)>=(int)sizeof(p->profile_control_path))return -1;
        } else if(snprintf(p->profile_control_path,sizeof(p->profile_control_path),"/run/user/%lu/sp11-dolby-profile.control",(unsigned long)getuid())>=(int)sizeof(p->profile_control_path)) return -1;
    }
    int fd=open(p->profile_control_path,O_RDWR|O_CREAT|O_CLOEXEC,0600);
    if(fd<0)return -2;
    struct stat st;
    if(fstat(fd,&st) || fchmod(fd,0600)){close(fd);return -3;}
    int had_profile_request=st.st_size>0;
    int had_postgain_request=st.st_size>=(off_t)(POSTGAIN_CONTROL_REQUEST_OFF+sizeof(int32_t));
    if(ftruncate(fd,PROFILE_CONTROL_BYTES)){close(fd);return -4;}
    const uint8_t zero=0;
    const int32_t postgain_none=POSTGAIN_CONTROL_NONE;
    /* Helpers may queue profile/postgain before lazy LADSPA instantiation.
     * Preserve request slots that already existed. Ack slots belong to the
     * current plugin instance and are always reset until the first callback. */
    if(!had_profile_request && pwrite(fd,&zero,1,0)!=1){close(fd);return -5;}
    if(pwrite(fd,&zero,1,1)!=1){close(fd);return -6;}
    if(!had_postgain_request && pwrite(fd,&postgain_none,sizeof(postgain_none),POSTGAIN_CONTROL_REQUEST_OFF)!=(ssize_t)sizeof(postgain_none)){close(fd);return -8;}
    if(pwrite(fd,&postgain_none,sizeof(postgain_none),POSTGAIN_CONTROL_ACK_OFF)!=(ssize_t)sizeof(postgain_none)){close(fd);return -9;}
    void *map=mmap(NULL,PROFILE_CONTROL_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(map==MAP_FAILED){close(fd);return -7;}
    p->profile_control_fd=fd;
    p->profile_control=(volatile uint8_t*)map;
    return 0;
}

static int chain_requested_profile(ChainInst *p,uint8_t *code_out){
    uint8_t code=PROFILE_CONTROL_NONE;
    if(p->profile_control)code=__atomic_load_n(p->profile_control,__ATOMIC_ACQUIRE);
    if(code<1 || code>CHAIN_PROFILE_COUNT)code=(uint8_t)chain_profile_code_from_port(p->ports[PORT_PROFILE]);
    if(code_out)*code_out=code;
    return code>=1 && code<=CHAIN_PROFILE_COUNT?(int)code-1:-1;
}

static void chain_ack_profile(ChainInst *p,uint8_t code){
    if(p->profile_control && code>=1 && code<=CHAIN_PROFILE_COUNT)
        __atomic_store_n(p->profile_control+1,code,__ATOMIC_RELEASE);
}

static int chain_requested_postgain(ChainInst *p,int32_t *value_out){
    if(!p->profile_control)return 0;
    volatile int32_t *slot=(volatile int32_t*)(p->profile_control+POSTGAIN_CONTROL_REQUEST_OFF);
    int32_t value=__atomic_load_n(slot,__ATOMIC_ACQUIRE);
    if(value<POSTGAIN_CONTROL_MIN || value>POSTGAIN_CONTROL_MAX)return 0;
    if(value_out)*value_out=value;
    return 1;
}

static void chain_ack_postgain(ChainInst *p,int32_t value){
    if(!p->profile_control)return;
    volatile int32_t *slot=(volatile int32_t*)(p->profile_control+POSTGAIN_CONTROL_ACK_OFF);
    __atomic_store_n(slot,value,__ATOMIC_RELEASE);
}

static int chain_apply_postgain_inplace(ChainInst *p,int32_t value){
    if(value<POSTGAIN_CONTROL_MIN || value>POSTGAIN_CONTROL_MAX)return -1;
    void *core=p->vl_inner?(void*)(uintptr_t)vl_r64(p->vl_inner,0x28):NULL;
    if(!core)return -2;
    ((VlScalarFn)sp11_pe_ptr_for_va(&p->vl_img,VL_POSTGAIN_VA))(core,value);
    p->vl_apply(core,2);
    p->current_postgain=value;
    return 0;
}

static int vr_build(ChainInst *p){
    if(p->vr_initialized && p->vr_deinit){
        p->vr_deinit(p->vr_outer+VR_INNER_OFF);
        p->vr_initialized=0;
    }
    memset(p->vr_outer,0,VR_OUTER_SIZE);
    ((OuterCtorFn)sp11_pe_ptr_for_va(&p->vr_img,VR_OUTER_CTOR_VA))(p->vr_outer);
    finish_outer_factory_vtables(&p->vr_img,p->vr_outer);
    uint8_t *inner=(uint8_t*)(uintptr_t)q(p->vr_outer,VR_INNER_PTR_OFF);
    if(inner!=p->vr_outer+VR_INNER_OFF)return -1;
    /* Resource shim has no Windows module context; point it at this instance's
       immutable copy while InitLibrary consumes it. Audio processing does not
       consult the resource API afterwards. */
    g_resource_data=p->vr_resource;
    if(init_outer_inner(&p->vr_img,p->vr_outer,p->vr_cfg))return -2;
    if(vr_apply_profile(p,inner))return -5;
    uint32_t window,stride,enabled; dump_inner_tuple(&p->vr_img,inner,&window,&stride,&enabled);
    if(window!=768||stride!=1024||enabled!=1)return -3;
    if(!((TransInitFn)sp11_pe_ptr_for_va(&p->vr_img,VR_TRANS_INIT_VA))
       (p->vr_outer+VR_TRANS_OFF,inner,window,enabled,stride,2,2,0))return -4;
    p->vr_initialized=1;
    return 0;
}

static int chain_alloc(ChainInst *p){
    if(posix_memalign(&p->vl_core_arena,64,VL_CORE_ARENA_SIZE)||
       posix_memalign(&p->vl_sched,64,VL_SCHED_SIZE)||posix_memalign(&p->vl_inner,64,VL_INNER_SIZE)||
       posix_memalign(&p->vl_inner_in,64,0x800)||posix_memalign(&p->vl_inner_out,64,0x800)||
       posix_memalign((void**)&p->vr_outer,64,VR_OUTER_SIZE)||
       posix_memalign((void**)&p->buf_a,64,RT_CHUNK_FRAMES*2*sizeof(float))||
       posix_memalign((void**)&p->buf_b,64,RT_CHUNK_FRAMES*2*sizeof(float))) return -1;
    return 0;
}

static void chain_free_mem(ChainInst *p){
    if(p->profile_control){munmap((void*)p->profile_control,PROFILE_CONTROL_BYTES);p->profile_control=NULL;}
    if(p->profile_control_fd>=0){close(p->profile_control_fd);p->profile_control_fd=-1;}
    /* Keep the control page across filter-chain restarts.  It lives in the
     * per-user runtime directory (cleared at logout) and may already contain
     * a profile/postgain request queued by helpers before the next instance. */
    free(p->buf_b);free(p->buf_a);free(p->vr_outer);free(p->vl_inner_out);free(p->vl_inner_in);
    free(p->vl_inner);free(p->vl_sched);free(p->vl_core_arena);
}

static LADSPA_Handle chain_instantiate(const LADSPA_Descriptor*d,unsigned long rate){
    (void)d;if(rate!=48000)return NULL;
    ChainInst *p=calloc(1,sizeof(*p));if(!p)return NULL;
    p->profile=chain_profile_from_env();
    p->last_profile_request=-2;
    p->current_postgain=0;
    p->last_postgain_request=INT32_MIN;
    p->profile_control_fd=-1;
    sp11_audioeng_limiter_init(&p->audioeng_limiter);
    int control_rc=chain_profile_control_open(p);
    if(control_rc<0)fprintf(stderr,"sp11-dolby: runtime profile control unavailable; LADSPA startup control only\n");
    /* With the production runtime control enabled, never begin a new endpoint
     * at postgain 0 while WirePlumber is still restoring its saved volume.
     * Honor an already queued request; otherwise start at the recovered SP11
     * minimum (-75 dB) until the volume monitor supplies the real endpoint
     * attenuation. Offline/control-disabled research keeps the historical 0. */
    if(control_rc==0){
        int32_t queued_postgain=0;
        p->current_postgain=chain_requested_postgain(p,&queued_postgain)?queued_postgain:POSTGAIN_CONTROL_MIN;
    }
    if(chain_alloc(p)){chain_free_mem(p);free(p);return NULL;}

    if(sp11_pe_load(&p->vl_img,chain_vlldp_path())) goto fail;
    p->vl_loaded=1;
    vl_patch_runtime(&p->vl_img);
    p->vl_core_ctor=(VlCoreCtorFn)sp11_pe_ptr_for_va(&p->vl_img,VL_CORE_CTOR_VA);
    p->vl_pid5=(VlPid5Fn)sp11_pe_ptr_for_va(&p->vl_img,VL_PID5_VA);p->vl_pid17=(VlPid17Fn)sp11_pe_ptr_for_va(&p->vl_img,VL_PID17_VA);
    p->vl_pid22=(VlPid22Fn)sp11_pe_ptr_for_va(&p->vl_img,VL_PID22_VA);p->vl_pid31=(VlPid31Fn)sp11_pe_ptr_for_va(&p->vl_img,VL_PID31_VA);
    p->vl_apply=(VlApplyFn)sp11_pe_ptr_for_va(&p->vl_img,VL_APPLY_VA);p->vl_sched_init=(VlSchedInitFn)sp11_pe_ptr_for_va(&p->vl_img,VL_SCHED_INIT_VA);p->vl_sched_run=(VlSchedRunFn)sp11_pe_ptr_for_va(&p->vl_img,VL_SCHED_RUN_VA);
    if(vl_reset(p))goto fail;

    if(init_dll(&p->vr_img,chain_vr_path())) goto fail;
    p->vr_loaded=1;
    memcpy(p->vr_resource,sp11_pe_ptr_for_va(&p->vr_img,VR_RESOURCE_VA),VR_RESOURCE_SIZE);
    p->vr_hot=(OuterHotFn)sp11_pe_ptr_for_va(&p->vr_img,VR_OUTER_HOT_VA);
    p->vr_deinit=(VrDeinitFn)sp11_pe_ptr_for_va(&p->vr_img,VR_DEINIT_VA);
    if(vr_build(p))goto fail;
    p->ready=1;return p;
fail:
    if(p->vr_initialized&&p->vr_deinit)p->vr_deinit(p->vr_outer+VR_INNER_OFF);
    if(p->vr_loaded) sp11_pe_unload(&p->vr_img);
    if(p->vl_loaded) sp11_pe_unload(&p->vl_img);
    chain_free_mem(p);free(p);return NULL;
}

static void chain_connect(LADSPA_Handle h,unsigned long port,LADSPA_Data *d){ChainInst*p=h;if(port<PORT_COUNT)p->ports[port]=d;}
static void chain_activate(LADSPA_Handle h){
    ChainInst *p=h;
    if(!p)return;
    /* PipeWire filter-chain calls LADSPA activate() from graph reset when the
       playback stream enters PAUSED. Windows CApoBase::Reset is a no-op for
       both shipped VLLDP150 and VR APOs, so rebuilding here incorrectly drops
       minutes-long adaptive Leveler/regulator history on every idle transition.
       Instantiation (and service/profile recreation) remains the cold-start
       boundary. Only recover here if construction was not ready. */
    if(p->ready)return;
    p->ready=(vl_reset(p)==0 && vr_build(p)==0);
}

static void chain_run(LADSPA_Handle h,unsigned long n){
    ChainInst*p=h;const float*il=p->ports[0],*ir=p->ports[1];float*ol=p->ports[2],*or=p->ports[3];
    if(!il||!ir||!ol||!or)return;
    if(!p->ready){for(unsigned long i=0;i<n;i++){ol[i]=il[i];or[i]=ir[i];}return;}
    uint8_t profile_code=PROFILE_CONTROL_NONE;
    int requested=chain_requested_profile(p,&profile_code);
    if(requested>=0 && requested!=(int)p->profile && requested!=p->last_profile_request){
        p->last_profile_request=requested;
        if(chain_apply_profile_inplace(p,(ChainProfile)requested)==0)chain_ack_profile(p,profile_code);
    } else if(requested==(int)p->profile){
        p->last_profile_request=requested;
        chain_ack_profile(p,profile_code);
    }
    int32_t postgain_request=0;
    if(chain_requested_postgain(p,&postgain_request)){
        if(postgain_request!=p->current_postgain && postgain_request!=p->last_postgain_request){
            p->last_postgain_request=postgain_request;
            if(chain_apply_postgain_inplace(p,postgain_request)==0)chain_ack_postgain(p,postgain_request);
        } else if(postgain_request==p->current_postgain){
            p->last_postgain_request=postgain_request;
            chain_ack_postgain(p,postgain_request);
        }
    }
    int dry=p->ports[PORT_BYPASS]&&*p->ports[PORT_BYPASS]>.5f;
    unsigned long pos=0;
    while(pos<n){
        uint32_t take=(uint32_t)((n-pos)>RT_CHUNK_FRAMES?RT_CHUNK_FRAMES:(n-pos));
        for(uint32_t i=0;i<take;i++){p->buf_a[2*i]=il[pos+i];p->buf_a[2*i+1]=ir[pos+i];p->buf_b[2*i]=p->buf_b[2*i+1]=0.0f;}
        /* Sample dependency proved from Aug-8 full-memory captures:
         * source -> DolbyApoVr -> DolbyAPOvlldp150.  The older VLLDP/VR
         * hardware-breakpoint ordering is scheduler invocation order only. */
        Conn ri={p->buf_a,take,1},ro={p->buf_b,0,0};Conn *rip=&ri,*rop=&ro;
        p->vr_hot(p->vr_outer+VR_RT_OFF,1,&rip,1,&rop,NULL);
        if(ro.flags==2)memset(p->buf_b,0,(size_t)take*2*sizeof(float));

        VlConnProp vip={p->buf_b,take,ro.flags?ro.flags:1,SP11_SIG,0},vop={p->buf_a,take,0,SP11_SIG,0};VlConnProp *vipa=&vip,*vopa=&vop;
        p->vl_sched_run(p->vl_sched,1,&vipa,1,&vopa,NULL);
        if(vop.flag==2)memset(p->buf_a,0,(size_t)take*2*sizeof(float));
        for(uint32_t i=0;i<take;i++){
            float limited_l,limited_r;
            sp11_audioeng_limiter_process_frame(&p->audioeng_limiter,p->buf_a[2*i],p->buf_a[2*i+1],&limited_l,&limited_r);
            if(dry){ol[pos+i]=il[pos+i];or[pos+i]=ir[pos+i];}
            else {ol[pos+i]=limited_l;or[pos+i]=limited_r;}
        }
        pos+=take;
    }
}

static void chain_cleanup(LADSPA_Handle h){
    ChainInst*p=h;if(!p)return;
    if(p->vr_initialized&&p->vr_deinit)p->vr_deinit(p->vr_outer+VR_INNER_OFF);
    if(p->vr_loaded) sp11_pe_unload(&p->vr_img);
    if(p->vl_loaded) sp11_pe_unload(&p->vl_img);
    chain_free_mem(p);free(p);
}

static LADSPA_PortDescriptor pd[PORT_COUNT];static const char*pn[PORT_COUNT];static LADSPA_PortRangeHint ph[PORT_COUNT];static LADSPA_Descriptor desc;
const LADSPA_Descriptor *ladspa_descriptor(unsigned long i){
    if(i)return NULL;
    pd[0]=pd[1]=LADSPA_PORT_INPUT|LADSPA_PORT_AUDIO;pd[2]=pd[3]=LADSPA_PORT_OUTPUT|LADSPA_PORT_AUDIO;
    pd[PORT_BYPASS]=pd[PORT_PROFILE]=LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL;
    pn[0]="Input L";pn[1]="Input R";pn[2]="Output L";pn[3]="Output R";pn[PORT_BYPASS]="Bypass";pn[PORT_PROFILE]="Profile";
    memset(ph,0,sizeof(ph));ph[PORT_BYPASS].HintDescriptor=LADSPA_HINT_TOGGLED|LADSPA_HINT_DEFAULT_0;
    ph[PORT_PROFILE].HintDescriptor=LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_0;
    ph[PORT_PROFILE].LowerBound=0.0f;ph[PORT_PROFILE].UpperBound=(float)CHAIN_PROFILE_COUNT;
    memset(&desc,0,sizeof(desc));desc.UniqueID=0x53503157;desc.Label="sp11_dolby_windows_chain";desc.Name="SP11 Exact Windows Dolby VLLDP+VR";
    desc.Maker="sp11 re project";desc.Copyright="research bridge; original Dolby DLLs supplied separately";
    desc.PortCount=PORT_COUNT;desc.PortDescriptors=pd;desc.PortNames=pn;desc.PortRangeHints=ph;
    desc.instantiate=chain_instantiate;desc.connect_port=chain_connect;desc.activate=chain_activate;desc.run=chain_run;desc.cleanup=chain_cleanup;
    return &desc;
}
