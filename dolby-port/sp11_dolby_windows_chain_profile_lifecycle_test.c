#define _GNU_SOURCE
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Include the production bridge so the regression can inspect the exact
 * state/pointer identities that must survive a live profile retune. */
#include "sp11_dolby_windows_chain_ladspa.c"

static uint32_t rng_state=0x91e10da5u;
static float noise_sample(void){
    rng_state=rng_state*1664525u+1013904223u;
    return ((float)((rng_state>>8)&0xffffffu)/(float)0x800000u)-1.0f;
}
static uint32_t bitsf(float x){uint32_t u;memcpy(&u,&x,4);return u;}
static int process(ChainInst *p,float *profile,uint64_t frames){
    float il[480],ir[480],ol[480],or_[480],bypass=0.0f;
    p->ports[PORT_BYPASS]=&bypass;p->ports[PORT_PROFILE]=profile;
    uint64_t pos=0;
    while(pos<frames){
        unsigned n=(unsigned)((frames-pos)>480?480:(frames-pos));
        for(unsigned i=0;i<n;i++){
            double t=(double)(pos+i)/48000.0;float z=.025f*noise_sample();
            il[i]=.17f*sinf((float)(2*M_PI*113*t))+.11f*sinf((float)(2*M_PI*997*t))+z;
            ir[i]=.14f*sinf((float)(2*M_PI*181*t))+.09f*sinf((float)(2*M_PI*701*t))-.7f*z;
            ol[i]=or_[i]=0.0f;
        }
        p->ports[PORT_IN_L]=il;p->ports[PORT_IN_R]=ir;p->ports[PORT_OUT_L]=ol;p->ports[PORT_OUT_R]=or_;
        chain_run(p,n);
        for(unsigned i=0;i<n;i++)if(!isfinite(ol[i])||!isfinite(or_[i]))return -1;
        pos+=n;
    }
    return 0;
}
static int apply_control_without_audio(ChainInst *p,float *profile){
    float x=0.0f,y=0.0f;
    p->ports[PORT_IN_L]=&x;p->ports[PORT_IN_R]=&x;p->ports[PORT_OUT_L]=&y;p->ports[PORT_OUT_R]=&y;
    p->ports[PORT_PROFILE]=profile;
    chain_run(p,0);
    return 0;
}
static int request_profile(ChainInst *p,float *profile,ChainProfile wanted){
    if(!p->profile_control)return -1;
    uint8_t code=(uint8_t)wanted+1u;
    __atomic_store_n(p->profile_control,code,__ATOMIC_RELEASE);
    if(apply_control_without_audio(p,profile))return -2;
    return __atomic_load_n(p->profile_control+1,__ATOMIC_ACQUIRE)==code?0:-3;
}

int main(void){
    const char *home=getenv("HOME");char vl[512],vr[512];
    if(!home)return 2;
    snprintf(vl,sizeof(vl),"%s/.local/lib/sp11-dolby/DolbyAPOvlldp150.dll",home);
    snprintf(vr,sizeof(vr),"%s/.local/lib/sp11-dolby/DolbyAPOVR.dll",home);
    setenv("SP11_VLLDP_DLL",vl,1);setenv("SP11_VR_DLL",vr,1);
    setenv("SP11_DOLBY_PROFILE","dynamic",1);setenv("SP11_DOLBY_GEQ","off",1);
    char ctl[160];snprintf(ctl,sizeof(ctl),"/tmp/sp11-dolby-profile-lifecycle-%ld.control",(long)getpid());
    unlink(ctl);setenv("SP11_DOLBY_CONTROL_PATH",ctl,1);

    ChainInst *p=(ChainInst*)chain_instantiate(NULL,48000);if(!p)return 3;
    float profile=0.0f;
    const size_t state_off=0x1F1768u;
    float initial;memcpy(&initial,p->vr_outer+state_off,4);
    if(process(p,&profile,12ULL*48000ULL))return 4;
    float warm;memcpy(&warm,p->vr_outer+state_off,4);

    void *outer_before=p->vr_outer;
    uint8_t *inner_before=(uint8_t*)(uintptr_t)q(p->vr_outer,VR_INNER_PTR_OFF);
    void *vr_core_before=(void*)(uintptr_t)q(inner_before,0x130);
    void *vl_core_before=(void*)(uintptr_t)vl_r64(p->vl_inner,0x28);
    int amount_before=(int)d(vr_core_before,0x6d4);

    if(request_profile(p,&profile,CHAIN_PROFILE_MUSIC))return 6;

    uint8_t *inner_after=(uint8_t*)(uintptr_t)q(p->vr_outer,VR_INNER_PTR_OFF);
    void *vr_core_after=(void*)(uintptr_t)q(inner_after,0x130);
    void *vl_core_after=(void*)(uintptr_t)vl_r64(p->vl_inner,0x28);
    float switched;memcpy(&switched,p->vr_outer+state_off,4);
    int amount_after=(int)d(vr_core_after,0x6d4);

    int identity=(outer_before==p->vr_outer && inner_before==inner_after &&
                  vr_core_before==vr_core_after && vl_core_before==vl_core_after);
    int state_preserved=(bitsf(warm)==bitsf(switched));
    int profile_applied=(p->profile==CHAIN_PROFILE_MUSIC && amount_before==5 && amount_after==0);

    /* Exercise every recovered profile as a zero-audio parameter transaction.
     * None is allowed to reconstruct the Dolby objects or perturb the adaptive
     * long-memory state merely because tuning changed. */
    static const ChainProfile sweep[]={CHAIN_PROFILE_MOVIE,CHAIN_PROFILE_MUSIC,CHAIN_PROFILE_GAME,
        CHAIN_PROFILE_VOICE,CHAIN_PROFILE_ONLINECOURSE,CHAIN_PROFILE_PERSONALIZE,CHAIN_PROFILE_DYNAMIC,CHAIN_PROFILE_MUSIC};
    int sweep_ok=1;
    for(size_t i=0;i<sizeof(sweep)/sizeof(sweep[0]);i++){
        float before;memcpy(&before,p->vr_outer+state_off,4);
        if(request_profile(p,&profile,sweep[i]))return 7;
        float after;memcpy(&after,p->vr_outer+state_off,4);
        uint8_t *ii=(uint8_t*)(uintptr_t)q(p->vr_outer,VR_INNER_PTR_OFF);
        void *vc=(void*)(uintptr_t)q(ii,0x130),*lc=(void*)(uintptr_t)vl_r64(p->vl_inner,0x28);
        int amount=(int)d(vc,0x6d4);
        int one=(p->profile==sweep[i] && bitsf(before)==bitsf(after) && p->vr_outer==outer_before &&
                 ii==inner_before && vc==vr_core_before && lc==vl_core_before && amount==chain_profiles[sweep[i]].leveler_amount);
        printf("sweep_profile=%s state_bits=%08"PRIx32" amount=%d result=%s\n",
               chain_profiles[sweep[i]].name,bitsf(after),amount,one?"PASS":"FAIL");
        if(!one)sweep_ok=0;
    }
    profile=0.0f;

    printf("initial_long_memory=%.9f bits=%08"PRIx32"\n",initial,bitsf(initial));
    printf("warmed_long_memory=%.9f bits=%08"PRIx32"\n",warm,bitsf(warm));
    printf("switched_long_memory=%.9f bits=%08"PRIx32"\n",switched,bitsf(switched));
    printf("vr_outer_before=%p after=%p\n",outer_before,p->vr_outer);
    printf("vr_core_before=%p after=%p\n",vr_core_before,vr_core_after);
    printf("vl_core_before=%p after=%p\n",vl_core_before,vl_core_after);
    printf("leveler_amount_before=%d after=%d\n",amount_before,amount_after);
    printf("identity=%s state_preserved=%s profile_applied=%s\n",
           identity?"YES":"NO",state_preserved?"YES":"NO",profile_applied?"YES":"NO");

    if(process(p,&profile,48000ULL))return 5;
    float after_audio;memcpy(&after_audio,p->vr_outer+state_off,4);
    printf("music_after_1s_long_memory=%.9f bits=%08"PRIx32"\n",after_audio,bitsf(after_audio));

    float before_return;memcpy(&before_return,p->vr_outer+state_off,4);
    if(request_profile(p,&profile,CHAIN_PROFILE_DYNAMIC))return 8;
    float after_return;memcpy(&after_return,p->vr_outer+state_off,4);
    int return_preserved=bitsf(before_return)==bitsf(after_return) && p->profile==CHAIN_PROFILE_DYNAMIC;
    printf("return_dynamic_long_memory_before=%.9f after=%.9f preserved=%s\n",
           before_return,after_return,return_preserved?"YES":"NO");

    chain_cleanup(p);unlink(ctl);

    /* PipeWire can instantiate the graph lazily. A request created by the
     * helper before instantiate must survive open/ftruncate and be applied on
     * the first process cycle. */
    char ctl2[160];snprintf(ctl2,sizeof(ctl2),"/tmp/sp11-dolby-profile-prequeue-%ld.control",(long)getpid());
    unlink(ctl2);int fd=open(ctl2,O_RDWR|O_CREAT|O_CLOEXEC,0600);
    uint8_t queued[2]={(uint8_t)CHAIN_PROFILE_MUSIC+1u,0};
    int prequeue_ok=0;
    if(fd>=0 && ftruncate(fd,2)==0 && pwrite(fd,queued,2,0)==2){
        close(fd);setenv("SP11_DOLBY_CONTROL_PATH",ctl2,1);setenv("SP11_DOLBY_PROFILE","dynamic",1);
        ChainInst *qinst=(ChainInst*)chain_instantiate(NULL,48000);
        if(qinst){
            float sentinel=0.0f;apply_control_without_audio(qinst,&sentinel);
            prequeue_ok=(qinst->profile==CHAIN_PROFILE_MUSIC && qinst->profile_control &&
                __atomic_load_n(qinst->profile_control,__ATOMIC_ACQUIRE)==3 &&
                __atomic_load_n(qinst->profile_control+1,__ATOMIC_ACQUIRE)==3);
            chain_cleanup(qinst);
        }
    } else if(fd>=0) close(fd);
    unlink(ctl2);
    printf("preinstantiate_queue=%s\n",prequeue_ok?"PASS":"FAIL");

    int ok=identity&&state_preserved&&profile_applied&&sweep_ok&&return_preserved&&prequeue_ok;
    printf("PROFILE_LIFECYCLE_RESULT %s\n",ok?"PASS":"FAIL");
    return ok?0:20;
}
