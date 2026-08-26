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

static void make_input(float *left,float *right,size_t count,size_t base)
{
    for(size_t i=0;i<count;i++){
        const double t=(double)(base+i)/48000.0;
        const double sweep=(180.0+1700.0*t/2.0)*t;
        left[i]=0.19f*sinf((float)(2.0*M_PI*137.0*t))+
                0.15f*sinf((float)(2.0*M_PI*733.0*t))+
                0.06f*sinf((float)(2.0*M_PI*sweep));
        right[i]=0.17f*sinf((float)(2.0*M_PI*211.0*t))+
                 0.13f*sinf((float)(2.0*M_PI*977.0*t))+
                 0.05f*sinf((float)(2.0*M_PI*(sweep+113.0)));
    }
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
    instance->bypass=0.0f;instance->profile=0.0f;
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

static uint64_t hash_float(uint64_t h,float value)
{
    uint32_t bits;memcpy(&bits,&value,sizeof bits);
    for(unsigned i=0;i<4;i++){h^=(bits>>(8u*i))&0xffu;h*=UINT64_C(1099511628211);}
    return h;
}

int main(int argc,char **argv)
{
    if(argc!=3){fprintf(stderr,"usage: %s CANDIDATE_SO PRIVATE_PACK\n",argv[0]);return 2;}
    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"%s\n",dlerror());return 3;}

    enum { FRAMES=480, WARM_BLOCKS=8, TEST_BLOCKS=200 };
    float left[FRAMES],right[FRAMES],out_left[FRAMES],out_right[FRAMES];
    uint64_t hashes[UBIG_PROFILE_COUNT]={0};
    const char *names[UBIG_PROFILE_COUNT]={"Dynamic","Movie","Music","Game","Voice","Course","Custom"};

    for(unsigned profile=0;profile<UBIG_PROFILE_COUNT;profile++){
        char path[128];snprintf(path,sizeof path,"/tmp/ubig-profile-matrix-%ld-%u",(long)getpid(),profile);unlink(path);
        CandidateInstance instance={0};
        if(create_instance(library,path,argv[2],&instance))return 4;
        ubig_control_handle control;if(ubig_control_open(&control,path,0))return 5;
        ubig_control_page page;if(ubig_control_snapshot(&control,&page))return 6;
        if(!(page.engine_flags&UBIG_CONTROL_ENGINE_LIVE))return 7;

        for(unsigned block=0;block<WARM_BLOCKS;block++){
            make_input(left,right,FRAMES,(size_t)block*FRAMES);
            run_instance(&instance,left,right,out_left,out_right,FRAMES);
        }
        if(profile!=UBIG_PROFILE_DYNAMIC){
            if(profile==UBIG_PROFILE_CUSTOM){
                const int32_t flat[UBIG_EQ_BANDS]={0};
                if(ubig_control_request_custom_eq(&control,flat))return 8;
            }else if(ubig_control_request_profile(&control,(ubig_profile)profile))return 9;
        }

        uint64_t hash=UINT64_C(1469598103934665603);
        for(unsigned block=0;block<TEST_BLOCKS;block++){
            size_t base=(size_t)(WARM_BLOCKS+block)*FRAMES;
            make_input(left,right,FRAMES,base);
            run_instance(&instance,left,right,out_left,out_right,FRAMES);
            for(unsigned i=0;i<FRAMES;i++){hash=hash_float(hash,out_left[i]);hash=hash_float(hash,out_right[i]);}
        }
        if(ubig_control_snapshot(&control,&page)||page.active_profile!=profile||page.last_error)return 10;
        hashes[profile]=hash;
        printf("%s %016llx\n",names[profile],(unsigned long long)hash);

        instance.descriptor->cleanup(instance.handle);
        if(ubig_control_snapshot(&control,&page))return 11;
        if(page.engine_flags&UBIG_CONTROL_ENGINE_LIVE)return 12;
        ubig_control_close(&control);unlink(path);
    }

    /* Final Windows stereo policy intentionally aliases Music and Game. */
    if(hashes[UBIG_PROFILE_MUSIC]!=hashes[UBIG_PROFILE_GAME]){
        fprintf(stderr,"Music/Game no longer match the Windows stereo alias\n");return 13;
    }
    for(unsigned i=0;i<UBIG_PROFILE_COUNT;i++)for(unsigned j=i+1;j<UBIG_PROFILE_COUNT;j++){
        if((i==UBIG_PROFILE_MUSIC&&j==UBIG_PROFILE_GAME))continue;
        if(hashes[i]==hashes[j]){
            fprintf(stderr,"unexpected profile collapse: %s == %s\n",names[i],names[j]);return 14;
        }
    }
    puts("PASS SP11 seven-profile matrix: six distinct outputs; Music/Game intentional Windows stereo alias");
    dlclose(library);return 0;
}
