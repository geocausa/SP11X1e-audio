#define _GNU_SOURCE
#define main sp11_vr_probe_embedded_main
#include "dolby-port/sp11_vr_init_probe.c"
#undef main
#include <sys/mman.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DLL_RUNTIME_BASE 0x00007ffd07a60000ULL
#define OUTER_BASE       0x0000024539010000ULL
#define OUTER_SIZE       0x3C0430u
#define INNER_OFF        0x12C2F0u
static double rr(const float*p,size_t n){double s=0;for(size_t i=0;i<n;i++)s+=(double)p[i]*p[i];return sqrt(s/n);}
static void tone_at(float*p,unsigned n,uint64_t off){for(unsigned i=0;i<n;i++){double t=(double)(off+i)/48000.;float v=(float)(.05*sin(2*M_PI*997*t));p[2*i]=v;p[2*i+1]=v;}}
static int readall(const char*p,void*b,size_t n){FILE*f=fopen(p,"rb");if(!f)return -1;size_t g=fread(b,1,n,f);fclose(f);return g==n?0:-2;}
int main(int ac,char**av){
 if(ac<3){fprintf(stderr,"usage: %s DLL OUTER.bin\n",av[0]);return 2;} hs();
 Sp11PeImage img;int rc=sp11_pe_load_at(&img,av[1],DLL_RUNTIME_BASE);if(rc){fprintf(stderr,"pe load rc=%d\n",rc);return 3;}
 g_resource_data=sp11_pe_ptr_for_va(&img,VR_RESOURCE_VA);patch_runtime(&img);((VoidFn)sp11_pe_ptr_for_va(&img,VR_RATE_MAP_INIT_VA))();
 size_t mapsz=(OUTER_SIZE+0xfff)&~0xfffull;void*mp=mmap((void*)OUTER_BASE,mapsz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(mp==MAP_FAILED){perror("outer mmap");return 4;}
 if(readall(av[2],mp,OUTER_SIZE)){fprintf(stderr,"read outer fail\n");return 5;}
 uint8_t*outer=(uint8_t*)mp;uint8_t*inner=outer+INNER_OFF;void*core=(void*)(uintptr_t)q(inner,0x130);
 printf("outer=%p inner=%p vtable=%p core=%p fill=%u b38=%u b3c=%u\n",outer,inner,(void*)(uintptr_t)q(inner,0),core,d(inner,0x20),d(inner,0x38),d(inner,0x3c));
 ProcessFn proc=(ProcessFn)sp11_pe_ptr_for_va(&img,VR_PROCESS_VA);enum{N=256};float in[N*2],out[N*2];
 for(unsigned k=0;k<4096;k++){memset(in,0,sizeof(in));memset(out,0,sizeof(out));tone_at(in,N,(uint64_t)k*N);proc(inner,out,in,N);float pk=0;for(unsigned j=0;j<N*2;j++)if(fabsf(out[j])>pk)pk=fabsf(out[j]);if(k<8 || k==15 || k==31 || k==63 || k==127 || k==255 || k==511 || k==1023 || k==2047 || k==4095) printf("block=%02u fill=%u in_rms=%.9f out_rms=%.9f peak=%.9f\n",k,d(inner,0x20),rr(in,N*2),rr(out,N*2),pk);}
 FILE *df=fopen("/tmp/vr-live-music-after-cont.bin","wb"); if(df){fwrite(core,1,0x6000,df);fclose(df);}
 return 0;
}
