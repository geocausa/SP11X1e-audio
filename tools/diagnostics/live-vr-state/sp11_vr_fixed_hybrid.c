#define _GNU_SOURCE
#define SP11_VR_OUTER_NO_MAIN
#include "dolby-port/sp11_dolby_windows_chain_ladspa.c"
#include <sys/mman.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DLL_RUNTIME_BASE 0x00007ffd07a60000ULL
#define OUTER_BASE       0x0000024539010000ULL
#define LIVE_CORE_VA     0x00000245391dd808ULL
#define LIVE_CORE_OFF    (LIVE_CORE_VA-OUTER_BASE)
static double rr(const float*p,size_t n){double s=0;for(size_t i=0;i<n;i++)s+=(double)p[i]*p[i];return sqrt(s/n);}
static void tone_at(float*p,unsigned n,uint64_t off){for(unsigned i=0;i<n;i++){double t=(double)(off+i)/48000.;float v=(float)(.05*sin(2*M_PI*997*t));p[2*i]=v;p[2*i+1]=v;}}
static int readall(const char*p,void*b,size_t n){FILE*f=fopen(p,"rb");if(!f)return -1;size_t g=fread(b,1,n,f);fclose(f);return g==n?0:-2;}
int main(int ac,char**av){
 const char*mode=ac>1?av[1]:"fresh";const char*live=ac>2?av[2]:"/tmp/june-vr-outer-174744.bin";
 hs(); size_t mapsz=(VR_OUTER_SIZE+0xfff)&~0xfffull;uint8_t*outer=mmap((void*)OUTER_BASE,mapsz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(outer==MAP_FAILED){perror("mmap");return 2;}
 ChainInst p;memset(&p,0,sizeof(p));p.profile=CHAIN_PROFILE_MUSIC;p.vr_outer=outer;
 if(sp11_pe_load_at(&p.vr_img,chain_vr_path(),DLL_RUNTIME_BASE)){puts("load fail");return 3;}p.vr_loaded=1;
 g_resource_data=sp11_pe_ptr_for_va(&p.vr_img,VR_RESOURCE_VA);patch_runtime(&p.vr_img);((VoidFn)sp11_pe_ptr_for_va(&p.vr_img,VR_RATE_MAP_INIT_VA))();init_empty_property_map();*(uintptr_t*)sp11_pe_ptr_for_va(&p.vr_img,VR_VTABLE_VA+0x18)=(uintptr_t)shim_GetEmptyPropertyMap;
 memcpy(p.vr_resource,sp11_pe_ptr_for_va(&p.vr_img,VR_RESOURCE_VA),VR_RESOURCE_SIZE);p.vr_deinit=(VrDeinitFn)sp11_pe_ptr_for_va(&p.vr_img,VR_DEINIT_VA);
 if(vr_build(&p)){puts("vr_build fail");return 4;}
 uint8_t*inner=outer+VR_INNER_OFF;uint8_t*core=(uint8_t*)(uintptr_t)q(inner,0x130);
 printf("fresh outer=%p inner=%p core=%p expected_core=%p fill=%u mode=%s\n",outer,inner,core,(void*)LIVE_CORE_VA,d(inner,0x20),mode);
 if(core!=(uint8_t*)LIVE_CORE_VA){fprintf(stderr,"core geometry mismatch\n");return 5;}
 uint8_t*cap=malloc(VR_OUTER_SIZE);if(!cap||readall(live,cap,VR_OUTER_SIZE)){puts("read live fail");return 6;}
 if(!strcmp(mode,"core")) memcpy(core,cap+LIVE_CORE_OFF,0x6000);
 else if(!strcmp(mode,"arena")) memcpy(outer+VR_ARENA_OFF,cap+VR_ARENA_OFF,VR_ARENA_SIZE);
 else if(!strcmp(mode,"inner")) memcpy(inner,cap+VR_INNER_OFF,0x300);
 else if(!strcmp(mode,"inner_core")){memcpy(inner,cap+VR_INNER_OFF,0x300);memcpy(core,cap+LIVE_CORE_OFF,0x6000);}
 else if(!strcmp(mode,"all")) memcpy(outer,cap,VR_OUTER_SIZE);
 free(cap);
 inner=outer+VR_INNER_OFF;core=(uint8_t*)(uintptr_t)q(inner,0x130);
 ProcessFn proc=(ProcessFn)sp11_pe_ptr_for_va(&p.vr_img,VR_PROCESS_VA);enum{N=256};float in[N*2],out[N*2];
 for(unsigned k=0;k<4096;k++){memset(out,0,sizeof(out));tone_at(in,N,(uint64_t)k*N);proc(inner,out,in,N);if(k==31||k==255||k==1023||k==4095){float pk=0;for(unsigned j=0;j<N*2;j++)if(fabsf(out[j])>pk)pk=fabsf(out[j]);printf("block=%u fill=%u rms=%.9f peak=%.9f\n",k,d(inner,0x20),rr(out,N*2),pk);}}
 return 0;
}
