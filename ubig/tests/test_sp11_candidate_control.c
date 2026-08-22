#define _GNU_SOURCE
#include "ubig/ubig_control.h"
#include <dlfcn.h>
#include <ladspa.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef const LADSPA_Descriptor *(*DescriptorFn)(unsigned long);

typedef struct {
    const LADSPA_Descriptor *descriptor;
    LADSPA_Handle handle;
    float bypass;
    float profile;
} CandidateInstance;

static uint32_t rng_state=UINT32_C(0x13579bdf);

static float noise_sample(void)
{
    rng_state=rng_state*UINT32_C(1664525)+UINT32_C(1013904223);
    return ((float)((rng_state>>8)&UINT32_C(0xffffff))/8388608.0f)-1.0f;
}

static void make_input(float *left,float *right,size_t count,size_t base)
{
    for(size_t i=0;i<count;i++){
        const double t=(double)(base+i)/48000.0;
        const float noise=0.004f*noise_sample();
        left[i]=0.17f*sinf((float)(2.0*M_PI*733.0*t))+
                0.05f*sinf((float)(2.0*M_PI*137.0*t))+noise;
        right[i]=0.13f*sinf((float)(2.0*M_PI*977.0*t))+
                 0.04f*sinf((float)(2.0*M_PI*211.0*t))-0.6f*noise;
    }
}

static int snapshot_matches(ubig_control_handle *handle,uint32_t generation,uint32_t postgain_generation,ubig_profile active,int32_t postgain)
{
    ubig_control_page page;
    if(ubig_control_snapshot(handle,&page)!=UBIG_OK)return -1;
    if(page.request_generation!=generation||page.ack_generation!=generation||
       page.postgain_request_generation!=postgain_generation||page.postgain_ack_generation!=postgain_generation||
       page.active_profile!=(uint32_t)active||page.active_postgain!=postgain||page.last_error!=0)return -2;
    return 0;
}

static int create_instance(void *library,const char *control_path,const char *pack_path,
                           CandidateInstance *instance)
{
    if(setenv("UBIG_CONTROL_PATH",control_path,1)||
       setenv("UBIG_SP11_STAGEB_PACK",pack_path,1)||
       setenv("UBIG_PROFILE","dynamic",1)||setenv("UBIG_GEQ","flat",1))return -1;
    DescriptorFn descriptor_fn=(DescriptorFn)dlsym(library,"ladspa_descriptor");
    instance->descriptor=descriptor_fn?descriptor_fn(0):NULL;
    if(!instance->descriptor)return -2;
    instance->handle=instance->descriptor->instantiate(instance->descriptor,48000);
    if(!instance->handle)return -3;
    instance->bypass=0.0f;
    instance->profile=0.0f;
    instance->descriptor->connect_port(instance->handle,4,&instance->bypass);
    instance->descriptor->connect_port(instance->handle,5,&instance->profile);
    if(instance->descriptor->activate)instance->descriptor->activate(instance->handle);
    return 0;
}

static void run_instance(CandidateInstance *instance,const float *left,const float *right,
                         float *out_left,float *out_right,size_t frames)
{
    instance->descriptor->connect_port(instance->handle,0,(float*)left);
    instance->descriptor->connect_port(instance->handle,1,(float*)right);
    instance->descriptor->connect_port(instance->handle,2,out_left);
    instance->descriptor->connect_port(instance->handle,3,out_right);
    instance->descriptor->run(instance->handle,frames);
}

static size_t count_differences(const float *a,const float *b,size_t count)
{
    size_t differences=0;
    for(size_t i=0;i<count;i++)differences+=(a[i]!=b[i]);
    return differences;
}

int main(int argc,char **argv)
{
    if(argc!=3){fprintf(stderr,"usage: %s CANDIDATE_SO PRIVATE_PACK\n",argv[0]);return 2;}
    char path_a[128],path_b[128];
    snprintf(path_a,sizeof path_a,"/tmp/ubig-control-a-%ld",(long)getpid());
    snprintf(path_b,sizeof path_b,"/tmp/ubig-control-b-%ld",(long)getpid());
    unlink(path_a);unlink(path_b);

    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"%s\n",dlerror());return 3;}
    CandidateInstance a={0},b={0};
    if(create_instance(library,path_a,argv[2],&a)||create_instance(library,path_b,argv[2],&b))return 4;

    ubig_control_handle control_a,control_b;
    if(ubig_control_open(&control_a,path_a,0)||ubig_control_open(&control_b,path_b,0))return 5;
    if(snapshot_matches(&control_a,0,0,UBIG_PROFILE_DYNAMIC,0)||snapshot_matches(&control_b,0,0,UBIG_PROFILE_DYNAMIC,0))return 6;

    enum { FRAMES=480 };
    float left[FRAMES],right[FRAMES],a_left[FRAMES],a_right[FRAMES],b_left[FRAMES],b_right[FRAMES];
    size_t base=0;
    make_input(left,right,FRAMES,base);
    run_instance(&a,left,right,a_left,a_right,FRAMES);
    run_instance(&b,left,right,b_left,b_right,FRAMES);
    if(count_differences(a_left,b_left,FRAMES)||count_differences(a_right,b_right,FRAMES))return 7;
    base+=FRAMES;

    if(ubig_control_request_profile(&control_a,UBIG_PROFILE_MOVIE)||
       ubig_control_request_profile(&control_b,UBIG_PROFILE_MOVIE))return 8;
    make_input(left,right,FRAMES,base);
    run_instance(&a,left,right,a_left,a_right,FRAMES);
    run_instance(&b,left,right,b_left,b_right,FRAMES);
    if(snapshot_matches(&control_a,1,0,UBIG_PROFILE_MOVIE,0)||snapshot_matches(&control_b,1,0,UBIG_PROFILE_MOVIE,0)||
       count_differences(a_left,b_left,FRAMES)||count_differences(a_right,b_right,FRAMES))return 9;
    base+=FRAMES;

    const int32_t eq_a[UBIG_EQ_BANDS]={-192,-160,-128,-96,-64,-32,0,32,64,96,128,160,192,160,128,96,64,32,0,-32};
    const int32_t eq_b[UBIG_EQ_BANDS]={192,160,128,96,64,32,0,-32,-64,-96,-128,-160,-192,-160,-128,-96,-64,-32,0,32};
    if(ubig_control_request_custom_eq(&control_a,eq_a)||ubig_control_request_custom_eq(&control_b,eq_a))return 10;
    make_input(left,right,FRAMES,base);
    run_instance(&a,left,right,a_left,a_right,FRAMES);
    run_instance(&b,left,right,b_left,b_right,FRAMES);
    if(snapshot_matches(&control_a,2,0,UBIG_PROFILE_CUSTOM,0)||snapshot_matches(&control_b,2,0,UBIG_PROFILE_CUSTOM,0)||
       count_differences(a_left,b_left,FRAMES)||count_differences(a_right,b_right,FRAMES))return 11;
    base+=FRAMES;

    if(ubig_control_request_custom_eq(&control_a,eq_a)||ubig_control_request_custom_eq(&control_b,eq_b))return 12;
    make_input(left,right,FRAMES,base);
    run_instance(&a,left,right,a_left,a_right,FRAMES);
    run_instance(&b,left,right,b_left,b_right,FRAMES);
    if(snapshot_matches(&control_a,3,0,UBIG_PROFILE_CUSTOM,0)||snapshot_matches(&control_b,3,0,UBIG_PROFILE_CUSTOM,0))return 13;
    const size_t changed=count_differences(a_left,b_left,FRAMES)+count_differences(a_right,b_right,FRAMES);
    if(!changed)return 14;

    char path_c[128],path_d[128];
    snprintf(path_c,sizeof path_c,"/tmp/ubig-control-c-%ld",(long)getpid());
    snprintf(path_d,sizeof path_d,"/tmp/ubig-control-d-%ld",(long)getpid());
    unlink(path_c);unlink(path_d);
    CandidateInstance c={0},d={0};
    if(create_instance(library,path_c,argv[2],&c)||create_instance(library,path_d,argv[2],&d))return 15;
    ubig_control_handle control_c,control_d;
    if(ubig_control_open(&control_c,path_c,0)||ubig_control_open(&control_d,path_d,0))return 16;
    if(ubig_control_request_postgain(&control_c,-332)||ubig_control_request_postgain(&control_d,0))return 17;
    size_t postgain_changed=0;
    for(unsigned block=0;block<12u;block++){
        make_input(left,right,FRAMES,(size_t)block*FRAMES);
        run_instance(&c,left,right,a_left,a_right,FRAMES);
        run_instance(&d,left,right,b_left,b_right,FRAMES);
        postgain_changed+=count_differences(a_left,b_left,FRAMES)+count_differences(a_right,b_right,FRAMES);
    }
    if(snapshot_matches(&control_c,0,1,UBIG_PROFILE_DYNAMIC,-332)||snapshot_matches(&control_d,0,1,UBIG_PROFILE_DYNAMIC,0))return 18;
    if(!postgain_changed)return 19;

    printf("PASS SP11 candidate public control lifecycle eq_changed=%zu/%u postgain_changed=%zu/%u\n",
           changed,2u*FRAMES,postgain_changed,24u*FRAMES);
    ubig_control_close(&control_c);ubig_control_close(&control_d);
    c.descriptor->cleanup(c.handle);d.descriptor->cleanup(d.handle);unlink(path_c);unlink(path_d);
    ubig_control_close(&control_a);ubig_control_close(&control_b);
    a.descriptor->cleanup(a.handle);b.descriptor->cleanup(b.handle);
    unlink(path_a);unlink(path_b);dlclose(library);
    return 0;
}
