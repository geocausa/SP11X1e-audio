#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define CTOR_VA 0x18001BFB0ULL
#define ORCH_VA 0x18001F7A8ULL
#define STATE_BYTES 0x20000
#define AUX_OFF 0x7848
#define BUF_A_OFF 0x9904
#define BUF_B_OFF 0xA104

typedef void *(*CtorFn)(uint32_t,uint32_t,uint32_t,uint32_t,void*);
typedef void (*OrchFn)(void*,void*,void*,void*);
typedef struct { uint64_t channels,stride,format,planes; } AudioDesc;
static void lock_noop(void*p){(void)p;} static int lock_true(void*p){(void)p;return 1;}
static int initex_true(void*p,unsigned a,unsigned b){(void)p;(void)a;(void)b;return 1;}
static void piat(Sp11PeImage*i,uint64_t va,void*f){*(uintptr_t*)sp11_pe_ptr_for_va(i,va)=(uintptr_t)f;}
static void patch(Sp11PeImage*i){
 piat(i,0x1801070E0ULL,lock_noop);piat(i,0x1801070E8ULL,lock_noop);piat(i,0x180107190ULL,lock_noop);
 piat(i,0x180107198ULL,lock_true);piat(i,0x180107248ULL,lock_noop);piat(i,0x180107250ULL,initex_true);
}
static uint32_t r32(void*p,size_t o){uint32_t v;memcpy(&v,(char*)p+o,4);return v;} static void w32(void*p,size_t o,uint32_t v){memcpy((char*)p+o,&v,4);}
static double rms(float*p,size_t n){double s=0;for(size_t i=0;i<n;i++)s+=(double)p[i]*p[i];return sqrt(s/n);} static double mx(float*p,size_t n){double m=0;for(size_t i=0;i<n;i++){double a=fabs(p[i]);if(a>m)m=a;}return m;}
int main(int argc,char**argv){const char*dll=argc>1?argv[1]:"dll/DolbyAPOvlldp150.dll"; int gate=argc>2?atoi(argv[2]):1; int blocks=argc>3?atoi(argv[3]):128;
 Sp11PeImage img;if(sp11_pe_load(&img,dll)){fprintf(stderr,"load fail\n");return 2;}patch(&img);
 void*arena=NULL;if(posix_memalign(&arena,32,STATE_BYTES)){return 3;}memset(arena,0,STATE_BYTES);
 CtorFn ctor=(CtorFn)sp11_pe_ptr_for_va(&img,CTOR_VA);uint8_t*s=ctor(256,48000,2,0,arena);if(!s){fprintf(stderr,"ctor fail\n");return 4;}
 printf("cold gate_before=%u c64=%u c88=%u selector=%u\n",r32(s,0xc6c),r32(s,0xc64),r32(s,0xc88),r32(s,0x1630));w32(s,0xc6c,gate?1:0);
 float*a=(float*)(s+BUF_A_OFF),*b=(float*)(s+BUF_B_OFF);uint64_t pa[2]={(uintptr_t)a,(uintptr_t)(a+1)},pb[2]={(uintptr_t)b,(uintptr_t)(b+1)};AudioDesc da={2,2,7,(uintptr_t)pa},db={2,2,7,(uintptr_t)pb};OrchFn orch=(OrchFn)sp11_pe_ptr_for_va(&img,ORCH_VA);
 double outsum=0;int nan=0;for(int k=0;k<blocks;k++){for(int i=0;i<256;i++){double t=(k*256+i)/48000.0;float x=.03f*sinf(2*M_PI*997*t)+.015f*sinf(2*M_PI*113*t);a[2*i]=x;a[2*i+1]=x;}memset(b,0,0x800);orch(s,&da,&db,s+AUX_OFF);for(int i=0;i<512;i++)if(!isfinite(b[i]))nan++;double rr=rms(b,512);outsum+=rr;if(k<8||k==blocks-1)printf("block=%d in_rms=%.9f out_rms=%.9f out_max=%.9f selector=%u\n",k,rms(a,512),rr,mx(b,512),r32(s,0x1630));}
 printf("DONE gate=%d blocks=%d mean_out_rms=%.9f nan=%d selector=%u\n",gate,blocks,outsum/blocks,nan,r32(s,0x1630));return nan?5:0;}
