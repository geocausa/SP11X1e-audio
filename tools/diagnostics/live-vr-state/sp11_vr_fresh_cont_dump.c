#define _GNU_SOURCE
#define SP11_VR_OUTER_NO_MAIN
#include "dolby-port/sp11_dolby_windows_chain_ladspa.c"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double rr(const float*p,size_t n){double s=0;for(size_t i=0;i<n;i++)s+=(double)p[i]*p[i];return sqrt(s/n);}
static void tone_at(float*p,unsigned n,uint64_t off){for(unsigned i=0;i<n;i++){double t=(double)(off+i)/48000.;float v=(float)(.05*sin(2*M_PI*997*t));p[2*i]=v;p[2*i+1]=v;}}
int main(void){
 setenv("SP11_DOLBY_PROFILE","music",1);setenv("SP11_DOLBY_GEQ","off",1);
 ChainInst*p=(ChainInst*)chain_instantiate(NULL,48000);if(!p)return 2;float bypass=0;chain_connect(p,PORT_BYPASS,&bypass);chain_activate(p);if(!p->ready)return 3;
 uint8_t*inner=(uint8_t*)(uintptr_t)q(p->vr_outer,VR_INNER_PTR_OFF);void*core=(void*)(uintptr_t)q(inner,0x130);
 printf("inner=%p core=%p fill=%u b38=%u b3c=%u\n",inner,core,d(inner,0x20),d(inner,0x38),d(inner,0x3c));
 ProcessFn proc=(ProcessFn)sp11_pe_ptr_for_va(&p->vr_img,VR_PROCESS_VA);enum{N=256};float in[N*2],out[N*2];
 for(unsigned k=0;k<4096;k++){memset(in,0,sizeof(in));memset(out,0,sizeof(out));tone_at(in,N,(uint64_t)k*N);proc(inner,out,in,N);float pk=0;for(unsigned j=0;j<N*2;j++)if(fabsf(out[j])>pk)pk=fabsf(out[j]);if(k<8 || k==15 || k==31 || k==63 || k==127 || k==255 || k==511 || k==1023 || k==2047 || k==4095) printf("block=%02u fill=%u in_rms=%.9f out_rms=%.9f peak=%.9f\n",k,d(inner,0x20),rr(in,N*2),rr(out,N*2),pk);}
 FILE *df=fopen("/tmp/vr-fresh-music-after-cont.bin","wb"); if(df){fwrite(core,1,0x6000,df);fclose(df);}
 chain_cleanup(p);return 0;
}
