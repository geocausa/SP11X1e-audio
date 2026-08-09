#define _GNU_SOURCE
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "sp11_dolby_windows_chain_ladspa.c"

static uint32_t bitsf(float x){uint32_t u;memcpy(&u,&x,4);return u;}
static void control_only(ChainInst *p){
    float x=0.0f,y=0.0f,profile=0.0f,bypass=0.0f;
    p->ports[PORT_IN_L]=&x;p->ports[PORT_IN_R]=&x;
    p->ports[PORT_OUT_L]=&y;p->ports[PORT_OUT_R]=&y;
    p->ports[PORT_PROFILE]=&profile;p->ports[PORT_BYPASS]=&bypass;
    chain_run(p,0);
}
static void request_postgain(ChainInst *p,int32_t v){
    volatile int32_t *slot=(volatile int32_t*)(p->profile_control+POSTGAIN_CONTROL_REQUEST_OFF);
    __atomic_store_n(slot,v,__ATOMIC_RELEASE);
    control_only(p);
}
static int32_t ack_postgain(ChainInst *p){
    volatile int32_t *slot=(volatile int32_t*)(p->profile_control+POSTGAIN_CONTROL_ACK_OFF);
    return __atomic_load_n(slot,__ATOMIC_ACQUIRE);
}

int main(void){
    const char *home=getenv("HOME");if(!home)return 2;
    char vl[512],vr[512],ctl[160];
    snprintf(vl,sizeof(vl),"%s/.local/lib/sp11-dolby/DolbyAPOvlldp150.dll",home);
    snprintf(vr,sizeof(vr),"%s/.local/lib/sp11-dolby/DolbyAPOVR.dll",home);
    snprintf(ctl,sizeof(ctl),"/tmp/sp11-dolby-postgain-%ld.control",(long)getpid());
    unlink(ctl);
    setenv("SP11_VLLDP_DLL",vl,1);setenv("SP11_VR_DLL",vr,1);
    setenv("SP11_DOLBY_PROFILE","dynamic",1);setenv("SP11_DOLBY_CONTROL_PATH",ctl,1);

    ChainInst *p=(ChainInst*)chain_instantiate(NULL,48000);if(!p)return 3;
    if(!p->profile_control)return 4;
    const size_t state_off=0x1F1768u;
    float state_before;memcpy(&state_before,p->vr_outer+state_off,4);
    void *outer=p->vr_outer;
    uint8_t *inner=(uint8_t*)(uintptr_t)q(p->vr_outer,VR_INNER_PTR_OFF);
    void *vr_core=(void*)(uintptr_t)q(inner,0x130);
    void *vl_core=(void*)(uintptr_t)vl_r64(p->vl_inner,0x28);

    request_postgain(p,-423);
    float state_after;memcpy(&state_after,p->vr_outer+state_off,4);
    uint32_t coeff_bits=d(vl_core,0x65c);
    int first_ok=(p->current_postgain==-423 && ack_postgain(p)==-423 &&
                  *(int32_t*)((uint8_t*)vl_core+0xbb0)==-423 &&
                  *(int32_t*)((uint8_t*)vl_core+0xbb4)==-423 &&
                  coeff_bits==0xbe503f04u &&
                  p->vr_outer==outer &&
                  (uint8_t*)(uintptr_t)q(p->vr_outer,VR_INNER_PTR_OFF)==inner &&
                  (void*)(uintptr_t)q(inner,0x130)==vr_core &&
                  (void*)(uintptr_t)vl_r64(p->vl_inner,0x28)==vl_core &&
                  bitsf(state_before)==bitsf(state_after));
    printf("apply_-423 ack=%d bb0=%d bb4=%d coeff=%08"PRIx32" state=%08"PRIx32" identity=%s result=%s\n",
           ack_postgain(p),*(int32_t*)((uint8_t*)vl_core+0xbb0),*(int32_t*)((uint8_t*)vl_core+0xbb4),
           coeff_bits,bitsf(state_after),p->vr_outer==outer?"YES":"NO",first_ok?"PASS":"FAIL");

    request_postgain(p,0);
    float state_zero;memcpy(&state_zero,p->vr_outer+state_off,4);
    int zero_ok=(p->current_postgain==0 && ack_postgain(p)==0 &&
                 *(int32_t*)((uint8_t*)vl_core+0xbb0)==0 &&
                 *(int32_t*)((uint8_t*)vl_core+0xbb4)==0 && d(vl_core,0x65c)==0 &&
                 bitsf(state_after)==bitsf(state_zero) && p->vr_outer==outer &&
                 (void*)(uintptr_t)vl_r64(p->vl_inner,0x28)==vl_core);
    printf("restore_0 ack=%d bb0=%d bb4=%d coeff=%08"PRIx32" state=%08"PRIx32" result=%s\n",
           ack_postgain(p),*(int32_t*)((uint8_t*)vl_core+0xbb0),*(int32_t*)((uint8_t*)vl_core+0xbb4),
           d(vl_core,0x65c),bitsf(state_zero),zero_ok?"PASS":"FAIL");
    chain_cleanup(p);unlink(ctl);

    /* Queue postgain before lazy graph instantiation. The control-page open
     * path must preserve it exactly like the existing profile request slot. */
    char ctl2[160];snprintf(ctl2,sizeof(ctl2),"/tmp/sp11-dolby-postgain-prequeue-%ld.control",(long)getpid());
    unlink(ctl2);int fd=open(ctl2,O_RDWR|O_CREAT|O_CLOEXEC,0600);int pre_ok=0;
    if(fd>=0){
        uint8_t page[PROFILE_CONTROL_BYTES]={0};
        int32_t none=POSTGAIN_CONTROL_NONE,queued=-385;
        memcpy(page+POSTGAIN_CONTROL_REQUEST_OFF,&queued,4);
        memcpy(page+POSTGAIN_CONTROL_ACK_OFF,&none,4);
        if(ftruncate(fd,PROFILE_CONTROL_BYTES)==0 && pwrite(fd,page,sizeof(page),0)==(ssize_t)sizeof(page)){
            close(fd);setenv("SP11_DOLBY_CONTROL_PATH",ctl2,1);
            ChainInst*qinst=(ChainInst*)chain_instantiate(NULL,48000);
            if(qinst){control_only(qinst);void*qc=(void*)(uintptr_t)vl_r64(qinst->vl_inner,0x28);
                pre_ok=(qinst->current_postgain==-385 && ack_postgain(qinst)==-385 &&
                    *(int32_t*)((uint8_t*)qc+0xbb0)==-385 && *(int32_t*)((uint8_t*)qc+0xbb4)==-385);
                chain_cleanup(qinst);
            }
        } else close(fd);
    }
    /* cleanup must not unlink the runtime page: the monitor may have queued
     * state for the next filter-chain instance before restart. */
    int page_survived=access(ctl2,F_OK)==0;
    unlink(ctl2);
    printf("preinstantiate_postgain_queue=%s runtime_page_survives_cleanup=%s\n",
           pre_ok?"PASS":"FAIL",page_survived?"PASS":"FAIL");

    char ctl3[160];snprintf(ctl3,sizeof(ctl3),"/tmp/sp11-dolby-postgain-safe-default-%ld.control",(long)getpid());
    unlink(ctl3);setenv("SP11_DOLBY_CONTROL_PATH",ctl3,1);
    ChainInst *safe=(ChainInst*)chain_instantiate(NULL,48000);int safe_default_ok=0;
    if(safe){
        void *sc=(void*)(uintptr_t)vl_r64(safe->vl_inner,0x28);
        safe_default_ok=(safe->current_postgain==POSTGAIN_CONTROL_MIN &&
            *(int32_t*)((uint8_t*)sc+0xbb0)==POSTGAIN_CONTROL_MIN &&
            *(int32_t*)((uint8_t*)sc+0xbb4)==POSTGAIN_CONTROL_MIN);
        chain_cleanup(safe);
    }
    unlink(ctl3);
    printf("unknown_endpoint_volume_safe_default=%s\n",safe_default_ok?"PASS":"FAIL");
    int ok=first_ok&&zero_ok&&pre_ok&&page_survived&&safe_default_ok;
    printf("POSTGAIN_CONTROL_RESULT %s\n",ok?"PASS":"FAIL");
    return ok?0:20;
}
