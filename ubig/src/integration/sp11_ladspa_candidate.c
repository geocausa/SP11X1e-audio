/*
 * UbiG SP11 native candidate bridge.
 *
 * Source-owned SP11 Stage-A + Stage-B deployment candidate. No Windows PE image is
 * loaded or executed; recovered endpoint-owned calibration/state bytes are
 * supplied separately by the corrected v3 private proof pack. The realtime callback performs
 * no dynamic allocation and slices arbitrary host buffers into fixed storage.
 */
#define _GNU_SOURCE
#include <ladspa.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ubig/ubig.h"
#include "ubig/ubig_control.h"
#include "stage_b_rt.h"
#include "stage_b_leveler.h"


#define RT_CHUNK_FRAMES 4096U
typedef struct { float *pBuffer; uint32_t frames; uint32_t flags; } Conn;
static uint64_t q(void*p,size_t o){uint64_t v;memcpy(&v,(char*)p+o,8);return v;}
static uint32_t d(void*p,size_t o){uint32_t v;memcpy(&v,(char*)p+o,4);return v;}
static void wd(void*p,size_t o,uint32_t v){memcpy((char*)p+o,&v,4);}
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
} ChainProfileCfg;

static const int32_t vr_centers[20]={47,141,234,328,469,656,844,1031,1313,1688,2250,3000,3750,4688,5813,7125,9000,11250,13875,19688};
static const ChainProfileCfg chain_profiles[CHAIN_PROFILE_COUNT]={
    [CHAIN_PROFILE_DYNAMIC]={"dynamic",1,5,1,5,1,10,1,96,1,10,10,10,96,11,0},
    [CHAIN_PROFILE_MOVIE]={"movie",1,0,1,2,0,6,0,72,1,16,10,16,104,11,1},
    [CHAIN_PROFILE_MUSIC]={"music",1,0,0,5,0,6,0,24,0,10,10,10,96,1,1},
    [CHAIN_PROFILE_GAME]={"game",1,0,0,7,0,10,0,0,1,10,10,10,96,11,0},
    [CHAIN_PROFILE_VOICE]={"voice",0,0,1,8,0,10,0,0,0,10,10,10,96,1,0},
    [CHAIN_PROFILE_ONLINECOURSE]={"onlinecourse",1,0,1,5,0,10,0,0,0,10,10,10,64,1,0},
    [CHAIN_PROFILE_PERSONALIZE]={"personalize",1,3,1,10,0,10,0,48,1,10,10,10,96,11,0},
};

typedef struct {
    LADSPA_Data *ports[PORT_COUNT];
    int ready;
    ubig_engine *native_stage_a;
    float *native_stage_a_l,*native_stage_a_r;
    ChainProfile profile;
    int last_profile_request;
    ubig_control_handle control;
    int control_ready;
    int32_t custom_eq[UBIG_EQ_BANDS];
    int custom_eq_valid;


    uint8_t *vr_inner;
    uint8_t *vr_arena;
    uint8_t vr_cfg[0x60] __attribute__((aligned(16)));
    int vr_initialized;

    float *buf_a,*buf_b;
} ChainInst;

static ChainProfile chain_profile_from_env(void){
    const char *v=getenv("UBIG_PROFILE"); if(!v||!*v)v=getenv("SP11_DOLBY_PROFILE");
    if(!v || !*v || !strcasecmp(v,"dynamic")) return CHAIN_PROFILE_DYNAMIC;
    if(!strcasecmp(v,"movie")) return CHAIN_PROFILE_MOVIE;
    if(!strcasecmp(v,"music")) return CHAIN_PROFILE_MUSIC;
    if(!strcasecmp(v,"game") || !strcasecmp(v,"gaming")) return CHAIN_PROFILE_GAME;
    if(!strcasecmp(v,"voice")) return CHAIN_PROFILE_VOICE;
    if(!strcasecmp(v,"onlinecourse") || !strcasecmp(v,"online-course") || !strcasecmp(v,"course")) return CHAIN_PROFILE_ONLINECOURSE;
    if(!strcasecmp(v,"personalize") || !strcasecmp(v,"personalized") || !strncasecmp(v,"custom",6)) return CHAIN_PROFILE_PERSONALIZE;
    fprintf(stderr,"ubig-sp11: unknown profile '%s'; using Dynamic\n",v);
    return CHAIN_PROFILE_DYNAMIC;
}

typedef void (*VrScalarHandlerFn)(void*,int);
static float vr_scalar_f32(void *core,size_t off){float v;memcpy(&v,(uint8_t*)core+off,4);return v;}
static void vr_scalar_wf32(void *core,size_t off,float v){memcpy((uint8_t*)core+off,&v,4);}
static float vr_scalar_mul(float a,float b){float r;__asm__("fmul %s0,%s1,%s2":"=w"(r):"w"(a),"w"(b));return r;}
static int vr_clip_i(int v,int lo,int hi){return v<lo?lo:(v>hi?hi:v);}
static void vr_set_u32_dirty(void *core,size_t off,uint32_t v,size_t dirty1,size_t dirty2){
    uint32_t old=d(core,off);if(old==v)return;wd(core,off,v);if(dirty1)wd(core,dirty1,1u);if(dirty2)wd(core,dirty2,1u);
}
static void vr_set_f32_dirty(void *core,size_t off,float v,size_t dirty1,size_t dirty2){
    float old=vr_scalar_f32(core,off);if(old==v)return;vr_scalar_wf32(core,off,v);if(dirty1)wd(core,dirty1,1u);if(dirty2)wd(core,dirty2,1u);
}
static void vr_scalar_native(void *core,uint64_t va,int value){
    if(!core)return;
    switch(va){
    case VR_H_LEVELER_ENABLE: vr_set_u32_dirty(core,0x6dc,(uint32_t)(value!=0),0x1278,0);break;
    case VR_H_LEVELER_AMOUNT: vr_set_u32_dirty(core,0x6d4,(uint32_t)vr_clip_i(value,0,10),0x1278,0);break;
    case VR_H_LEVELER_IN:{int v=vr_clip_i(value,-640,0);float f=vr_scalar_mul(vr_scalar_mul((float)v,0x1p-15f),0x1.f81f82p-1f);f=vr_scalar_mul(f,16.0f);vr_set_f32_dirty(core,0x64c,f,0x610,0x1278);break;}
    case VR_H_LEVELER_OUT:{int v=vr_clip_i(value,-640,0);float f=vr_scalar_mul(vr_scalar_mul((float)v,0x1p-15f),0x1.f81f82p-1f);f=vr_scalar_mul(f,16.0f);vr_set_f32_dirty(core,0x644,f,0x610,0x1278);break;}
    case VR_H_LEVELER_DRC: vr_set_u32_dirty(core,0x6e4,(uint32_t)(value!=0),0x1278,0);break;
    case VR_H_REG_ENABLE: vr_set_u32_dirty(core,0xe00,(uint32_t)(value!=0),0xdcc,0x1278);break;
    case VR_H_REG_SPKDIST: vr_set_u32_dirty(core,0xdf8,(uint32_t)(value!=0),0xdcc,0x1278);break;
    case VR_H_REG_OVERDRIVE: vr_set_u32_dirty(core,0xde4,(uint32_t)vr_clip_i(value,0,192),0xdcc,0x1278);break;
    case VR_H_REG_RELAX: vr_set_u32_dirty(core,0xdf0,(uint32_t)vr_clip_i(value,0,144),0xdcc,0x1278);break;
    case VR_H_REG_TIMBRE: vr_set_u32_dirty(core,0xdd8,(uint32_t)vr_clip_i(value,0,16),0xdcc,0x1278);break;
    case VR_H_DIALOG_ENABLE: vr_set_u32_dirty(core,0x6ac,(uint32_t)value,0x1278,0);break;
    case VR_H_DIALOG_AMOUNT: vr_set_u32_dirty(core,0x6b4,(uint32_t)vr_clip_i(value,0,16),0x1278,0);break;
    case VR_H_DIALOG_DUCK:{int v=vr_clip_i(value,0,16);float f=vr_scalar_mul(vr_scalar_mul((float)v,0x1p-15f),0x1p+11f);vr_set_f32_dirty(core,0x6bc,f,0x1278,0);break;}
    case VR_H_IEQ_ENABLE: vr_set_u32_dirty(core,0x6ec,(uint32_t)(value!=0),0x1278,0);break;
    case VR_H_IEQ_AMOUNT:{int v=vr_clip_i(value,0,16);vr_set_f32_dirty(core,0x6f4,vr_scalar_mul((float)v,0x1p-4f),0x1278,0);break;}
    case VR_H_GEQ_ENABLE: vr_set_u32_dirty(core,0xa4c,(uint32_t)(value!=0),0xaa0,0x1278);break;
    case VR_H_MI_DIALOG: vr_set_u32_dirty(core,0x678,(uint32_t)value,0x1278,0);break;
    case VR_H_MI_LEVELER: vr_set_u32_dirty(core,0x670,(uint32_t)value,0x1278,0);break;
    case VR_H_MI_IEQ: vr_set_u32_dirty(core,0x668,(uint32_t)value,0x1278,0);break;
    case VR_H_MI_SURR_COMP: vr_set_u32_dirty(core,0x680,(uint32_t)value,0x1278,0);break;
    case VR_H_MI_VIRT: vr_set_u32_dirty(core,0x688,(uint32_t)value,0x1278,0);break;
    case VR_H_SURROUND_BOOST:{int v=vr_clip_i(value,0,96);vr_set_f32_dirty(core,0x6c4,vr_scalar_mul((float)v,0x1.f81f82p-12f),0x1278,0);break;}
    case VR_H_SURROUND_DEC: vr_set_u32_dirty(core,0x698,(uint32_t)(value!=0),0x1278,0);break;
    case VR_H_VIRT_FRONT:{uint32_t v=(uint32_t)value;if(v==0u)v=1u;if(v>30u)v=30u;vr_set_u32_dirty(core,0x994,v,0x9bc,0x1278);break;}
    case VR_H_VIRT_HEIGHT:{uint32_t v=(uint32_t)value;if(v==0u)v=1u;if(v>30u)v=30u;vr_set_u32_dirty(core,0x9a4,v,0x9bc,0x1278);break;}
    case VR_H_VIRT_SURROUND:{uint32_t v=(uint32_t)value;if(v==0u)v=1u;if(v>30u)v=30u;vr_set_u32_dirty(core,0x99c,v,0x9bc,0x1278);break;}
    case VR_H_VOLMAX_BOOST:{int v=vr_clip_i(value,0,192);float f=vr_scalar_mul(vr_scalar_mul((float)v,0x1p-15f),0x1.f81f82p-1f);f=vr_scalar_mul(f,16.0f);vr_set_f32_dirty(core,0x634,f,0x610,0x1278);break;}
    default: fprintf(stderr,"unknown native VR scalar handler %llx\n",(unsigned long long)va);abort();
    }
}
static unsigned long g_cvrscalar_native;
static void vr_scalar(ChainInst *p,void *core,uint64_t va,int value){(void)p;++g_cvrscalar_native;vr_scalar_native(core,va,value);}
typedef void (*VrOutputModeFn)(void*,uint32_t,uint32_t,const int32_t*);
typedef int (*VrBandGridFn)(void*,void*,uint32_t,const int32_t*,uint32_t);
typedef int (*VrBandTargetFn)(void*,void*,const int32_t*,int32_t,int32_t);
typedef void (*VrRegTuneFn)(void*,uint32_t,const int32_t*,const int32_t*,const int32_t*,const int32_t*);
#define VR_OUTPUT_MODE_VA      0x180032320ULL
#define VR_BAND_GRID_VA        0x18004C560ULL
#define VR_BAND_TARGET_VA      0x18004C8E8ULL
#define VR_REG_TUNING_VA       0x1800463C0ULL
static int vr_parse_geq(int32_t target[20]){
    const char *v=getenv("UBIG_GEQ"); if(!v||!*v)v=getenv("SP11_DOLBY_GEQ");
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

static int vr_apply_geq_values(ChainInst *p,void *core,const int32_t target[UBIG_EQ_BANDS]){
    uint8_t *raw=(uint8_t*)core+0xaa4;
    UbigStageBRtBandControlMap map;
    for(uint32_t i=0;i<UBIG_EQ_BANDS;i++){
        memcpy(&map.weight[i],raw+4u*i,4);
        map.lower_index[i]=d(raw,0xa0u+4u*i);
        map.control_frequency[i]=(int32_t)d(raw,0x148u+4u*i);
    }
    map.output_count=d(raw,0x140);map.control_count=d(raw,0x144);
    uint32_t gr=ubig_stage_b_rt_band_control_map_prepare(&map,vr_centers,UBIG_EQ_BANDS,
                                                         vr_centers,UBIG_EQ_BANDS);
    if(gr==2u)return -3;
    for(uint32_t i=0;i<UBIG_EQ_BANDS;i++){
        memcpy(raw+4u*i,&map.weight[i],4);
        wd(raw,0xa0u+4u*i,map.lower_index[i]);
        wd(raw,0x148u+4u*i,(uint32_t)map.control_frequency[i]);
    }
    wd(raw,0x140,map.output_count);wd(raw,0x144,map.control_count);
    uint32_t tr=ubig_stage_b_rt_band_target_apply(&map,(int32_t*)((uint8_t*)core+0xa50),
                                                  target,-576,576);
    if(tr)wd(core,0xaa0,1u);
    if(d(core,0xaa0))wd(core,0x1278,1u);
    vr_scalar(p,core,VR_H_GEQ_ENABLE,1);
    return 0;
}

static int vr_apply_geq(ChainInst *p,void *core){
    int32_t target[UBIG_EQ_BANDS];
    if(p->custom_eq_valid)return vr_apply_geq_values(p,core,p->custom_eq);
    int have=vr_parse_geq(target);
    if(have<0){fprintf(stderr,"ubig-sp11: invalid Custom EQ; GEQ disabled\n");vr_scalar(p,core,VR_H_GEQ_ENABLE,0);return 0;}
    if(!have){vr_scalar(p,core,VR_H_GEQ_ENABLE,0);return 0;}
    memcpy(p->custom_eq,target,sizeof p->custom_eq);p->custom_eq_valid=1;
    if(p->native_stage_a)ubig_engine_set_custom_eq(p->native_stage_a,p->custom_eq);
    return vr_apply_geq_values(p,core,p->custom_eq);
}

static unsigned long g_cvrcomplex_noop;
static int vr_apply_profile_complex(ChainInst *p,void *core,const ChainProfileCfg *pc){(void)p;(void)core;(void)pc;++g_cvrcomplex_noop;return 0;}

static int vr_apply_profile(ChainInst *p,uint8_t *inner){
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

    return 0;
}

static int chain_apply_profile_inplace(ChainInst *p,ChainProfile next){
    if(!p || !p->ready || next<0 || next>=CHAIN_PROFILE_COUNT)return -1;
    ChainProfile old=p->profile; if(old==next)return 0;
    uint8_t *vr_inner=p->vr_inner;
    void *vr_core=vr_inner?(void*)(uintptr_t)q(vr_inner,0x130):NULL;
    if(!vr_core || !p->native_stage_a)return -2;
    if(vr_retarget_profile(p,vr_core,old,next))return -3;
    if(ubig_engine_set_profile(p->native_stage_a,(ubig_profile)next)!=UBIG_OK)return -4;
    p->profile=next;
    return 0;
}

static int chain_profile_code_from_port(const LADSPA_Data *v){
    if(!v || *v!=*v)return -1;
    int code=(int)(*v+.5f);
    return code>=1 && code<=CHAIN_PROFILE_COUNT?code-1:-1;
}

static int chain_control_open(ChainInst *p){
    const char *override=getenv("UBIG_CONTROL_PATH");
    if(override&&*override&&(!strcasecmp(override,"off")||!strcasecmp(override,"none")||!strcmp(override,"0")))return 1;
    int rc=ubig_control_open(&p->control,override,1);if(rc)return rc;
    p->control_ready=1;
    __atomic_store_n(&p->control.page->active_profile,(uint32_t)p->profile,__ATOMIC_RELEASE);
    return 0;
}

static void chain_control_ack(ChainInst *p,uint32_t generation,int error){
    if(!p->control_ready||!p->control.page)return;
    __atomic_store_n(&p->control.page->active_profile,(uint32_t)p->profile,__ATOMIC_RELAXED);
    __atomic_store_n(&p->control.page->last_error,error,__ATOMIC_RELAXED);
    __atomic_store_n(&p->control.page->ack_generation,generation,__ATOMIC_RELEASE);
}

static int chain_apply_control_request(ChainInst *p){
    if(!p->control_ready)return 0;
    ubig_control_page request;int rc=ubig_control_snapshot(&p->control,&request);if(rc)return rc;
    if(request.request_generation==request.ack_generation)return 0;
    const uint32_t generation=request.request_generation;
    if(request.desired_profile>=UBIG_PROFILE_COUNT){chain_control_ack(p,generation,UBIG_EINVAL);return UBIG_EINVAL;}
    const ChainProfile next=(ChainProfile)request.desired_profile;
    const int has_eq=(request.desired_flags&UBIG_CONTROL_FLAG_CUSTOM_EQ_VALID)!=0u;
    if(has_eq){
        for(unsigned i=0;i<UBIG_EQ_BANDS;i++)if(request.custom_eq[i]<-192||request.custom_eq[i]>192){chain_control_ack(p,generation,UBIG_EINVAL);return UBIG_EINVAL;}
        memcpy(p->custom_eq,request.custom_eq,sizeof p->custom_eq);p->custom_eq_valid=1;
        if(p->native_stage_a&&ubig_engine_set_custom_eq(p->native_stage_a,p->custom_eq)!=UBIG_OK){chain_control_ack(p,generation,UBIG_ESTATE);return UBIG_ESTATE;}
    }
    if(next!=(ChainProfile)p->profile)rc=chain_apply_profile_inplace(p,next);
    else if(next==CHAIN_PROFILE_PERSONALIZE&&has_eq){
        void *core=p->vr_inner?(void*)(uintptr_t)q(p->vr_inner,0x130):NULL;
        rc=core?vr_apply_geq_values(p,core,p->custom_eq):UBIG_ESTATE;
    }else rc=0;
    chain_control_ack(p,generation,rc);return rc;
}


__attribute__((visibility("hidden"),used)) unsigned long g_c60200,g_c596e0,g_c54a48,g_c5f5a8;
__attribute__((visibility("hidden"),used)) unsigned long g_c7b2f0,g_c7bnative,g_c7bfallback,g_c9cbnative,g_c9cbfallback,g_c8cnative,g_c8cfallback,g_cuppernative,g_cloweranative,g_clowerbnative,g_c584native,g_c584fallback;
static const float *g_projection_lut;
static float g_owned_projection_lut[UBIG_STAGE_B_RT_PROJECTION_LUT] __attribute__((aligned(16)));
static unsigned long g_c56b80,g_c56fallback;
static unsigned long g_c602fallback,g_c5ffallback,g_c558_native,g_c4bab_native;
static void sib60200(float a,float b,uint32_t *main_raw,void *extra_raw,void *unused,uint32_t *bounds,int32_t *map,uint32_t *out_raw,void *tele_raw){
    ++g_c60200;(void)unused;if(!main_raw||!bounds||!map||!out_raw||!tele_raw||extra_raw||main_raw[0]!=2u||main_raw[1]!=4u||out_raw[0]!=2u||out_raw[1]!=20u||out_raw[5]!=20u){++g_c602fallback;return;}float ***groups=0;float **rows=0;int32_t **tele_rows=0;memcpy(&groups,(char*)main_raw+16,8);memcpy(&rows,(char*)out_raw+8,8);memcpy(&tele_rows,(char*)tele_raw+8,8);if(!groups||!rows||!tele_rows){++g_c602fallback;return;}UbigStageBRtComplexGroups main={main_raw[0],main_raw[1],groups};UbigStageBRtBandRows out={out_raw[0],out_raw[1],rows,out_raw[5]};UbigStageBRtTelemetryRows tele={tele_rows};ubig_stage_b_rt_band_log_process(a,b,&main,NULL,bounds,map,&out,&tele);
}
__attribute__((visibility("hidden"),used)) unsigned long g_c4a570,g_c5bc98,g_c5c6d0,g_c45288,g_c5ad38;
static void sib4a570_native(void *raw_cfg,void *raw_coeff,uint32_t history_row,float *output,float *tail){
    ++g_c4a570;
    if(!raw_cfg||!raw_coeff||!output||!tail)abort();
    UbigStageBRtHistoryTransform32 state;
    memset(&state,0,sizeof state);
    memcpy(&state.history_rows,(const uint8_t*)raw_cfg+0,8);
    memcpy(&state.primary,(const uint8_t*)raw_coeff+0,8);
    memcpy(&state.secondary,(const uint8_t*)raw_coeff+8,8);
    memcpy(&state.count,(const uint8_t*)raw_coeff+16,4);
    memcpy(&state.phase,(const uint8_t*)raw_coeff+20,4);
    if(!state.history_rows||!state.primary||!state.secondary||state.count==0u)abort();
    ubig_stage_b_rt_history_transform32(&state,history_row,output,tail);
}
static __attribute__((unused)) void sib5ad38_noop(void *input_v,void *output_v){
    ++g_c5ad38;
    const uint8_t *input=(const uint8_t*)input_v,*output=(const uint8_t*)output_v;
    uint32_t in_rows=0,in_width=0,out_type=0;uint64_t out_stride=0;
    if(!input||!output)abort();
    memcpy(&in_rows,input+0,4);memcpy(&in_width,input+4,4);
    memcpy(&out_stride,output+8,8);memcpy(&out_type,output+16,4);
    if(in_rows!=0u||in_width!=256u||out_type!=7u||out_stride!=2u)abort();
}
typedef void (*Sib5fFn)(float,float,void*,void*,uint32_t*,uint32_t*,uint32_t*,void*);
static void sib5f5a8(float a,float b,void *in_v,void *out_v,uint32_t *map,uint32_t *bounds,uint32_t *obj,void *extra){
    ++g_c5f5a8;uint32_t *in=(uint32_t*)in_v,*out=(uint32_t*)out_v;if(!in||!out||!map||!bounds||!obj||extra||in[0]!=2u||in[1]!=20u||out[0]!=2u||out[1]!=20u||obj[0]!=2u||obj[2]!=77u){++g_c5ffallback;return;}float **in_rows=0,**out_rows=0;float ***raw_objects=0;memcpy(&in_rows,(char*)in+8,8);memcpy(&out_rows,(char*)out+8,8);memcpy(&raw_objects,(char*)obj+16,8);if(!in_rows||!out_rows||!raw_objects){++g_c5ffallback;return;}UbigStageBRtTargetObject objects[2];for(unsigned r=0;r<2u;r++){if(!raw_objects[r]){++g_c5ffallback;return;}for(unsigned z=0;z<4u;z++)objects[r].plane[z]=raw_objects[r][z];}UbigStageBRtBandRows ui={2u,20u,in_rows,20u},uo={2u,20u,out_rows,20u};UbigStageBRtTargetSet targets={2u,77u,objects};ubig_stage_b_rt_output_shape(a,b,&ui,&uo,map,bounds,&targets);
}
static __attribute__((unused)) float sib558_native(uint32_t *desc,float *weights,void *p3,void *p4,int mode){++g_c558_native;if(weights==NULL&&p4==NULL&&mode==0){(void)desc;(void)p3;return 0.0f;}abort();}
static __attribute__((unused)) uint32_t sib4bab_native(uint32_t *matrix,void *plan,void *work){++g_c4bab_native;(void)matrix;(void)plan;(void)work;return 0u;}

static uint32_t rr32(const void*p,size_t o){uint32_t v;memcpy(&v,(const char*)p+o,4);return v;}
static float rrf(const void*p,size_t o){float v;memcpy(&v,(const char*)p+o,4);return v;}
static void ww32(void*p,size_t o,uint32_t v){memcpy((char*)p+o,&v,4);}
static void wwf(void*p,size_t o,float v){memcpy((char*)p+o,&v,4);}
static void rawagg_to_native(const uint8_t*r,UbigStageBRtControlAggregateState*x){
    memset(x,0,sizeof *x);x->enabled=rr32(r,0);x->slot2_state=rrf(r,40);x->hysteresis.response_a=rrf(r,44);x->hysteresis.response_b=rrf(r,48);x->hysteresis.response_c=rrf(r,52);x->hysteresis.input=rrf(r,56);x->smoothing_keep=rrf(r,60);x->activity_alpha_low=rrf(r,64);x->activity_alpha_high=rrf(r,68);x->activity_state=rrf(r,72);x->hysteresis.countdown_scale=rrf(r,76);x->hysteresis.countdown_bias=rrf(r,80);x->hysteresis.smoothed_input=rrf(r,84);x->hysteresis.toggle_keep=rrf(r,88);x->hysteresis.toggle_state=rrf(r,92);x->hysteresis.countdown=(int32_t)rr32(r,96);x->hysteresis.toggle=rr32(r,100);x->final_blend=rrf(r,104);x->final_state=rrf(r,108);
}
static void nativeagg_to_raw(uint8_t*r,const UbigStageBRtControlAggregateState*x){
    ww32(r,0,x->enabled);wwf(r,40,x->slot2_state);wwf(r,44,x->hysteresis.response_a);wwf(r,48,x->hysteresis.response_b);wwf(r,52,x->hysteresis.response_c);wwf(r,56,x->hysteresis.input);wwf(r,60,x->smoothing_keep);wwf(r,64,x->activity_alpha_low);wwf(r,68,x->activity_alpha_high);wwf(r,72,x->activity_state);wwf(r,76,x->hysteresis.countdown_scale);wwf(r,80,x->hysteresis.countdown_bias);wwf(r,84,x->hysteresis.smoothed_input);wwf(r,88,x->hysteresis.toggle_keep);wwf(r,92,x->hysteresis.toggle_state);ww32(r,96,(uint32_t)x->hysteresis.countdown);ww32(r,100,x->hysteresis.toggle);wwf(r,104,x->final_blend);wwf(r,108,x->final_state);
}
static void result_to_item(const uint8_t*out,UbigStageBRtControlAggregateItem*x){
    memset(x,0,sizeof *x);x->winner=rr32(out,0);x->slot1_transfer=rrf(out,12);x->slot2_transfer=rrf(out,20);x->slot5_transfer=rrf(out,44);x->slot6_transfer=rrf(out,52);x->secondary_transfer=rrf(out,108);
}
typedef void (*Fn9CB)(void*,void*,void*);
typedef void (*Fn8C)(void*,void*,void*);
static void *rrp(const void*p,size_t o){void*v=0;memcpy(&v,(const char*)p+o,8);return v;}
static __attribute__((unused)) void wwp(void*p,size_t o,const void*v){memcpy((char*)p+o,&v,8);}
static int build_control_cfg(uint8_t *rc,UbigStageBRtControlCadenceConfig *cfg,UbigStageBRtControlDescriptor descs[5]){
    if(!rc||!cfg||!descs)return -1;
    for(unsigned g=0;g<4u;g++){
        uint8_t *rg=rc+16u*g;uint8_t *rd=(uint8_t*)rrp(rg,8);if(!rd)return -2;
        descs[g].term_count=rr32(rd,0);descs[g].transfer_gain=rrf(rd,4);descs[g].transfer_bias=rrf(rd,8);descs[g].terms=(const UbigStageBRtControlTerm*)(const void*)(rd+12);
        cfg->groups[g].output_index=rr32(rg,0);cfg->groups[g].descriptor=&descs[g];
    }
    uint8_t *rd=(uint8_t*)rrp(rc,72);if(!rd)return -3;
    descs[4].term_count=rr32(rd,0);descs[4].transfer_gain=rrf(rd,4);descs[4].transfer_bias=rrf(rd,8);descs[4].terms=(const UbigStageBRtControlTerm*)(const void*)(rd+12);cfg->secondary=&descs[4];
    return 0;
}
typedef struct { float *bins; uint32_t count; int32_t exponent; float aggregate; } Raw9Export;
static int sib9cbnative(void *raw_v,void *desc_v,void *export_v){
    ++g_c9cbnative;uint8_t *raw=(uint8_t*)raw_v,*desc=(uint8_t*)desc_v;Raw9Export *out=(Raw9Export*)export_v;
    if(!raw||!desc||!out||!out->bins){++g_c9cbfallback;return -1;}
    const uint32_t row_count=rr32(desc,0);void ***lists=(void***)rrp(desc,16);
    if(row_count!=2u||!lists||!lists[0]||!lists[1]){++g_c9cbfallback;return -2;}
    float *row0=0,*row1=0;memcpy(&row0,lists[0],8);memcpy(&row1,lists[1],8);
    if(!row0||!row1){++g_c9cbfallback;return -3;}
    UbigStageBRtSpectralAccumulator st;memset(&st,0,sizeof st);
    st.period=rr32(raw,4);st.counter=rr32(raw,12);st.exponent_offset=(int32_t)rr32(raw,20);st.output_scale=rrf(raw,24);
    memcpy(st.energy,raw+0x1c,sizeof st.energy);memcpy(st.shift,raw+0x150,sizeof st.shift);st.global_shift=(int32_t)rr32(raw,0x284);
    UbigStageBRtSpectralExport ex;memcpy(ex.bins,out->bins,sizeof ex.bins);ex.count=out->count;ex.exponent=out->exponent;ex.aggregate=out->aggregate;
    ubig_stage_b_rt_spectral_accumulate(&st,row0,row1,&ex);
    ww32(raw,12,st.counter);memcpy(raw+0x1c,st.energy,sizeof st.energy);memcpy(raw+0x150,st.shift,sizeof st.shift);ww32(raw,0x284,(uint32_t)st.global_shift);
    memcpy(out->bins,ex.bins,sizeof ex.bins);out->count=ex.count;out->exponent=ex.exponent;out->aggregate=ex.aggregate;return 0;
}
typedef void (*Fn2)(void*,void*);
typedef void (*Fn3)(void*,void*,void*);
typedef float (*FnMean)(void*);
typedef void (*FnRank)(float,void*,void*,void*);
static void raw_export_sem(const Raw9Export *r,UbigStageBRtSpectralExport *s){
    memset(s,0,sizeof *s);if(!r||!r->bins)return;memcpy(s->bins,r->bins,sizeof s->bins);s->count=r->count;s->exponent=r->exponent;s->aggregate=r->aggregate;
}
static int upper_feature_history(uint8_t *raw,const UbigStageBRtSpectralExport *in){
    if(!raw||!in)return -1;
    UbigStageBRtFeatureHistory s;memset(&s,0,sizeof s);memcpy(s.records,raw,sizeof s.records);s.index=rr32(raw,0xa0c);s.phase=rr32(raw,0xa14);memcpy(s.segment_sum,raw+0xa18,sizeof s.segment_sum);memcpy(s.delta_sum,raw+0xa38,sizeof s.delta_sum);memcpy(s.segment_shift,raw+0xa58,sizeof s.segment_shift);memcpy(s.delta_shift,raw+0xa78,sizeof s.delta_shift);UbigStageBRtFeatureHistoryConfig c={0};c.boundaries=(const uint32_t*)rrp(raw,0xa00);c.scaled_sum_count=rr32(raw,0xa08);if(!c.boundaries)return -2;ubig_stage_b_rt_feature_history_process(&s,&c,in);memcpy(raw,s.records,sizeof s.records);ww32(raw,0xa0c,s.index);ww32(raw,0xa14,s.phase);memcpy(raw+0xa18,s.segment_sum,sizeof s.segment_sum);memcpy(raw+0xa38,s.delta_sum,sizeof s.delta_sum);memcpy(raw+0xa58,s.segment_shift,sizeof s.segment_shift);memcpy(raw+0xa78,s.delta_shift,sizeof s.delta_shift);return 0;
}
static int upper_segment_ratio(uint8_t *raw,const UbigStageBRtSpectralExport *in){
    if(!raw||!in)return -1;
    UbigStageBRtSegmentRatioHistory s;memcpy(s.history,raw,sizeof s.history);s.index=rr32(raw,0x408);UbigStageBRtSegmentRatioConfig c={(const uint32_t*)rrp(raw,0x400)};if(!c.boundaries)return -2;ubig_stage_b_rt_segment_ratio_process(&s,&c,in);memcpy(raw,s.history,sizeof s.history);ww32(raw,0x408,s.index);return 0;
}
static int upper_variation(uint8_t *raw,const UbigStageBRtSpectralExport *in){
    if(!raw||!in)return -1;
    UbigStageBRtVariationHistory s;memcpy(s.history,raw,sizeof s.history);s.index=rr32(raw,0x414);UbigStageBRtVariationConfig c={rr32(raw,0x410),(const uint32_t*)rrp(raw,0x400),(const float*)rrp(raw,0x408)};if(c.segment_count&&!c.boundaries)return -2;ubig_stage_b_rt_variation_history_process(&s,&c,in->bins,in->count);memcpy(raw,s.history,sizeof s.history);ww32(raw,0x414,s.index);return 0;
}
static int upper_spectral_change(uint8_t *raw,const UbigStageBRtSpectralExport *in){
    if(!raw||!in)return -1;
    UbigStageBRtSpectralChangeHistory s;memcpy(s.history,raw,sizeof s.history);memcpy(s.previous_bins,raw+0x80,sizeof s.previous_bins);s.previous_aggregate=rrf(raw,0x1b4);s.previous_exponent=(int32_t)rr32(raw,0x1b8);s.index=rr32(raw,0x1bc);ubig_stage_b_rt_spectral_change_process(&s,in);memcpy(raw,s.history,sizeof s.history);memcpy(raw+0x80,s.previous_bins,sizeof s.previous_bins);wwf(raw,0x1b4,s.previous_aggregate);ww32(raw,0x1b8,(uint32_t)s.previous_exponent);ww32(raw,0x1bc,s.index);return 0;
}
static int upper_projection(uint8_t *raw,const UbigStageBRtSpectralExport *in){
    if(!raw||!in||!g_projection_lut)return -1;
    UbigStageBRtProjectionHistory s;memset(&s,0,sizeof s);memcpy(s.records,raw,sizeof s.records);s.index=rr32(raw,0x530);s.phase=rr32(raw,0x538);memcpy(s.sum,raw+0x53c,sizeof s.sum);memcpy(s.delta_sum,raw+0x55c,sizeof s.delta_sum);memcpy(s.shift,raw+0x578,sizeof s.shift);memcpy(s.delta_shift,raw+0x598,sizeof s.delta_shift);UbigStageBRtProjectionConfig c;memset(&c,0,sizeof c);for(uint32_t i=0;i<UBIG_STAGE_B_RT_PROJECTION_MEASUREMENTS;i++){uint8_t*d=raw+0x400+16u*i;c.bands[i].start=rr32(d,0);c.bands[i].count=rr32(d,4);c.bands[i].weights=(const float*)rrp(d,8);if(c.bands[i].count&&!c.bands[i].weights)return -2;}c.projection_lut=g_projection_lut;ubig_stage_b_rt_projection_history_process(&s,&c,in);memcpy(raw,s.records,sizeof s.records);ww32(raw,0x530,s.index);ww32(raw,0x538,s.phase);memcpy(raw+0x53c,s.sum,sizeof s.sum);memcpy(raw+0x55c,s.delta_sum,sizeof s.delta_sum);memcpy(raw+0x578,s.shift,sizeof s.shift);memcpy(raw+0x598,s.delta_shift,sizeof s.delta_shift);return 0;
}
static int upper_feature_change(uint8_t *raw,const float *in,float *normalized){
    if(!raw||!in||!normalized)return -1;
    UbigStageBRtFeatureChangeHistory s;memcpy(s.history,raw,sizeof s.history);memcpy(s.previous,raw+0x80,sizeof s.previous);s.index=rr32(raw,0xa0);ubig_stage_b_rt_feature_change_process(&s,in,normalized);memcpy(raw,s.history,sizeof s.history);memcpy(raw+0x80,s.previous,sizeof s.previous);ww32(raw,0xa0,s.index);return 0;
}
static int upper_peak(uint8_t *raw,const UbigStageBRtSpectralExport *in,float *scratch){
    if(!raw||!in||!scratch)return -1;
    UbigStageBRtPeakResidualHistory s;memcpy(s.history,raw,sizeof s.history);s.index=rr32(raw,0x180);ubig_stage_b_rt_peak_residual_process(&s,in,scratch);memcpy(raw,s.history,sizeof s.history);ww32(raw,0x180,s.index);return 0;
}
static int lower_feature_cadence(uint8_t *raw,float *out){
    if(!raw||!out)return -1;
    UbigStageBRtFeatureHistory s;memset(&s,0,sizeof s);memcpy(s.records,raw,sizeof s.records);s.index=rr32(raw,0xa0c);s.phase=rr32(raw,0xa14);memcpy(s.segment_sum,raw+0xa18,sizeof s.segment_sum);memcpy(s.delta_sum,raw+0xa38,sizeof s.delta_sum);memcpy(s.segment_shift,raw+0xa58,sizeof s.segment_shift);memcpy(s.delta_shift,raw+0xa78,sizeof s.delta_shift);ubig_stage_b_rt_feature_cadence_process(&s,rr32(raw,0xa10),out);ww32(raw,0xa14,s.phase);return 0;
}
static int lower_ratio_stats(uint8_t *raw,float *out,float *scratch){
    if(!raw||!out||!scratch)return -1;
    UbigStageBRtStatCursor c={rr32(raw,0x40c),rr32(raw,0x410)};ubig_stage_b_rt_stat32_ring_columns(&c,(const float(*)[8])raw,rr32(raw,0x408),scratch,out,out+8);ww32(raw,0x410,c.index);return 0;
}
static int lower_variation_stats(uint8_t *raw,float *out,float *scratch){
    if(!raw||!out||!scratch)return -1;
    UbigStageBRtStatCursor c={rr32(raw,0x418),rr32(raw,0x41c)};uint32_t n=rr32(raw,0x410);if(n>8u)return -2;ubig_stage_b_rt_stat32_columns(&c,(const float(*)[8])raw,n,scratch,out,out+8);ww32(raw,0x41c,c.index);return 0;
}
static int lower_stat_step(uint8_t *raw,size_t step_off,size_t index_off,float *out,float *scratch){
    if(!raw||!out||!scratch)return -1;
    UbigStageBRtStatCursor c={rr32(raw,step_off),rr32(raw,index_off)};ubig_stage_b_rt_stat32_step(&c,(const float*)raw,scratch,out);ww32(raw,index_off,c.index);return 0;
}
static int lower_projection_cadence(uint8_t *raw,float *out,float *scratch){
    if(!raw||!out||!scratch)return -1;
    UbigStageBRtCadenceSummary c;memcpy(c.matrix,raw,sizeof c.matrix);c.cursor.step=rr32(raw,0x534);c.cursor.index=rr32(raw,0x538);memcpy(c.column_accumulator,raw+0x53c,sizeof c.column_accumulator);memcpy(c.delta_accumulator,raw+0x55c,sizeof c.delta_accumulator);memcpy(c.column_shift,raw+0x578,sizeof c.column_shift);memcpy(c.delta_shift,raw+0x598,sizeof c.delta_shift);ubig_stage_b_rt_cadence_summary_process(&c,out,scratch);ww32(raw,0x538,c.cursor.index);return 0;
}
static int lower_rank(uint8_t *raw,float control,float *out,float *scratch){
    if(!raw||!out||!scratch)return -1;
    UbigStageBRtRankHistory s;memcpy(s.matrix,raw,sizeof s.matrix);s.cursor.step=rr32(raw,0x184);s.cursor.index=rr32(raw,0x188);ubig_stage_b_rt_rank_history_process(&s,control,out,scratch);ww32(raw,0x188,s.cursor.index);return 0;
}
static int sib8cnative(void *sched_v,void *input_box_v,void *work){
    ++g_c8cnative;uint8_t *sched=(uint8_t*)sched_v;void **input_box=(void**)input_box_v;
    if(!sched||!input_box||!*input_box){++g_c8cfallback;return -1;}
    void **lower=(void**)rrp(sched,0x38);if(!lower){++g_c8cfallback;return -2;}
    UbigStageBRtSchedulerClock clock;memcpy(&clock,sched+0x40,sizeof clock);
    const uint32_t actions=ubig_stage_b_rt_scheduler_step(&clock);memcpy(sched+0x40,&clock,sizeof clock);
    Raw9Export *ri=(Raw9Export*)*input_box;UbigStageBRtSpectralExport input;if(!ri||!ri->bins){++g_c8cfallback;return -3;}raw_export_sem(ri,&input);
    if(actions&UBIG_STAGE_B_RT_SCHED_UPPER){
        ++g_cuppernative;
        uint8_t *feature=(uint8_t*)rrp(sched,0x00),*ratio=(uint8_t*)rrp(sched,0x10),*variation=(uint8_t*)rrp(sched,0x08),*spectral=(uint8_t*)rrp(sched,0x28),*projection=(uint8_t*)rrp(sched,0x18),*change=(uint8_t*)rrp(sched,0x20),*peak=(uint8_t*)rrp(sched,0x30);
        if(!feature||!ratio||!variation||!spectral||!projection||!change||!peak){++g_c8cfallback;return -4;}
        if(upper_feature_history(feature,&input)||upper_segment_ratio(ratio,&input)||upper_variation(variation,&input)||upper_spectral_change(spectral,&input)||upper_projection(projection,&input)){++g_c8cfallback;return -5;}
        uint32_t index=rr32(projection,0x530),written=index?index-1u:31u;
        if(upper_feature_change(change,(const float*)(projection+(size_t)written*0x20u),(float*)work)||upper_peak(peak,&input,(float*)work)){++g_c8cfallback;return -6;}
    }
    if(actions&UBIG_STAGE_B_RT_SCHED_LOWER_A){
        ++g_cloweranative;uint8_t *feature=(uint8_t*)rrp(sched,0x00),*ratio=(uint8_t*)rrp(sched,0x10);if(!feature||!ratio||!lower[0]||!lower[1]||lower_feature_cadence(feature,(float*)lower[0])||lower_ratio_stats(ratio,(float*)lower[1],(float*)work)){++g_c8cfallback;return -7;}
    }
    if(actions&UBIG_STAGE_B_RT_SCHED_LOWER_B){
        ++g_clowerbnative;uint8_t *feature=(uint8_t*)rrp(sched,0x00),*variation=(uint8_t*)rrp(sched,0x08),*spectral=(uint8_t*)rrp(sched,0x28),*projection=(uint8_t*)rrp(sched,0x18),*change=(uint8_t*)rrp(sched,0x20),*peak=(uint8_t*)rrp(sched,0x30);if(!feature||!variation||!spectral||!projection||!change||!peak||!lower[2]||!lower[3]||!lower[4]||!lower[5]||!lower[6]){++g_c8cfallback;return -8;}const float mean=ubig_stage_b_rt_feature_history_mean((const float(*)[20])feature);if(lower_variation_stats(variation,(float*)lower[2],(float*)work)||lower_stat_step(spectral,0x1c0,0x1c4,(float*)lower[6],(float*)work)||lower_projection_cadence(projection,(float*)lower[3],(float*)work)||lower_stat_step(change,0xa4,0xa8,(float*)lower[5],(float*)work)||lower_rank(peak,mean,(float*)lower[4],(float*)work)){++g_c8cfallback;return -9;}
    }
    return 0;
}
__attribute__((noinline)) static void sib7bnative(void *controller,void *desc,void *out,void *work){
    ++g_c7bnative;if(!controller||!desc||!out){++g_c7bfallback;abort();}
    void *spectral_box=rrp(controller,0),*sched=rrp(controller,8),*rc_v=rrp(controller,16),*export_v=rrp(controller,24);
    if(!spectral_box||!sched||!rc_v||!export_v){++g_c7bfallback;abort();}
    void *spectral=rrp(spectral_box,0);if(!spectral){++g_c7bfallback;abort();}
    if(sib9cbnative(spectral,desc,export_v)!=0){++g_c7bfallback;abort();}
    void *sched_out=(uint8_t*)controller+24;uintptr_t aligned=((uintptr_t)work+3u)&~(uintptr_t)3u;if(sib8cnative(sched,sched_out,(void*)aligned)!=0){++g_c7bfallback;abort();}
    void *holder=rrp(sched,0x38);float *features=holder?(float*)rrp(holder,0x38):NULL;
    uint8_t *rc=(uint8_t*)rc_v;if(!features){++g_c7bfallback;abort();}
    UbigStageBRtControlCadenceConfig cfg;UbigStageBRtControlDescriptor descs[5];if(build_control_cfg(rc,&cfg,descs)){++g_c7bfallback;abort();}
    UbigStageBRtControlCadence st;memset(&st,0,sizeof st);st.counter=rr32(rc,80);st.period=rr32(rc,84);st.cycle=rr32(rc,88);st.target=rr32(rc,92);st.reset=rr32(rc,96);st.armed=rr32(rc,100);memcpy(st.primary_result,(uint8_t*)controller+32,60);memcpy(st.secondary_result,(uint8_t*)controller+140,8);st.updated=rr32(controller,148);
    ubig_stage_b_rt_control_cadence_process(&st,&cfg,features);
    ww32(rc,80,st.counter);ww32(rc,84,st.period);ww32(rc,88,st.cycle);ww32(rc,92,st.target);ww32(rc,96,st.reset);ww32(rc,100,st.armed);memcpy((uint8_t*)controller+32,st.primary_result,60);memcpy((uint8_t*)controller+140,st.secondary_result,8);ww32(controller,148,st.updated);memcpy(out,(uint8_t*)controller+32,120);
}
__attribute__((noinline)) static void sib584native(void *raw_v,uint32_t *desc,float *out,void *work){
    ++g_c584native;uint8_t *raw=(uint8_t*)raw_v;
    if(!raw||!desc||!out){++g_c584fallback;abort();}
    const uint32_t rows=desc[0],objects=desc[1];
    if(rows>10u||objects>8u){++g_c584fallback;abort();}
    UbigStageBRtControlAggregateItem items[8];uint32_t item_count=0;
    if(rr32(raw,0)!=0u && objects!=0u){
        void ***source_rows=0;memcpy(&source_rows,(char*)desc+16,8);void *controller=0;memcpy(&controller,raw+32,8);
        if(!source_rows||!controller){++g_c584fallback;abort();}
        for(uint32_t object=0;object<objects;object++){
            uint8_t local[40] __attribute__((aligned(8)));memcpy(local,desc,40);ww32(local,4,1u);
            void *one[10];void **row_boxes[10];
            for(uint32_t row=0;row<rows;row++){if(!source_rows[row]){++g_c584fallback;abort();}one[row]=source_rows[row][object];row_boxes[row]=&one[row];}
            void ***boxes=row_boxes;memcpy(local+16,&boxes,8);
            uint8_t child_out[120] __attribute__((aligned(8)));memset(child_out,0,sizeof child_out);
            ++g_c7b2f0;sib7bnative(controller,local,child_out,work);result_to_item(child_out,&items[item_count++]);
        }
    }
    UbigStageBRtControlAggregateState ns;rawagg_to_native(raw,&ns);ubig_stage_b_rt_control_aggregate_process(&ns,items,item_count,out);nativeagg_to_raw(raw,&ns);
}
static void sem_deep_from_raw(UbigStageBRtDeepControllerState*s,UbigStageBRtDeepControllerConfig*c,uint8_t*r){
 uint8_t*m=*(uint8_t**)(r+8);memset(c,0,sizeof *c);memset(s,0,sizeof *s);uint32_t n;memcpy(&n,m+4,4);c->active_width=n;
 float *pf=(float*)(m+8);c->pair_bounds=(UbigStageBRtPairBoundsConfig){pf[1],pf[3],pf[4],pf[5]};memcpy(&c->dual_envelope,m+0x20,sizeof c->dual_envelope);memcpy(&c->residual_mean,m+0x48,sizeof c->residual_mean);
 float *e=(float*)(m+0x58);c->envelope.smooth_keep=e[2];c->envelope.smooth_inject=e[3];c->envelope.lower_limit=e[4];c->envelope.negative_slope=e[5];c->envelope.quadratic_scale=e[6];c->envelope.quadratic_limit=e[7];c->envelope.linear_offset=e[8];memcpy(&c->envelope.lane_weight,m+0x80,8);memcpy(&c->post_new,m+0x88,4);memcpy(&c->post_old,m+0x8c,4);
 s->config=c;memcpy(&s->mode,r,4);memcpy(&s->row_count_cache,r+4,4);s->dual.config=&c->dual_envelope;s->dual.active_width=n;memcpy(s->dual.primary,r+0x1c,80);memcpy(s->dual.secondary,r+0x6c,80);s->envelope.config=&c->envelope;s->envelope.active_width=n;memcpy(s->envelope.status,r+0xcc,80);memcpy(s->envelope.envelope,r+0x11c,80);memcpy(&s->envelope.scalar_envelope,r+0x16c,4);memcpy(&s->envelope.activity_state,r+0x170,4);memcpy(s->envelope.lane_activity,r+0x174,80);s->pair_bounds.config=&c->pair_bounds;s->pair_bounds.active_width=n;memcpy(&s->pair_bounds.baseline,r+0x1d4,4);s->residual_mean.config=&c->residual_mean;s->residual_mean.active_width=n;memcpy(&s->residual_mean.scalar,r+0x1e4,4);memcpy(s->intermediate,r+0x1f4,80);memcpy(s->output,r+0x248,80);
}
static void sem_deep_to_raw(uint8_t*r,const UbigStageBRtDeepControllerState*s){memcpy(r,&s->mode,4);memcpy(r+4,&s->row_count_cache,4);memcpy(r+0x1c,s->dual.primary,80);memcpy(r+0x6c,s->dual.secondary,80);memcpy(r+0xcc,s->envelope.status,80);memcpy(r+0x11c,s->envelope.envelope,80);memcpy(r+0x16c,&s->envelope.scalar_envelope,4);memcpy(r+0x170,&s->envelope.activity_state,4);memcpy(r+0x174,s->envelope.lane_activity,80);memcpy(r+0x1d4,&s->pair_bounds.baseline,4);memcpy(r+0x1e4,&s->residual_mean.scalar,4);memcpy(r+0x1f4,s->intermediate,80);memcpy(r+0x248,s->output,80);}
static void sem_late_from_raw(UbigStageBRtLateControllerState*s,UbigStageBRtLateControllerConfig*c,uint8_t*r,uint8_t*cfg){memset(s,0,sizeof*s);memset(c,0,sizeof*c);uint8_t*ctx=*(uint8_t**)(r+16),*sub=*(uint8_t**)(r+24);float**th=*(float***)(ctx);memcpy(&c->transform_filter,cfg+16,8);memcpy(&c->transform_phase,cfg+24,8);memcpy(&c->response_curve,sub+40,8);memcpy(&c->history_kernel,sub+48,8);memcpy(&c->limit,sub+4,4);memcpy(&c->history_scale,sub+8,4);memcpy(&s->output,r,4);memcpy(s->transform_history[0],th[0],576*4);memcpy(s->transform_history[1],th[1],576*4);memcpy(&s->previous_peak,sub+60,4);memcpy(&s->delayed_envelope,sub+64,4);memcpy(&s->ring_index,sub+68,4);memcpy(&s->gain,sub+76,4);memcpy(&s->envelope,sub+80,4);memcpy(&s->smoothed,sub+84,4);memcpy(&s->history_scale,sub+116,4);float*h=*(float**)(sub+88);memcpy(s->history,h,128*4);float*ring=*(float**)(sub+120);s->minimum_ring=*ring;}
static void sem_late_to_raw(uint8_t*r,const UbigStageBRtLateControllerState*s){uint8_t*ctx=*(uint8_t**)(r+16),*sub=*(uint8_t**)(r+24);float**th=*(float***)(ctx);memcpy(r,&s->output,4);memcpy(th[0],s->transform_history[0],576*4);memcpy(th[1],s->transform_history[1],576*4);memcpy(sub+60,&s->previous_peak,4);memcpy(sub+64,&s->delayed_envelope,4);memcpy(sub+68,&s->ring_index,4);memcpy(sub+76,&s->gain,4);memcpy(sub+80,&s->envelope,4);memcpy(sub+84,&s->smoothed,4);memcpy(sub+116,&s->history_scale,4);float*h=*(float**)(sub+88);memcpy(h,s->history,128*4);float*ring=*(float**)(sub+120);*ring=s->minimum_ring;}
__attribute__((noinline)) static void sib56b80(float p1,float p2,int *p3,void **p4,void *p5,void *p6,uint32_t *p7,uint32_t *p8,uint32_t *p9,void *p10,void *p11,void *p12,int p13,int p14,void *p15,void *p16,void *p17){
 ++g_c56b80;(void)p17;
 if(!p3||!p4||!p5||!p6||!p7||!p8||!p9||!p10||!p11||p2!=0.0f||p13!=1||p14!=0||p4[4]||p4[6]||p7[0]!=0||p7[2]!=2||p7[3]!=4||p7[4]!=77){++g_c56fallback;abort();}
 uint8_t*dr=*(uint8_t**)((char*)p3+8),*lr=*(uint8_t**)((char*)p3+16);UbigStageBRtLatePipelineState st;UbigStageBRtDeepControllerConfig dc;UbigStageBRtLateControllerConfig lc;sem_deep_from_raw(&st.deep,&dc,dr);sem_late_from_raw(&st.late,&lc,lr,p6);
 float***gp=*(float****)((char*)p7+24);UbigStageBRtComplexGroups groups={2,4,gp};float**ar=*(float***)((char*)p8+8),**orr=*(float***)((char*)p9+8);UbigStageBRtBandRows analysis={p8[0],p8[1],ar,p8[5]},output={p9[0],p9[1],orr,p9[5]};float**lpr=*(float***)((char*)p10+8);float lrows[2][256];for(int r=0;r<2;r++)memcpy(lrows[r],lpr[r],256*4);
 UbigStageBRtLatePipelineConfig cfg;memset(&cfg,0,sizeof cfg);memcpy(&cfg.analysis_offset,(char*)p5+0x60,4);cfg.band_ends=*(uint32_t**)((char*)p5+0x50);cfg.deep_controls=(const UbigStageBRtDeepControllerControls*)p4[0];cfg.deep_lower_source=p4[1];cfg.deep_upper_source=p4[2];cfg.deep_status=p4[3];cfg.late_config=&lc;cfg.band_aux=(const float*)p4[5];cfg.band_aux_meter_scale=0x820;cfg.band_aux_meter=(int32_t*)p12;
 ubig_stage_b_rt_late_pipeline_process(p1,&st,&cfg,&groups,&analysis,&output,p11,p15,p16,lrows);for(int r=0;r<2;r++)memcpy(lpr[r],lrows[r],256*4);sem_deep_to_raw(dr,&st.deep);sem_late_to_raw(lr,&st.late);p3[0]=1;p3[1]=0;((uint32_t*)p10)[0]=2u;
}

typedef struct { uint32_t rows,width; float **row; } RawRows54;
static const UbigStageBRtCurveRecord *g54_curve_fall,*g54_curve_rise;
static const float *g54_tail_weights,*g54_chain_coeff;
static unsigned char *g54_mode_ref[3],*g54_mode_slope[3];
static uint32_t rd32_54(const void*p,size_t o){uint32_t v;memcpy(&v,(const char*)p+o,4);return v;}
static int32_t rdi32_54(const void*p,size_t o){int32_t v;memcpy(&v,(const char*)p+o,4);return v;}
static float rdf_54(const void*p,size_t o){float v;memcpy(&v,(const char*)p+o,4);return v;}
static void *rdp_54(const void*p,size_t o){void*v;memcpy(&v,(const char*)p+o,8);return v;}
static void wr32_54(void*p,size_t o,uint32_t v){memcpy((char*)p+o,&v,4);}
static void wrf_54(void*p,size_t o,float v){memcpy((char*)p+o,&v,4);}
static void wrp_54(void*p,size_t o,const void*v){memcpy((char*)p+o,&v,8);}

static void corr_load54(unsigned char *p,UbigStageBRtCorrelationState *u){
    memset(u,0,sizeof(*u));
    u->primary.depth=rd32_54(p,0xa0);u->primary.index=rd32_54(p,0xa4);u->primary.buffer=rdp_54(p,0xb0);
    u->secondary_depth=rd32_54(p,0xe0);u->secondary_index=rd32_54(p,0xe4);u->secondary_buffer=rdp_54(p,0xe8);u->secondary_status=rdp_54(p,0xf0);
    u->integrator_depth=rd32_54(p,0xf8);u->integrator_index=rd32_54(p,0xfc);u->correlation_scale=rdf_54(p,0x100);u->accumulator_a=rdf_54(p,0x104);u->accumulator_b=rdf_54(p,0x108);u->integrator_ring=rdp_54(p,0x110);u->output_state=rdf_54(p,0x118);
}
static void corr_sync54(unsigned char*p,const UbigStageBRtCorrelationState*u){
    wr32_54(p,0xa4,u->primary.index);wr32_54(p,0xe4,u->secondary_index);wr32_54(p,0xfc,u->integrator_index);wrf_54(p,0x104,u->accumulator_a);wrf_54(p,0x108,u->accumulator_b);wrf_54(p,0x118,u->output_state);
}
static void win_load54(unsigned char *p,unsigned char *common,UbigStageBRtWindowBlendState *u){
    memset(u,0,sizeof(*u));
    memcpy(u->input_window.accumulator,p,80);memcpy(u->input_window.window_sum,p+0x140,80);u->input_window.depth=rd32_54(p,0x190);u->input_window.index=rd32_54(p,0x194);u->input_window.history=rdp_54(p,0x198);u->input_window.scale=rdf_54(p,0x1a0);
    memcpy(u->rms_window.accumulator,p+0x1c0,80);memcpy(u->rms_window.window_sum,p+0x300,80);u->rms_window.depth=rd32_54(p,0x350);u->rms_window.index=rd32_54(p,0x354);u->rms_window.history=rdp_54(p,0x358);u->rms_window.scale=rdf_54(p,0x360);
    u->blend_bias=rdf_54(common,0x2964);u->rms_scale=rdf_54(common,0x2968);u->blend_scale=rdf_54(common,0x296c);
}
static void win_sync54(unsigned char*p,const UbigStageBRtWindowBlendState*u){
    memcpy(p,u->input_window.accumulator,80);memcpy(p+0x140,u->input_window.window_sum,80);wr32_54(p,0x194,u->input_window.index);
    memcpy(p+0x1c0,u->rms_window.accumulator,80);memcpy(p+0x300,u->rms_window.window_sum,80);wr32_54(p,0x354,u->rms_window.index);
}
static void stereo_load54(unsigned char*p,UbigStageBRtStereoBlendState*u){
    memset(u,0,sizeof(*u));u->counter_scale=rdf_54(p,0);for(unsigned i=0;i<20;i++)u->counter[i]=rdi32_54(p,4+4*i);u->output_scale=rdf_54(p,0xa4);
    for(unsigned i=0;i<20;i++){u->adaptive[i]=rdf_54(p,0x148+4*i);u->smoothed[i]=rdf_54(p,0x198+4*i);}u->input_state0=rdf_54(p,0x1e8);u->input_state1=rdf_54(p,0x1ec);u->gate_state=rdf_54(p,0x1f0);u->input_mix=rdf_54(p,0x1f4);u->adaptive_mix=rdf_54(p,0x1f8);u->history=rdp_54(p,0x200);
}
static void stereo_sync54(unsigned char*p,const UbigStageBRtStereoBlendState*u){
    for(unsigned i=0;i<20;i++){wr32_54(p,4+4*i,(uint32_t)u->counter[i]);wrf_54(p,0x148+4*i,u->adaptive[i]);wrf_54(p,0x198+4*i,u->smoothed[i]);}wrf_54(p,0x1e8,u->input_state0);wrf_54(p,0x1ec,u->input_state1);wrf_54(p,0x1f0,u->gate_state);wrf_54(p,0x1f4,u->input_mix);wrf_54(p,0x1f8,u->adaptive_mix);
}

static void candidate54(float f0,float f1,void *state_v,uint32_t mode,float *optional,uint32_t *desc_v,void *work_v,int *out){
    unsigned char *s=state_v;RawRows54 *desc=(RawRows54*)desc_v,*work=(RawRows54*)work_v;
    if(!s||!desc||!work||desc->rows!=2u||desc->width!=20u||work->rows!=2u||work->width!=20u)return;
    UbigStageBRtMultibandState u;memset(&u,0,sizeof u);
    memcpy(u.curve_rows,s,sizeof u.curve_rows);u.curve_mode=rd32_54(s,0xf0);
    for(uint32_t r=0;r<2u;r++)corr_load54(s+0x2a80+(size_t)r*0xea0,&u.correlation[r]);
    u.optional_mix.bias=rdf_54(s,0x2a70);u.optional_mix.control_scale=rdf_54(s,0x2a74);
    for(uint32_t r=0;r<2u;r++)win_load54(s+0x100+(size_t)r*0x1460,s+0x100,&u.window[r]);
    memcpy(u.post_rows,s+0x29c0,sizeof u.post_rows);
    memcpy(u.blend_rows,s+0x4b40,sizeof u.blend_rows);u.blend_alpha=rdf_54(s,0x4be0);
    memcpy(&u.tail,s+0x4b38,sizeof u.tail);
    u.gate_decay_step=rdf_54(s,0x49c8);u.gate_correction_step=rdf_54(s,0x49cc);u.gate_keep=rdf_54(s,0x49e0);u.gate_inject=rdf_54(s,0x49e4);
    memcpy(u.gate_rows,s+0x49f0,sizeof u.gate_rows);
    u.stereo_alpha=rdf_54(s,0x4be4);memcpy(u.stereo_row,s+0x4be8,sizeof u.stereo_row);stereo_load54(s+0x47c0,&u.stereo);
    memcpy(&u.crossfade,s+0x4b30,sizeof u.crossfade);u.active_mode=rd32_54(s,0x49dc);u.enable_value=rdf_54(s,0x4864);
    uint32_t sel=rd32_54(s,0x49d8);unsigned fam=(sel==0)?0u:(sel==1?1u:2u);
    const float *ref=(const float*)(g54_mode_ref[fam]+(size_t)mode*0x50),*slope=(const float*)(g54_mode_slope[fam]+(size_t)mode*0x50);
    UbigStageBRtMultibandTuning tun={g54_curve_fall,g54_curve_rise,g54_tail_weights,g54_chain_coeff,ref,slope};
    UbigStageBRtBandRows dr={desc->rows,desc->width,desc->row,20u},wr={work->rows,work->width,work->row,20u};
    ubig_stage_b_rt_multiband_process(f0,f1,&u,mode,optional,&dr,&wr,out,&tun);
    memcpy(s,u.curve_rows,sizeof u.curve_rows);
    for(uint32_t r=0;r<2u;r++)corr_sync54(s+0x2a80+(size_t)r*0xea0,&u.correlation[r]);
    for(uint32_t r=0;r<2u;r++)win_sync54(s+0x100+(size_t)r*0x1460,&u.window[r]);
    memcpy(s+0x29c0,u.post_rows,sizeof u.post_rows);memcpy(s+0x4b40,u.blend_rows,sizeof u.blend_rows);wrf_54(s,0x4be0,u.blend_alpha);
    memcpy(s+0x4b38,&u.tail,sizeof u.tail);memcpy(s+0x49f0,u.gate_rows,sizeof u.gate_rows);
    wrf_54(s,0x4be4,u.stereo_alpha);memcpy(s+0x4be8,u.stereo_row,sizeof u.stereo_row);stereo_sync54(s+0x47c0,&u.stereo);memcpy(s+0x4b30,&u.crossfade,sizeof u.crossfade);
    wr32_54(s,0x49dc,u.active_mode);wrf_54(s,0x4864,u.enable_value);wrp_54(s,0x49d0,ref);wrp_54(s,0x49e8,slope);
}


static unsigned long g_c54native,g_c54fallback;
static void sib54native(float f0,float f1,void *state,uint32_t mode,float *optional,uint32_t *desc,void *work,int *out){
    ++g_c54native;RawRows54 *d=(RawRows54*)desc,*w=(RawRows54*)work;
    if(!state||!d||!w||d->rows!=2u||d->width!=20u||w->rows!=2u||w->width!=20u){++g_c54fallback;abort();}
    candidate54(f0,f1,state,mode,optional,desc,work,out);
}


static UbigStageBLevelerLookupTables g34_lookup;
static UbigStageBLevelerInverseLookupTables g34_inverse;
static const UbigStageBLevelerNormalizedCubic *g34_cubic;
static const float *g34_offsets,*g34_thresholds,*g34_band_weights,*g34_tail;
static const uint8_t *g34_prev_base,*g34_tmpl_base;
static unsigned long g_c34native,g_c34fallback;
typedef struct { float **rows; } G34StateRows;
typedef struct { const UbigStageBLevelerTransitionRecord *large_rise,*normal; } G34TransitionPair;
typedef struct {
    G34StateRows *matrix; UbigStageBLevelerLookupState *lookup; UbigStageBLevelerRowState *lifecycle;
    G34StateRows *transition_rows; UbigStageBLevelerProducerState *producer; UbigStageBLevelerState *writer;
    UbigStageBLevelerAdaptiveState *adaptive;
} G34StateBundle;
typedef struct {
    const float *base; const UbigStageBLevelerRowConfig *lifecycle; const UbigStageBLevelerLookupConfig *lookup;
    const G34TransitionPair *transition; const UbigStageBLevelerAdaptiveControl *adaptive;
    const UbigStageBLevelerSymmetricFilter *filter; const UbigStageBLevelerTransitionRecord *matrix_transition;
    const UbigStageBLevelerConfig *writer; const UbigStageBLevelerProducerConfig *producer;
} G34ConfigBundle;
static uint32_t g34_r32(const uint8_t*p,size_t o){uint32_t v;memcpy(&v,p+o,4);return v;}
static uint64_t g34_r64(const uint8_t*p,size_t o){uint64_t v;memcpy(&v,p+o,8);return v;}
static float g34_rf(const uint8_t*p,size_t o){float v;memcpy(&v,p+o,4);return v;}
static void g34_w32(uint8_t*p,size_t o,uint32_t v){memcpy(p+o,&v,4);}
static void g34_wf(uint8_t*p,size_t o,float v){memcpy(p+o,&v,4);}
static int g34_native(float caller_f0,uint64_t a0,uint64_t a1,uint64_t a2,uint64_t a3,uint64_t a4,uint64_t a5){
    (void)caller_f0;(void)a5;uint8_t *raw=(uint8_t*)(uintptr_t)a0;float *control_a=(float*)(uintptr_t)a3,*control_b=(float*)(uintptr_t)a4;
    if(!raw||!control_a||!control_b||!a1||!a2)return -1;
    const uint32_t flag_d0=g34_r32(raw,0x6d0),flag_e0=g34_r32(raw,0x6e0),flag_f0=g34_r32(raw,0x6f0);
    if(!flag_d0&&!flag_e0&&!flag_f0)return 0;
    const uint32_t alt=g34_r32(raw,0x944);if(alt||flag_d0||!flag_e0)return -2;
    const uint64_t cfgp=g34_r64(raw,0x28),bundlep=g34_r64(raw,0x1300);if(!cfgp||!bundlep)return -3;
    G34StateBundle *bs=(G34StateBundle*)(uintptr_t)bundlep;const G34ConfigBundle *bc=(const G34ConfigBundle*)(uintptr_t)g34_r64((const uint8_t*)(uintptr_t)cfgp,0x88);
    UbigStageBLevelerInputRows *input=(UbigStageBLevelerInputRows*)(uintptr_t)a1,*output=(UbigStageBLevelerInputRows*)(uintptr_t)(a1+0x18u);
    if(!bs||!bc||!input||!output||input->count!=2u||input->width!=20u||output->count!=2u||output->width!=20u)return -4;
    UbigStageBLevelerParentState ps={bs->matrix->rows,bs->lookup,bs->lifecycle,bs->transition_rows->rows,bs->producer,bs->writer,bs->adaptive};
    UbigStageBLevelerParentConfig pc={bc->base,bc->lifecycle,bc->lookup,bc->transition->large_rise,bc->transition->normal,bc->adaptive,bc->filter,bc->matrix_transition,bc->writer,bc->producer};
    UbigStageBLevelerParentTuning pt={&g34_lookup,&g34_inverse,g34_cubic,g34_offsets,g34_thresholds,g34_band_weights,g34_tail};
    UbigStageBLevelerWrapperState ws={g34_rf(raw,0x98c),g34_rf(raw,0x990),g34_r32(raw,0x940),g34_r32(raw,0x700)};float step;memcpy(&step,(const void*)(uintptr_t)(cfgp+0x94u),4);
    UbigStageBLevelerWrapperConfig wc={1u,flag_f0,g34_r32(raw,0x66c)!=0u,g34_r32(raw,0x674)!=0u,g34_r32(raw,0x6e8)!=0u,step,g34_rf(raw,0x648),g34_rf(raw,0x650),g34_rf(raw,0x6f8),g34_rf(raw,0x658),g34_rf(raw,0x654)};
    const uint32_t mode=g34_r32(raw,0x6d8);const float *prev=(const float*)(g34_prev_base+(size_t)mode*0x48u),*tmpl=(const float*)(g34_tmpl_base+(size_t)mode*0x48u);
    const UbigStageBLevelerSourceGate *source=(const UbigStageBLevelerSourceGate*)(uintptr_t)g34_r64(raw,0x12f0);
    ubig_stage_b_leveler_wrapper_process(&ws,&wc,&ps,&pc,&pt,prev,tmpl,source,input,output,(int32_t*)(uintptr_t)a2,control_a,control_b);
    g34_wf(raw,0x98c,ws.smoothed_limit);g34_wf(raw,0x990,ws.parent_result);g34_w32(raw,0x940,ws.force_target);g34_w32(raw,0x700,ws.adaptive_direct);return 0;
}
static void sib34native(float f0,uint64_t a0,uint64_t a1,uint64_t a2,uint64_t a3,uint64_t a4,uint64_t a5){++g_c34native;int rc=g34_native(f0,a0,a1,a2,a3,a4,a5);if(rc){++g_c34fallback;fprintf(stderr,"34 native unexpected rc=%d flags=%u/%u/%u alt=%u mode=%u\\n",rc,a0?g34_r32((uint8_t*)(uintptr_t)a0,0x6d0):0,a0?g34_r32((uint8_t*)(uintptr_t)a0,0x6e0):0,a0?g34_r32((uint8_t*)(uintptr_t)a0,0x6f0):0,a0?g34_r32((uint8_t*)(uintptr_t)a0,0x944):0,a0?g34_r32((uint8_t*)(uintptr_t)a0,0x6d8):0);abort();}}


static unsigned long g_c347native,g_c456native,g_c4f1native,g_cq31native;
static __attribute__((unused)) void sib347native(void *state,void *desc,void *p3,uint64_t p4,uint64_t p5){(void)desc;(void)p3;(void)p4;(void)p5;++g_c347native;if(!state)abort();uint64_t zero=0;memcpy((uint8_t*)state+0x6a0,&zero,8);}
__attribute__((noinline,used)) static void *sib45600_c(void *p0,void *p1,uint32_t active_width,void *workspace){(void)p1;++g_c456native;void *nested=NULL;uint32_t width=0;if(p0)memcpy(&nested,(const char*)p0+8,8);if(nested)memcpy(&width,(const char*)nested+12,4);return ubig_stage_b_rt_outer_support_build(width,active_width,workspace);}
extern void *sib456native(void*,void*,uint32_t,void*);
__asm__(".text\n.align 2\n.type sib456native,%function\nsib456native:\n stp x4,x30,[sp,#-16]!\n bl sib45600_c\n ldp x4,x30,[sp],#16\n ret\n.size sib456native,.-sib456native\n");
__attribute__((noinline,used)) UbigStageBRtSparseRemapPlan *sib4f1native_c(uint32_t source_rows,uint32_t target_rows,const float *dense,void *workspace){++g_c4f1native;return ubig_stage_b_rt_sparse_plan_build(source_rows,target_rows,dense,workspace);}
extern UbigStageBRtSparseRemapPlan *sib4f1native(uint32_t,uint32_t,const float*,void*);
__asm__(".text\n.align 2\n.type sib4f1native,%function\nsib4f1native:\n stp x4,x30,[sp,#-16]!\n bl sib4f1native_c\n ldp x4,x30,[sp],#16\n ret\n.size sib4f1native,.-sib4f1native\n");
static __attribute__((unused)) int32_t sibq31scaled(float scaled){++g_cq31native;return (int32_t)lrintf(scaled);}


typedef struct { int32_t v[5]; } Ret376Native;
typedef struct { uint32_t rows,width; void **row; uint32_t slots,stride; } RawRows376;
typedef struct { RawRows376 analysis, output; } RawRowsPair376;
static unsigned long g_c376native;
static float fb376(uint32_t u){float f;memcpy(&f,&u,4);return f;}
static float fadd376(float a,float b){float r;__asm__("fadd %s0,%s1,%s2":"=w"(r):"w"(a),"w"(b));return r;}
static float fsub376(float a,float b){float r;__asm__("fsub %s0,%s1,%s2":"=w"(r):"w"(a),"w"(b));return r;}
static void *rp376(const void *p,size_t o){void *v=NULL;memcpy(&v,(const uint8_t*)p+o,8);return v;}
static void wp376(void *p,size_t o,void *v){memcpy((uint8_t*)p+o,&v,8);}
static Ret376Native native376(void *state_v,float row_offset,void *late_rows_raw,uint32_t *main,void *aux_raw,void *workspace_v){
    ++g_c376native;
    uint8_t *state=(uint8_t*)state_v,*cfg=(uint8_t*)rp376(state,0x28);uint8_t *workspace=(uint8_t*)workspace_v;
    Ret376Native ret={{0}};
    if(!state||!cfg||!main||!workspace||rr32(state,0x11c)!=1u||main[0]!=0u||main[1]!=0u||main[2]!=2u||main[3]!=4u||main[4]!=77u||(aux_raw&&((uint32_t*)aux_raw)[2]!=0u)||rr32(state,4)!=0u||rr32(state,8)!=0u||rr32(state,0xc)!=0u||rr32(state,0x2d98)!=0u||rr32(state,0xd64)!=0u){fprintf(stderr,"N376 contract fail mode=%u main=%u/%u/%u/%u/%u aux=%p st4=%u st8=%u c=%u 2d98=%u d64=%u\\n",state?rr32(state,0x11c):999,main?main[0]:999,main?main[1]:999,main?main[2]:999,main?main[3]:999,main?main[4]:999,aux_raw,state?rr32(state,4):999,state?rr32(state,8):999,state?rr32(state,0xc):999,state?rr32(state,0x2d98):999,state?rr32(state,0xd64):999);abort();}
    const uint32_t bands=rr32(cfg,0x0c);if(bands!=20u){fprintf(stderr,"N376 bands=%u\\n",bands);abort();}

    const float c490=fb376(0x3dbd0bd1u),c494=fb376(0x3ead4ad5u),c4a0=fb376(0xbd3d0bd1u),c4a8=fb376(0x3ddc8dc9u),c4ac=fb376(0xbe8dc8ddu),pair_scale=fb376(0x3f34fe00u),c4e4=fb376(0x3e7c8c5fu);
    const float extra=rr32(state,0x6e0)?rrf(state,0x638):0.0f;
    float base=fadd376(rrf(state,0x630),extra);if(c490<=base)base=c490;
    float control=fadd376(rrf(state,0x628),base);if(control<-1.0f)control=-1.0f;if(control>c494)control=c494;
    const float initial_control=control;

    memset((uint8_t*)workspace,0,0); /* document caller-owned scratch; rows below are explicitly initialized */
    int32_t meter[20];float linked[20];memset(meter,0,sizeof meter);memset(linked,0,sizeof linked);

    if(rr32(state,0x690)!=0u){void *agg=rp376(state,0x1308);sib584native(agg,main+2,(float*)(state+0x654),workspace);}
    ret.v[0]=ubig_stage_b_rt_q31_encode(rrf(state,0x654));
    ret.v[1]=ubig_stage_b_rt_q31_encode(rrf(state,0x658));
    ret.v[2]=ubig_stage_b_rt_q31_encode(rrf(state,0x65c));
    ret.v[3]=ubig_stage_b_rt_q31_encode(rrf(state,0x660));
    ret.v[4]=ubig_stage_b_rt_q31_encode(rrf(state,0x664));

    /* 0x34778 is a deployed stereo no-op: it only clears the legacy route pointer. */
    wp376(state,0x6a0,NULL);

    const uint32_t active_rows=2u;
    if(rr32(state,0x93c)!=active_rows){
        void *outer_cfg=rp376(cfg,0x88),*nested=NULL;uint32_t width=0; if(outer_cfg)memcpy(&nested,(uint8_t*)outer_cfg+8,8);if(nested)memcpy(&width,(uint8_t*)nested+12,4);
        void *bundle=ubig_stage_b_rt_outer_support_build(width,bands,rp376(state,0x12f8));
        ww32(state,0x700,1u);wp376(state,0x1300,bundle);ww32(state,0x93c,active_rows);ww32(state,0x940,1u);
    }
    if(rr32(state,0x6e0)==0u&&rr32(state,0x6d0)==0u&&rr32(state,0x6f0)==0u)ww32(state,0x940,1u);

    void **a_ptr=(void**)(((uintptr_t)workspace+7u)&~(uintptr_t)7u);
    for(uint32_t i=0;i<10u;i++)a_ptr[i]=(void*)(((uintptr_t)workspace+0x76u+(uintptr_t)i*0x6fu)&~(uintptr_t)31u);
    void **b_ptr=(void**)(((uintptr_t)workspace+0x4b4u)&~(uintptr_t)7u);
    for(uint32_t i=0;i<10u;i++)b_ptr[i]=(void*)(((uintptr_t)workspace+0x523u+(uintptr_t)i*0x6fu)&~(uintptr_t)31u);
    void *leveler_tail=workspace+0x95a;
    RawRowsPair376 rows={
        .analysis={2u,20u,a_ptr,10u,20u},
        .output={2u,20u,b_ptr,10u,20u}
    };
    uint32_t map[10]={0u,1u,0u,0u,0u,0u,0u,0u,0u,0u};

    float ***groups=NULL;memcpy(&groups,(uint8_t*)(main+2)+16,8);if(!groups||!groups[0]||!groups[1])abort();
    for(uint32_t plane=0;plane<4u;plane++){if(!groups[0][plane]||!groups[1][plane])abort();ubig_stage_b_rt_pair_transform(groups[0][plane],groups[1][plane],77u,pair_scale);}

    float band_offset=rrf(state,0x618);if(band_offset<c4ac)band_offset=c4ac;band_offset=fadd376(band_offset,row_offset);
    sib60200(band_offset,rrf(cfg,0x60),main+2,NULL,NULL,rp376(cfg,0x50), (int32_t*)map,(uint32_t*)&rows.analysis,&rows.output);

    if(rr32(state,0x6a8)!=0u){float optional=rrf(state,0x65c);float *op=rr32(state,0x67c)?&optional:NULL;sib54native(rrf(state,0x6b8),rrf(state,0x990),rp376(state,0x12e0),rr32(state,0x6b0),op,(uint32_t*)&rows.analysis,&rows.output,meter);}
    float pair_controls[2]={0.0f,0.0f};
    sib34native(fadd376(rrf(state,0x620),rrf(state,0x618)),(uint64_t)(uintptr_t)state,(uint64_t)(uintptr_t)&rows.analysis,(uint64_t)(uintptr_t)meter,(uint64_t)(uintptr_t)&control,(uint64_t)(uintptr_t)pair_controls,(uint64_t)(uintptr_t)leveler_tail);

    const float processed_control=control;
    float negative=0.0f;if(control<=0.0f)negative=control;
    const float shape_a=fsub376(negative,c4a8);
    const float shape_b=fsub376(c4a0,fsub376(row_offset,c4a8));
    sib5f5a8(shape_a,shape_b,&rows.analysis,&rows.output,map,rp376(cfg,0x50),main+2,NULL);
    for(uint32_t plane=0;plane<4u;plane++)ubig_stage_b_rt_pair_inverse_transform(groups[0][plane],groups[1][plane],77u,pair_scale);

    const float local420=fsub376(row_offset,c4a8);
    const float local408=fsub376(processed_control,negative);

    /* Mode-1 deployed sparse path. It is identity at run time but the one-time
       plan build is retained because the caller owns/cache-checks that state. */
    if(rr32(state,0x12c)!=0u){
        if(rr32(state,0x5e0)!=0u||rr32(state,0x5e4)!=main[0]){
            const uint32_t target=rr32(state,0x124);for(uint32_t r=0;r<target;r++)for(uint32_t c=0;c<2u;c++){float v=rrf(state,0x2c0+4u*(r*2u+c));if((main[0]>>c)&1u)v=0.0f;wwf(state,0x450+4u*(r*2u+c),v);}
            UbigStageBRtSparseRemapPlan *plan=ubig_stage_b_rt_sparse_plan_build(2u,target,(const float*)(state+0x450),rp376(state,0x5e8));ww32(state,0x5e0,0u);wp376(state,0x5f0,plan);ww32(state,0x5e4,main[0]);
        }
        main[0]=0u;
    }

    float late_offset=local408;if(late_offset>c494)late_offset=c494;
    UbigStageBRtDeepControllerControls deep_controls;
    deep_controls.gain=rrf(state,0xdd0);deep_controls.subtract=rrf(state,0x620);
    deep_controls.bias=fsub376(late_offset,fadd376(local420,c4e4));deep_controls.base_offset=rrf(state,0xddc);deep_controls.dual_offset=c4e4;deep_controls.modulation_scale=rrf(state,0xde8);
    void *deep_bundle[7]={&deep_controls,state+0x112c,state+0x10dc,state+0x117c,NULL,NULL,NULL};
    void *db8=rp376(state,0xdb8);if(db8&&rr32(db8,0)!=0u)deep_bundle[4]=(uint8_t*)db8+0x338; /* cold on deployed contract */
    if(rr32(state,0x9e8)!=0u)deep_bundle[5]=state+0x9fc;
    if(rr32(state,0xc94)!=0u)deep_bundle[6]=state+0xd0c;
    if(deep_bundle[4]||deep_bundle[6]){fprintf(stderr,"N376 unexpected late aux\\n");abort();}

    ubig_stage_b_rt_telemetry_bias(meter,linked,20u,initial_control,fsub376(base,extra),local408,local420);
    int *late_state=(int*)rp376(state,0x18a8);
    sib56b80(late_offset,0.0f,late_state,deep_bundle,cfg,state+0x88,main,(uint32_t*)&rows.analysis,(uint32_t*)&rows.output,late_rows_raw,linked,meter,(int)(rr32(state,0xdfc)!=0u),(int)(rr32(state,0xdf4)!=0u),state+0x1220,state+0x11d0,b_ptr);

    uint8_t *traw=(uint8_t*)rp376(state,0x1270);if(!traw)abort();UbigStageBRtTelemetrySmoothState ts;memcpy(ts.code,traw,80);memcpy(ts.scaled,traw+0x50,80);memcpy(ts.value,traw+0xa0,80);ts.coeff=(const float*)rp376(traw,0x1e8);ubig_stage_b_rt_telemetry_smooth(&ts,meter,linked,20u);memcpy(traw,ts.code,80);memcpy(traw+0x50,ts.scaled,80);memcpy(traw+0xa0,ts.value,80);
    return ret;
}


static unsigned long g_c5cenative;
static uintptr_t align8_5ce(uintptr_t v){return (v+7u)&~(uintptr_t)7u;}
static uintptr_t align32_5ce(uintptr_t v){return (v+31u)&~(uintptr_t)31u;}

static Ret376Native native5ce70(float input_offset,void *state_v,uint32_t *input_v,
                                void *export_v,void *workspace_v)
{
    ++g_c5cenative;
    uint8_t *state=(uint8_t*)state_v,*workspace=(uint8_t*)workspace_v;
    Ret376Native zero={{0}};
    if(!state||!input_v||!workspace||!export_v)abort();
    if(rr32(state,8)!=0u||rr32(state,0x10)!=0u||rr32(state,0x14)!=0u||
       rr32(state,0xcc)!=0u||rr32(state,0x114)!=0u||rr32(state,0x5f8)!=77u||
       rr32(state,0x2d98)!=0u||input_v[0]!=2u||input_v[1]!=4u||
       **(uint32_t**)(state+0x28)>=0x15888u){
        fprintf(stderr,"N5CE contract fail st8=%u s10=%u s14=%u cc=%u 114=%u bins=%u aux=%u in=%u/%u rate=%u\n",
                rr32(state,8),rr32(state,0x10),rr32(state,0x14),rr32(state,0xcc),
                rr32(state,0x114),rr32(state,0x5f8),rr32(state,0x2d98),
                input_v[0],input_v[1],**(uint32_t**)(state+0x28));
        abort();
    }

    /* Persistent 10 x 256 late-row scratch: exact deployed workspace layout. */
    float **late_rows=(float**)align8_5ce((uintptr_t)workspace+7u);
    for(uint32_t i=0;i<10u;i++)
        late_rows[i]=(float*)align32_5ce((uintptr_t)workspace+0x76u+(uintptr_t)i*0x41fu);
    struct {uint32_t rows,width;float **row;uint32_t slots,stride;} late_desc={0u,256u,late_rows,10u,256u};

    /* Exact 10-row x four-plane scratch layout used by the live transform bank. */
    const uint32_t scratch_rows=10u;
    uintptr_t lvar8=(uintptr_t)workspace+0x298du;
    uintptr_t lvar12=lvar8+0x26u;
    uintptr_t cursor=lvar12+(uintptr_t)scratch_rows*8u;
    void ***group_records=(void***)align8_5ce(lvar8+0x2du);
    uint32_t index=0u;
    if(scratch_rows>3u){
        uintptr_t u=cursor+0x27u;
        cursor+=(uintptr_t)(((scratch_rows-4u)>>2)+1u)*0x80u;
        uint32_t p=2u;
        do{
            group_records[index]=(void**)align8_5ce(u-0x20u);
            uint32_t next=index+4u;index=next;
            group_records[p-1u]=(void**)align8_5ce(u);
            group_records[p]=(void**)align8_5ce(u+0x20u);
            group_records[p+1u]=(void**)align8_5ce(u+0x40u);
            u+=0x80u;p+=4u;
        }while(index<scratch_rows-3u);
    }
    if(index<scratch_rows){
        uint32_t remaining=scratch_rows-index;
        uintptr_t u=cursor+7u;
        cursor+=(uintptr_t)remaining*0x20u;
        while(remaining--){group_records[index++]=(void**)align8_5ce(u);u+=0x20u;}
    }
    uintptr_t data=align32_5ce(cursor+0x1fu);
    for(uint32_t i=0;i<scratch_rows;i++){
        group_records[i][0]=(void*)data;
        group_records[i][1]=(void*)(data+0x280u);
        group_records[i][2]=(void*)(data+0x500u);
        group_records[i][3]=(void*)(data+0x780u);
        data+=0xa00u;
    }

    uint8_t main_raw[40] __attribute__((aligned(8)));
    memset(main_raw,0,sizeof main_raw);
    ww32(main_raw,0,0u);ww32(main_raw,4,0u);ww32(main_raw,8,2u);ww32(main_raw,12,4u);
    ww32(main_raw,16,77u);memcpy(main_raw+24,&group_records,8);ww32(main_raw,32,10u);

    float ***input_groups=NULL;memcpy(&input_groups,(uint8_t*)input_v+16,8);
    void ***ring_groups=(void***)rp376(state,0x12c0);
    void *history_cfg=rp376(state,0x12a0),*coeff_cfg=rp376(state,0xa8);
    if(!input_groups||!ring_groups||!history_cfg||!coeff_cfg)abort();
    const uint32_t initial_ring=rr32(state,0x12a8);
    const uint32_t ring_depth=rr32(coeff_cfg,0x14);
    if(ring_depth==0u)abort();

    for(uint32_t group=0;group<2u;group++){
        uint32_t ring=initial_ring;ww32(state,0x12a8,ring);
        if(!input_groups[group]||!ring_groups[group]||!group_records[group])abort();
        for(uint32_t plane=0;plane<4u;plane++){
            uint8_t *slot=(uint8_t*)ring_groups[group][ring];
            float *src=input_groups[group][plane];
            float *dst=(float*)group_records[group][plane];
            if(!slot||!src||!dst)abort();
            memcpy((uint8_t*)dst+0x80u,slot+0x18u,0x1e8u);
            for(uint32_t k=0;k<128u;k++)((float*)slot)[k]=src[k]*0.125f;
            sib4a570_native(history_cfg,coeff_cfg,group,dst,(float*)slot);
            ring++;
            if(ring_depth<=ring)ring=0u;
            ww32(state,0x12a8,ring);
        }
    }

    /* Reference computes this child-workspace address independently of alignment. */
    void *child_workspace=(void*)((uintptr_t)workspace+0x8f69u);
    const float row_offset=fsub376(input_offset,fb376(0x3e0e457bu));
    Ret376Native ret=native376(state,row_offset,&late_desc,(uint32_t*)main_raw,NULL,child_workspace);
    uint64_t export_rows=0,export_stride=0,export_type=0;float **export_ptrs=NULL;
    memcpy(&export_rows,(uint8_t*)export_v+0,8);memcpy(&export_stride,(uint8_t*)export_v+8,8);
    memcpy(&export_type,(uint8_t*)export_v+16,8);memcpy(&export_ptrs,(uint8_t*)export_v+24,8);
    if(late_desc.rows!=2u||late_desc.width!=256u||export_rows!=2u||export_stride!=2u||
       export_type!=7u||!export_ptrs||!export_ptrs[0]||!export_ptrs[1])abort();
    for(uint32_t r=0;r<2u;r++)for(uint32_t i=0;i<256u;i++)
        export_ptrs[r][(size_t)i*export_stride]=late_rows[r][i];
    (void)zero;
    return ret;
}



static unsigned long g_c3anative;
static Ret376Native native3abe0(void *state_v,void *desc_v,void *workspace_v)
{
    ++g_c3anative;
    uint8_t *state=(uint8_t*)state_v;
    uint64_t *desc=(uint64_t*)desc_v;
    Ret376Native zero={{0}};
    if(!state||!desc||!workspace_v)return zero;
    if(rp376(state,0xd8)!=NULL||rp376(state,0x80)!=NULL||rr32(state,0xd0)!=1u||
       rr32(state,0x10)!=0u||rr32(state,0x14)!=0u||rr32(state,0x58)!=2u||
       rr32(state,0x60)!=2u||rr32(state,0x68)!=7u||
       (uint32_t)desc[0]!=2u||(uint32_t)desc[1]!=2u||(uint32_t)desc[2]!=7u||!desc[3]||
       !rp376(state,0x70)||!rp376(state,0xc0)||!rp376(state,0x1288)||
       !rp376(state,0x98)||!rp376(state,0xa0)){
        fprintf(stderr,"N3A contract fail d8=%p s80=%p d0=%u s10=%u s14=%u 58=%llu 60=%llu 68=%llu desc=%llu/%llu/%llu\n",
                rp376(state,0xd8),rp376(state,0x80),rr32(state,0xd0),rr32(state,0x10),rr32(state,0x14),
                (unsigned long long)*(uint64_t*)(state+0x58),(unsigned long long)*(uint64_t*)(state+0x60),
                (unsigned long long)*(uint64_t*)(state+0x68),(unsigned long long)desc[0],
                (unsigned long long)desc[1],(unsigned long long)desc[2]);
        abort();
    }
    /* Deployed d8=NULL route forces direct mode and clears the two legacy flags. */
    ww32(state,0xcc,0u);ww32(state,0x9c8,0u);ww32(state,0x9d0,0u);

    float **source_rows=(float**)rp376(state,0x70);
    float **history=*(float ***)rp376(state,0x1288);
    uint32_t *counter=*(uint32_t**)((uint8_t*)rp376(state,0x1288)+16u);
    if(!source_rows||!source_rows[0]||!source_rows[1]||!history||!counter)abort();
    UbigStageBRtHistoryFilter64State hist={history,counter,ubig_stage_b_rt_fft64_normalized};

    _Alignas(32) float source[2][256];
    _Alignas(32) float transformed[2][4][128];
    float *group_rows[2][4];
    float **groups[2]={group_rows[0],group_rows[1]};
    for(uint32_t row=0u;row<2u;row++){
        const float *src=source_rows[row];
        for(uint32_t n=0u;n<64u;n++){
            source[row][4u*n]=src[8u*n];
            source[row][4u*n+1u]=src[8u*n+2u];
            source[row][4u*n+2u]=src[8u*n+4u];
            source[row][4u*n+3u]=src[8u*n+6u];
        }
        for(uint32_t block=0u;block<4u;block++){
            group_rows[row][block]=transformed[row][block];
            ubig_stage_b_rt_history_filter64_process(&hist,
                (const float*)rp376(state,0x98),(const float*)rp376(state,0xa0),row,
                transformed[row][block],source[row]+64u*block);
        }
    }

    uint8_t input_raw[40] __attribute__((aligned(8)));
    memset(input_raw,0,sizeof input_raw);
    ww32(input_raw,0,2u);ww32(input_raw,4,4u);ww32(input_raw,8,64u);
    float ***groups_ptr=groups;memcpy(input_raw+16,&groups_ptr,8);ww32(input_raw,24,10u);ww32(input_raw,28,4u);ww32(input_raw,32,64u);

    /* Reference scratch parent is 256-byte aligned. With ten row records the
       nested 5CE70 workspace begins at +0x51b6 from that aligned base. */
    uintptr_t base=((uintptr_t)workspace_v+0xffu)&~(uintptr_t)0xffu;
    void *child_workspace=(void*)(base+0x51b6u);
    Ret376Native r=native5ce70(fb376(0xbd3d0bd1u),state,(uint32_t*)input_raw,desc_v,child_workspace);
    return r;
}

static unsigned long g_c3awrap,g_c425,g_c425blocks;

static unsigned long g_c30native,g_c2fref,g_c31native,g_cf93native;
static uint32_t native30f78_live(void *state_v){
 ++g_c30native;uint8_t*s=(uint8_t*)state_v;if(!s)abort();
 const uint32_t prior_d4=rr32(s,0xd4);
 /* Deployed call has all optional arguments null/zero and state+0xd8 remains null. */
 ww32(s,0x944,0u);wp376(s,0xd8,NULL);ww32(s,0x110,0u);ww32(s,0x2db4,0u);
 ww32(s,0x104,0u);ww32(s,0x108,0u);ww32(s,0x10c,0u);
 ww32(s,0xe0,0x4000u);ww32(s,0xe4,0u);ww32(s,0xe8,0u);ww32(s,0xec,0u);
 ww32(s,0xf0,0x4000u);ww32(s,0xf4,0u);ww32(s,0xf8,0u);ww32(s,0xfc,0u);ww32(s,0x100,0x4000u);
 ww32(s,0x690,1u);wwf(s,0x9dc,fb376(0x3eb4fdf4u));wwf(s,0x9e0,fb376(0x3eb4fdf4u));
 if(rr32(s,0x114)!=0u){ww32(s,0x114,0u);ww32(s,0x1278,1u);}
 /* Lock shim is semantically uncontended. Config application is one-shot after init. */
 if(rr32(s,0x1278)!=0u){
  if(rr32(s,0x0c)==0u && rr32(s,0xaa0)!=0u){
   const uint32_t n=rr32(s,0x9f8);if(n>20u)abort();
   const float k=fb376(0x39fc0fc1u);
   for(uint32_t i=0;i<n;i++)wwf(s,0x9fc+4u*i,(float)(int32_t)rr32(s,0xa50+4u*i)*k);
   ww32(s,0xaa0,0u);ww32(s,0x9e8,rr32(s,0xa4c));
  }
  ++g_c2fref;ww32(s,0x1278,0u);
 }
 uint32_t out;
 if(rr32(s,0x12c)==0u){switch(rr32(s,0x11c)){case 0u:out=1u;break;case 3u:case 4u:case 10u:out=6u;break;case 5u:case 6u:case 11u:out=8u;break;case 7u:out=10u;break;default:out=2u;break;}}
 else out=rr32(s,0x124);
 void*map=rp376(s,0x2da0);if(map)memset(map,0,(size_t)rr32(s,0x10)*4u);ww32(s,0x2d98,0u);
 /* With null routing and unchanged d8, the deployed reset branches are not taken. */
 if(prior_d4==1u||prior_d4==2u||prior_d4==6u||prior_d4==8u||prior_d4==10u)return out;
 return 0u;
}
static uint32_t native31b68_live(void *state_v,uint32_t mode,void *desc_v,void *unused3,uint32_t unused4,void*unused5,void*unused6,void*unused7,void*unused8,void*unused9,uint32_t unused10,uint32_t unused11){
 (void)unused3;(void)unused4;(void)unused5;(void)unused6;(void)unused7;(void)unused8;(void)unused9;(void)unused10;(void)unused11;
 ++g_c31native;uint8_t *s=(uint8_t*)state_v;
 if(!s||!desc_v||!rp376(s,0x28)||**(uint32_t**)(s+0x28)>=0x15888u||mode<1u||mode>8u){fprintf(stderr,"N31 contract fail mode=%u rate=%u desc=%p\n",mode,(s&&rp376(s,0x28))?**(uint32_t**)(s+0x28):0u,desc_v);abort();}
 memcpy(s+0x58,desc_v,32u);
 wp376(s,0x80,NULL);ww32(s,0x88,1u);ww32(s,0x8c,128u);ww32(s,0x90,192u);
 uint8_t *cfg=(uint8_t*)rp376(s,0x28);wp376(s,0x98,rp376(cfg,0x28));wp376(s,0xa0,rp376(cfg,0x30));wp376(s,0xa8,rp376(cfg,0x38));
 wp376(s,0xb0,NULL);wp376(s,0xb8,NULL);ww32(s,0xd0,mode);
 return native30f78_live(s);
}
static uint32_t native_f93a8(void *outer_v){
 ++g_cf93native;uint8_t *o=(uint8_t*)outer_v,*inner=(uint8_t*)rp376(o,8);if(!inner)abort();
 const uint32_t base=rr32(inner,8),expanded=rr32(inner,12);
 if(*(uint8_t*)(o+0xf8)!=0u&&rr32(o,0xfc)==0u&&expanded>base)return expanded;
 return base;
}


static void native_f65e0(void *self_v,float *host_out,const float *host_in,uint32_t frames);
static void native1d1000(void *outer_v,void *input_desc,void *output_desc);
static unsigned long g_c94native;
typedef struct {
    uint64_t count;
    uint64_t stride;
    uint64_t type;
    float **rows;
} NativeF94Rows;
static void native_f94b0(void *self_v,float *processed,float *source){
    uint8_t *self=(uint8_t*)self_v;
    ++g_c94native;
    if(!self||!processed||!source)return;
    uint8_t *format=(uint8_t*)rp376(self,0x08);
    if(!format)return;
    const uint32_t source_rows=rr32(format,0x04);
    const uint32_t processed_rows=rr32(format,0x08);
    const uint32_t expanded_rows=rr32(format,0x0c);
    if(source_rows!=2u||processed_rows!=2u||expanded_rows!=2u||
       *(uint8_t*)(self+0xf8)!=0u||rr32(self,0xfc)!=0u){
        fprintf(stderr,"F94 contract rows=%u/%u/%u expand=%u/%u\n",source_rows,processed_rows,expanded_rows,
                *(uint8_t*)(self+0xf8),rr32(self,0xfc));abort();
    }
    float *source_ptrs[2]={source,source+1};
    float *processed_ptrs[2]={processed,processed+1};
    NativeF94Rows input={2u,2u,7u,source_ptrs};
    NativeF94Rows output={2u,2u,7u,processed_ptrs};
    native1d1000(self,&input,&output);
}

static unsigned long g_c6440native;
static void native_f6440(void *self_v,uint32_t input_count,void *input_desc_pp,uint32_t output_count,void *output_desc_pp){
    uint8_t *self=(uint8_t*)self_v;
    ++g_c6440native;
    if(!self||!input_desc_pp||!output_desc_pp)return;
    uint8_t *input=*(uint8_t**)input_desc_pp;
    uint8_t *output=*(uint8_t**)output_desc_pp;
    if(!input||!output)return;
    const uint32_t frames=rr32(input,0x08);
    const uint32_t flag=rr32(input,0x0c);
    ww32(output,0x08,frames);
    ww32(output,0x0c,flag);
    if(input_count!=1u||output_count!=1u||flag!=1u){fprintf(stderr,"F6440 contract %u/%u flag=%u\n",input_count,output_count,flag);abort();}
    if(rr32(self,0x54)!=0u)ww32(self,0x54,0u);
    float *input_data=(float*)rp376(input,0);
    float *output_data=(float*)rp376(output,0);
    native_f65e0(self,output_data,input_data,frames);
}

static unsigned long g_c65native;
static void native_f65_block(void *context,float *processed,float *source){native_f94b0(context,processed,source);}
static void native_f65e0(void *self_v,float *host_out,const float *host_in,uint32_t frames){
    uint8_t *self=(uint8_t*)self_v;
    ++g_c65native;
    if(!self||!host_out||!host_in||frames==0u)return;
    uint8_t *format=(uint8_t*)rp376(self,0x08);
    UbigStageBRtStream256State stream={
        (float*)rp376(self,0x10),(float*)rp376(self,0x18),rr32(self,0x20),
        format?rr32(format,0x04):0u,format?rr32(format,0x08):0u
    };
    if(stream.source_channels!=2u||stream.processed_channels!=2u){fprintf(stderr,"F65 contract channels %u/%u\n",stream.source_channels,stream.processed_channels);abort();}
    ubig_stage_b_rt_stream256_process(&stream,host_out,host_in,frames,native_f65_block,self);
    ww32(self,0x20,stream.position);
}

static unsigned long g_c1dnative;
static void native1d1000(void *outer_v,void *input_desc,void *output_desc){
 ++g_c1dnative;uint8_t*o=(uint8_t*)outer_v;
 if(!o||!rp376(o,8)||!input_desc||!output_desc)return;
 void *state=rp376(o,0x130);
 if(!state)return;
 const uint32_t mode=rr32(o,0x3c)>>8;
 uint32_t prepared=native31b68_live(state,mode,input_desc,NULL,0u,NULL,NULL,NULL,NULL,NULL,0u,0u);
 uint32_t expected=native_f93a8(o);
 if(prepared!=0u&&prepared==expected){Ret376Native r=native3abe0(state,output_desc,rp376(o,0x30));(void)r;}
}

static unsigned long g_couter_native;
static __attribute__((unused)) void native_outer_hot(void *rt_v,uint32_t input_count,void *input_pp,uint32_t output_count,void *output_pp,void *extra){
    uint8_t *rt=(uint8_t*)rt_v;
    ++g_couter_native;
    if(!rt||!input_pp||!output_pp)return;
    uint8_t *transition=rt+0x78;
    const uint32_t mode=rr32(transition,0);
    if(rt[0x70]!=0u||extra!=NULL||rr32(transition,0x12c048)!=1u||rr32(transition,0x12c02c)!=0u||
       (mode!=0u&&mode!=3u)){
        fprintf(stderr,"HOT contract gate=%u extra=%p f48=%u f2c=%u mode=%u\n",rt[0x70],extra,rr32(transition,0x12c048),rr32(transition,0x12c02c),mode);abort();
    }
    /* Enabled transition wrapper is already in its direct/steady branch on the
       shipped profile set. First entry promotes state 0 -> 3; steady state 3
       clears the dormant transition counter before dispatch. */
    if(mode==3u)ww32(transition,0x12c01c,0u);
    ww32(transition,0,3u);
    void *child=rp376(transition,0x12c040);
    if(!child)abort();
    native_f6440(child,input_count,input_pp,output_count,output_pp);
}

static unsigned long g_c705native;
static __attribute__((unused)) void native705e8_raw(void *state_v){
    ++g_c705native;uint64_t*q=(uint64_t*)state_v;if(!q)return;
    memset(q+0x4c,0,(0x6a-0x4c)*8u);
    uint32_t n=0;memcpy(&n,q+0x6a,4);if(q[0x6b]&&n)memset((void*)(uintptr_t)q[0x6b],0,(size_t)n*0x50u);
    memset(q+0x38,0,(0x4c-0x38)*8u);
    memset((uint8_t*)q+0x354,0,4);
    memset(q+0x28,0,(0x32-0x28)*8u);
    memset(q+0x14,0,(0x28-0x14)*8u);
    memcpy(&n,q+0x32,4);if(q[0x33]&&n)memset((void*)(uintptr_t)q[0x33],0,(size_t)n*0x50u);
    memset(q+0x00,0,0x14u*8u);memset((uint8_t*)q+0x194,0,4);
    memset(q+0x2ec,0,(0x2f6-0x2ec)*8u);memset(q+0x2d8,0,(0x2ec-0x2d8)*8u);
    memcpy(&n,q+0x2f6,4);if(q[0x2f7]&&n)memset((void*)(uintptr_t)q[0x2f7],0,(size_t)n*0x50u);
    memset(q+0x2c4,0,(0x2d8-0x2c4)*8u);memset((uint8_t*)q+0x17b4,0,4);
    memset(q+0x2b4,0,(0x2be - 0x2b4)*8u);memset(q+0x2a0,0,(0x2b4-0x2a0)*8u);
    memcpy(&n,q+0x2be,4);if(q[0x2bf]&&n)memset((void*)(uintptr_t)q[0x2bf],0,(size_t)n*0x50u);
    memset(q+0x28c,0,(0x2a0-0x28c)*8u);memset((uint8_t*)q+0x15f4,0,4);
    memset(q+0x518,0,20u*8u);
}

static void native_vr_inner_ctor(ChainInst *p){
    uint8_t *o=p->vr_inner;
    memset(o,0,0x200u);
    wp376(o,0,NULL);
    {uint64_t one=1u;memcpy(o+0x40,&one,8);}
    /* SharedMemoryControllerAudio base. The deployed Linux bridge never enables
       shared-memory transport; keep its optional controller payload NULL. */
    wp376(o+0x68,0,NULL);
    {uint64_t rate=48000u;memcpy(o+0x108,&rate,8);}
}


typedef int (*VrBaseInitFn)(void*,void*);
typedef int (*VrTuneFn)(void*,void*);
typedef uint32_t (*VrSizeFn)(void*);
typedef void *(*VrArenaAllocFn)(void*,uint32_t,uint64_t);
typedef void *(*VrCoreCreateFn)(void*,void*);
typedef uint32_t (*VrInitDspFn)(void*,uint32_t,uint32_t,uint32_t,uint32_t,void**);
typedef int (*VrAllocBufferFn)(void*);
typedef void (*VrDeleteFn)(void*,uint64_t);
static void *native_vr_arena_alloc(uint8_t *cfg,uint64_t align,uint64_t size){
    uint64_t total=0,base=0,cur=0;memcpy(&total,cfg+0x20,8);memcpy(&base,cfg+0x28,8);memcpy(&cur,cfg+0x30,8);
    if(align>1u){uint64_t rem=cur%align;if(rem)cur+=align-rem;}
    if(!base||cur<base||size>total||cur+size<cur||cur+size>base+total)return NULL;
    uint64_t next=cur+size;memcpy(cfg+0x30,&next,8);return (void*)(uintptr_t)cur;
}


static void wq_core64(uint8_t *b,size_t o,uint64_t v){memcpy(b+o,&v,8);}
typedef struct { const float *primary; const float *secondary; uint32_t count; uint32_t phase; } NativeHistCoeffOwned;
static uint32_t g_owned_hist_primary_bits[22] __attribute__((aligned(32)));
static uint32_t g_owned_hist_secondary_bits[22] __attribute__((aligned(32)));
static uint32_t g_owned_history_filter_bits[640] __attribute__((aligned(32)));
static uint32_t g_owned_history_phase_bits[128] __attribute__((aligned(32)));
static uint32_t g_owned_band_ends[20] __attribute__((aligned(16)));
enum { OWNED_CONTROL_DESC_COUNT=5, OWNED_CONTROL_DESC_BYTES=12+500*16 };
static uint8_t g_owned_control_desc[OWNED_CONTROL_DESC_COUNT][OWNED_CONTROL_DESC_BYTES] __attribute__((aligned(16)));
static int load_private_stageb_pack(void);
static uint32_t g_owned_deep_lane_weight_bits[20] __attribute__((aligned(16)));
static uint8_t g_owned_deep_config[0x90] __attribute__((aligned(16)));
static uint8_t g_owned_deep_config_template[0x90] __attribute__((aligned(16)));
static void init_owned_deep_config(void){
    memcpy(g_owned_deep_config,g_owned_deep_config_template,sizeof g_owned_deep_config);
    wq_core64(g_owned_deep_config,0x80,(uint64_t)(uintptr_t)g_owned_deep_lane_weight_bits);
}
static uint32_t g_owned_feature_boundaries[9] __attribute__((aligned(16)));
static uint8_t g_owned_core_cfg_template[0xc0] __attribute__((aligned(16)));
static uint8_t g_owned_core_cfg[0xc0] __attribute__((aligned(16)));
static const NativeHistCoeffOwned g_owned_hist_coeff={
    (const float*)(const void*)g_owned_hist_primary_bits,
    (const float*)(const void*)g_owned_hist_secondary_bits,11u,3u};
static void init_owned_core_cfg(void){
    memcpy(g_owned_core_cfg,g_owned_core_cfg_template,sizeof g_owned_core_cfg);
    wq_core64(g_owned_core_cfg,0x28,(uint64_t)(uintptr_t)g_owned_history_filter_bits);
    wq_core64(g_owned_core_cfg,0x30,(uint64_t)(uintptr_t)g_owned_history_phase_bits);
    wq_core64(g_owned_core_cfg,0x38,(uint64_t)(uintptr_t)&g_owned_hist_coeff);
    wq_core64(g_owned_core_cfg,0x50,(uint64_t)(uintptr_t)g_owned_band_ends);
    wq_core64(g_owned_core_cfg,0x10,0ULL);
    wq_core64(g_owned_core_cfg,0x18,0ULL);
    wq_core64(g_owned_core_cfg,0x20,0ULL);
    wq_core64(g_owned_core_cfg,0x40,0ULL);
    wq_core64(g_owned_core_cfg,0x48,0ULL);
    wq_core64(g_owned_core_cfg,0x58,0ULL);
    wq_core64(g_owned_core_cfg,0x68,0ULL);
    wq_core64(g_owned_core_cfg,0x70,0ULL);
    wq_core64(g_owned_core_cfg,0x78,0ULL);
    wq_core64(g_owned_core_cfg,0x80,0ULL);
    wq_core64(g_owned_core_cfg,0x88,0ULL);
    wq_core64(g_owned_core_cfg,0x98,0ULL);
    wq_core64(g_owned_core_cfg,0xa0,0ULL);
}

enum { OWNED_CORE_SNAPSHOT_BYTES=298937u };
static uint8_t g_owned_core_snapshot[OWNED_CORE_SNAPSHOT_BYTES] __attribute__((aligned(16)));
static float g_upper_variation_weights[8] __attribute__((aligned(16)));
static uint8_t g_upper_projection_table[19u*0x30u] __attribute__((aligned(16)));
static float g_owned_late_response[5] __attribute__((aligned(16)));
static float g_owned_late_history_kernel[64] __attribute__((aligned(32)));
static uint64_t g_pack_old_core_base,g_pack_old_pe_base;

static int load_private_stageb_pack(void){
    const char *path=getenv("UBIG_SP11_STAGEB_PACK");
    if(!path||!*path)path=getenv("SP11_VR_STAGEB_PACK");
    if(!path||!*path)return -7;
    FILE *f=fopen(path,"rb"); if(!f)return -1;
    uint8_t h[96]; if(fread(h,1,sizeof h,f)!=sizeof h){fclose(f);return -2;}
    if(memcmp(h,"UBGVRP3\0",8)!=0){fclose(f);return -3;}
    uint32_t version=0,core_bytes=0,shape[16];
    memcpy(&version,h+8,4);memcpy(&core_bytes,h+12,4);
    memcpy(&g_pack_old_core_base,h+16,8);memcpy(&g_pack_old_pe_base,h+24,8);memcpy(shape,h+32,sizeof shape);
    const uint32_t expect[16]={0xc0u,22u,22u,640u,128u,20u,9u,0x90u,20u,5u,
                               OWNED_CONTROL_DESC_BYTES,UBIG_STAGE_B_RT_PROJECTION_LUT,
                               8u,19u*0x30u,5u,64u};
    if(version!=3u||core_bytes!=OWNED_CORE_SNAPSHOT_BYTES||!g_pack_old_core_base||!g_pack_old_pe_base||
       memcmp(shape,expect,sizeof shape)!=0){fclose(f);return -4;}
#define RDBUF(x) do{ if(fread((x),1,sizeof(x),f)!=sizeof(x)){fclose(f);return -5;} }while(0)
    RDBUF(g_owned_core_snapshot);
    RDBUF(g_owned_core_cfg_template);
    RDBUF(g_owned_hist_primary_bits);
    RDBUF(g_owned_hist_secondary_bits);
    RDBUF(g_owned_history_filter_bits);
    RDBUF(g_owned_history_phase_bits);
    RDBUF(g_owned_band_ends);
    RDBUF(g_owned_feature_boundaries);
    RDBUF(g_owned_deep_config_template);
    RDBUF(g_owned_deep_lane_weight_bits);
    RDBUF(g_owned_control_desc);
    RDBUF(g_owned_projection_lut);
    RDBUF(g_upper_variation_weights);
    RDBUF(g_upper_projection_table);
    RDBUF(g_owned_late_response);
    RDBUF(g_owned_late_history_kernel);
#undef RDBUF
    if(fgetc(f)!=EOF){fclose(f);return -6;}
    fclose(f);g_projection_lut=g_owned_projection_lut;return 0;
}

static void *native_vr_core_ctor64(ChainInst *p,uint8_t *b){
    (void)p;
    const size_t n=OWNED_CORE_SNAPSHOT_BYTES;
    memcpy(b,g_owned_core_snapshot,n);
    /* The endpoint snapshot is pure data. Rebase allocation-relative pointers;
       PE-relative pointers are either rebound to owner-supplied semantic data
       below or nulled when the source-owned deployed path has proven them dead. */
    for(size_t off=0;off+8u<=n;off+=8u){
        uint64_t qv;memcpy(&qv,b+off,8);
        if(qv>=g_pack_old_core_base&&qv<g_pack_old_core_base+n){
            qv=(uint64_t)(uintptr_t)(b+(qv-g_pack_old_core_base));
            memcpy(b+off,&qv,8);
        }else if(qv>=g_pack_old_pe_base&&qv<g_pack_old_pe_base+0x400000ULL){
            const uint64_t r=qv-g_pack_old_pe_base;
            if(r==0x2694b0ULL)qv=(uint64_t)(uintptr_t)g_owned_feature_boundaries;
            else if(r==0x269520ULL)qv=(uint64_t)(uintptr_t)g_upper_variation_weights;
            else if(r>=0x2686b8ULL&&r<=0x268a18ULL&&((r-0x2686b8ULL)%0x30ULL)==0)
                qv=(uint64_t)(uintptr_t)(g_upper_projection_table+(r-0x2686b0ULL));
            else if(r==0x244d10ULL)qv=(uint64_t)(uintptr_t)g_owned_late_response;
            else if(r==0x245200ULL)qv=(uint64_t)(uintptr_t)g_owned_late_history_kernel;
            else qv=0ULL;
            memcpy(b+off,&qv,8);
        }
    }

    init_owned_core_cfg();
    wq_core64(b,0x28,(uint64_t)(uintptr_t)g_owned_core_cfg);
    wq_core64(b,0xdc0,0ULL);wq_core64(b,0x39a8,0ULL);
    wq_core64(b,0x11028,0ULL);wq_core64(b,0x11030,0ULL);

    /* Universal scheduler caller-owned data. Keep all three upper-analysis
       families faithful rather than relying on an audio-only liveness test. */
    wq_core64(b,0x1d220,(uint64_t)(uintptr_t)g_owned_feature_boundaries);
    wq_core64(b,0x1d700,(uint64_t)(uintptr_t)g_owned_feature_boundaries);
    wq_core64(b,0x1d708,(uint64_t)(uintptr_t)g_upper_variation_weights);
    wq_core64(b,0x1db40,(uint64_t)(uintptr_t)g_owned_feature_boundaries);
    for(uint32_t i=0;i<19u;i++){
        uint32_t start=0,count=0;memcpy(&start,g_upper_projection_table+(size_t)i*0x30u,4);
        memcpy(&count,g_upper_projection_table+(size_t)i*0x30u+4u,4);
        ww32(b,0x1df70u+(size_t)i*16u,start);ww32(b,0x1df74u+(size_t)i*16u,count);
        wq_core64(b,0x1df78u+(size_t)i*16u,
            (uint64_t)(uintptr_t)(g_upper_projection_table+(size_t)i*0x30u+8u));
    }

    wq_core64(b,0x1e5b0,(uint64_t)(uintptr_t)g_owned_control_desc[0]);
    wq_core64(b,0x1e5c0,(uint64_t)(uintptr_t)g_owned_control_desc[1]);
    wq_core64(b,0x1e5d0,(uint64_t)(uintptr_t)g_owned_control_desc[2]);
    wq_core64(b,0x1e5e0,(uint64_t)(uintptr_t)g_owned_control_desc[3]);
    wq_core64(b,0x1e5f0,(uint64_t)(uintptr_t)g_owned_control_desc[4]);
    init_owned_deep_config();
    wq_core64(b,0x414d0,(uint64_t)(uintptr_t)g_owned_deep_config);
    wq_core64(b,0x473a8,(uint64_t)(uintptr_t)g_owned_late_response);
    wq_core64(b,0x473b0,(uint64_t)(uintptr_t)g_owned_late_history_kernel);
    wq_core64(b,0x417a8,0ULL);
    return b;
}

static int native_vr_init_library(ChainInst *p,void *cfg_v){
    uint8_t *o=p->vr_inner,*cfg=(uint8_t*)cfg_v;
    wp376(o,0x08,cfg);
    /* BaseInitLibrary only validates the sample-rate map on this endpoint.
       For 48 kHz the recovered pair is frameA=512, frameB=256. */
    if(rr32(cfg,0)!=48000u)return -10;
    ww32(o,0x38,512u);ww32(o,0x3c,256u);
    ww32(o,0x108,rr32(cfg,0));
    /* cfg+0x10 property store is NULL in the deployed bridge, so the optional
       0x10c/0x110/0x114/0x118 properties remain at their constructor defaults. */
    /* Default headphone tuning is not part of the deployed stereo-speaker path. */
    const uint32_t persistent_size=298937u;
    void *persistent=native_vr_arena_alloc(cfg,4u,persistent_size);
    if(!persistent)return -13;
    wp376(o,0x28,persistent);
    void *core=native_vr_core_ctor64(p,(uint8_t*)persistent);
    if(!core)return -14;
    wp376(o,0x130,core);
    const uint32_t scratch_size=90476u;
    void *scratch=native_vr_arena_alloc(cfg,4u,scratch_size);
    if(!scratch)return -15;
    wp376(o,0x30,scratch);
    /* Fixed deployed InitDSP result: stereo mode 1, no mix matrix. */
    /* Core constructor defaults already match deployed stereo output mode 1. */
    /* Native F65E0 only consumes the 256-frame source/processed ping-pong
       buffers. The reference 38,400-frame delay and intermediate allocations
       are dead after the streamer replacement. */
    void *in=native_vr_arena_alloc(cfg,4u,2048u),*out=native_vr_arena_alloc(cfg,4u,2048u);
    if(!in||!out)return -16;
    memset(in,0,2048u);memset(out,0,2048u);wp376(o,0x10,in);wp376(o,0x18,out);
    return 0;
}
static int native_init_inner(ChainInst *p){
    const size_t arena_sz=8u*1024u*1024u;
    if(posix_memalign((void**)&p->vr_arena,64,arena_sz))return -1;
    memset(p->vr_arena,0,arena_sz);memset(p->vr_cfg,0,sizeof p->vr_cfg);
    ww32(p->vr_cfg,0,48000u);ww32(p->vr_cfg,4,2u);ww32(p->vr_cfg,8,2u);ww32(p->vr_cfg,0xc,2u);
    {uint64_t z=arena_sz;memcpy(p->vr_cfg+0x20,&z,8);z=(uint64_t)(uintptr_t)p->vr_arena;memcpy(p->vr_cfg+0x28,&z,8);memcpy(p->vr_cfg+0x30,&z,8);}
    return native_vr_init_library(p,p->vr_cfg);
}

static int vr_build(ChainInst *p){
    p->vr_initialized=0;
    if(p->vr_arena){free(p->vr_arena);p->vr_arena=NULL;}
    native_vr_inner_ctor(p);
    /* Source-owned cold graph: no constructor entry or resource callback. */
    /* No mapped VR setup code is entered. */
    if(native_init_inner(p))return -2;
    if(vr_apply_profile(p,p->vr_inner))return -5;
    g_c705native=0;g_couter_native=0;g_c94native=0;g_c6440native=0;g_c65native=0;g_c1dnative=0;g_c30native=g_c2fref=0;g_c31native=g_cf93native=0;g_c3anative=g_c3awrap=g_c425=g_c425blocks=0;g_c5cenative=0;g_c4a570=g_c5bc98=g_c5c6d0=g_c45288=g_c5ad38=0;g_c376native=0;g_c60200=g_c596e0=g_c54a48=g_c5f5a8=0;g_c602fallback=g_c5ffallback=g_c558_native=g_c4bab_native=0;g_c56b80=g_c56fallback=0;g_c54native=g_c54fallback=0;g_c34native=g_c34fallback=0;g_c347native=g_c456native=g_c4f1native=g_cq31native=0;g_c7b2f0=g_c7bnative=g_c7bfallback=g_c9cbnative=g_c9cbfallback=g_c8cnative=g_c8cfallback=g_cuppernative=g_cloweranative=g_clowerbnative=g_c584native=g_c584fallback=0;
    /* Leveler tuning tables are unreachable on the shipped source-owned route: the wrapper returns before tuning use. */
    /* Multiband tuning tables are unreachable because the deployed native parent does not enter sib54native. */
    /* Direct-inner experiment: the exact native stream and frame processors
       consume LibWrapperVr directly. The 3.75 MiB outer APO object and its
       0x1B96B8 transition wrapper are not constructed or initialized. */
    p->vr_initialized=1;
    return 0;
}

static int chain_alloc(ChainInst *p){
    if(posix_memalign((void**)&p->vr_inner,64,0x200u)||
       posix_memalign((void**)&p->buf_a,64,RT_CHUNK_FRAMES*2*sizeof(float))||
       posix_memalign((void**)&p->buf_b,64,RT_CHUNK_FRAMES*2*sizeof(float))||
       posix_memalign((void**)&p->native_stage_a_l,64,RT_CHUNK_FRAMES*sizeof(float))||
       posix_memalign((void**)&p->native_stage_a_r,64,RT_CHUNK_FRAMES*sizeof(float))) return -1;
    return 0;
}

static void chain_free_mem(ChainInst *p){
    if(p->control_ready){ubig_control_close(&p->control);p->control_ready=0;}
    free(p->native_stage_a_r);free(p->native_stage_a_l);free(p->buf_b);free(p->buf_a);free(p->vr_arena);free(p->vr_inner);
}

static LADSPA_Handle chain_instantiate(const LADSPA_Descriptor*d,unsigned long rate){
    (void)d;if(rate!=48000)return NULL;
    ChainInst *p=calloc(1,sizeof(*p));if(!p)return NULL;
    p->profile=chain_profile_from_env();
    p->last_profile_request=-2;
    p->control.fd=-1;
    if(chain_alloc(p)){chain_free_mem(p);free(p);return NULL;}

    ubig_engine_config native_cfg={UBIG_ABI_VERSION,UBIG_SAMPLE_RATE,UBIG_CHANNELS,(ubig_profile)p->profile};
    p->native_stage_a=ubig_engine_create(&native_cfg);
    if(!p->native_stage_a)goto fail;

    if(load_private_stageb_pack()) goto fail;
    if(vr_build(p))goto fail;
    p->ready=1;
    {int control_rc=chain_control_open(p);if(control_rc<0)fprintf(stderr,"ubig-sp11: runtime control unavailable; LADSPA startup/profile port only\n");}
    return p;
fail:
    if(p->native_stage_a)ubig_engine_destroy(p->native_stage_a);
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
    p->ready=(p->native_stage_a!=NULL && vr_build(p)==0);
}

static void chain_run(LADSPA_Handle h,unsigned long n){
    ChainInst*p=h;const float*il=p->ports[0],*ir=p->ports[1];float*ol=p->ports[2],*or=p->ports[3];
    if(!il||!ir||!ol||!or)return;
    if(!p->ready){for(unsigned long i=0;i<n;i++){ol[i]=il[i];or[i]=ir[i];}return;}
    (void)chain_apply_control_request(p);
    int requested=chain_profile_code_from_port(p->ports[PORT_PROFILE]);
    if(requested>=0&&requested!=(int)p->profile&&requested!=p->last_profile_request){
        p->last_profile_request=requested;(void)chain_apply_profile_inplace(p,(ChainProfile)requested);
    }else if(requested==(int)p->profile)p->last_profile_request=requested;
    int dry=p->ports[PORT_BYPASS]&&*p->ports[PORT_BYPASS]>.5f;
    unsigned long pos=0;
    while(pos<n){
        uint32_t take=(uint32_t)((n-pos)>RT_CHUNK_FRAMES?RT_CHUNK_FRAMES:(n-pos));
        if(ubig_engine_process(p->native_stage_a,il+pos,ir+pos,p->native_stage_a_l,p->native_stage_a_r,take)!=UBIG_OK){p->ready=0;for(uint32_t i=0;i<take;i++)p->native_stage_a_l[i]=p->native_stage_a_r[i]=0.0f;}
        for(uint32_t i=0;i<take;i++){p->buf_b[2*i]=p->native_stage_a_l[i];p->buf_b[2*i+1]=p->native_stage_a_r[i];p->buf_a[2*i]=p->buf_a[2*i+1]=0.0f;}
        Conn ri={p->buf_b,take,1},ro={p->buf_a,0,0};Conn *rip=&ri,*rop=&ro;
        native_f6440(p->vr_inner,1,&rip,1,&rop);
        if(ro.flags==2)memset(p->buf_a,0,(size_t)take*2*sizeof(float));
        for(uint32_t i=0;i<take;i++){
            if(dry){ol[pos+i]=il[pos+i];or[pos+i]=ir[pos+i];}
            else {ol[pos+i]=p->buf_a[2*i];or[pos+i]=p->buf_a[2*i+1];}
        }
        pos+=take;
    }
}

static void chain_cleanup(LADSPA_Handle h){
    ChainInst*p=h;if(!p)return;
    if(p->native_stage_a)ubig_engine_destroy(p->native_stage_a);
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
    memset(&desc,0,sizeof(desc));desc.UniqueID=0x55424947;desc.Label="ubig_sp11_candidate";desc.Name="UbiG SP11 Native Candidate";
    desc.Maker="UbiG";desc.Copyright="source-owned DSP; endpoint data supplied separately";
    desc.PortCount=PORT_COUNT;desc.PortDescriptors=pd;desc.PortNames=pn;desc.PortRangeHints=ph;
    desc.instantiate=chain_instantiate;desc.connect_port=chain_connect;desc.activate=chain_activate;desc.run=chain_run;desc.cleanup=chain_cleanup;
    return &desc;
}
