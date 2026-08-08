#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CTOR_VA  0x18001BFB0ULL
#define PID5_VA  0x18001BC80ULL
#define APPLY_VA 0x18001D280ULL
#define PID17_VA 0x18001CDD0ULL
#define PID22_VA 0x18001E5C8ULL
#define PID31_VA 0x18001EE68ULL
#define AO_ENABLE_VA     0x18001B8E0ULL
#define AO_GAINS_VA      0x18001B938ULL
#define REG_ISO_VA       0x18001E420ULL
#define REG_TUNE_VA      0x18001E670ULL
#define REG_SLOPE_VA     0x18001E3B0ULL
#define REG_OVERDRIVE_VA 0x18001E510ULL
#define REG_SPKDIST_VA   0x18001E570ULL
#define REG_TIMBRE_VA    0x18001E810ULL
#define TARGET_POWER_VA  0x18001F1B0ULL
#define PEAK_LEVEL_VA    0x18001D100ULL
#define POSTGAIN_VA      0x18001D170ULL
#define SYSTEM_GAIN_VA   0x18001F150ULL
#define NOISE_GATE_EN_VA 0x18001D010ULL
#define NOISE_GATE_TH_VA 0x18001D080ULL
#define ARENA_SIZE 0x20000

typedef void *(*CtorFn)(uint32_t,uint32_t,uint32_t,uint32_t,void*);
typedef void (*Pid5Fn)(void*,const uint32_t*);
typedef void (*ApplyFn)(void*,uint32_t);
typedef void (*Pid17Fn)(void*,uint32_t,const int32_t *const*);
typedef void (*Pid22Fn)(void*,uint32_t,const int32_t*);
typedef void (*Pid31Fn)(void*,const int32_t*);
typedef void (*ScalarFn)(void*,int32_t);
typedef void (*ArrayCountFn)(void*,const int32_t*,uint32_t);
typedef void (*Array20Fn)(void*,const int32_t*);
typedef void (*ArrayPair20Fn)(void*,const int32_t*,const int32_t*);

static void lock_noop(void*p){(void)p;} static int lock_true(void*p){(void)p;return 1;}
static int initex_true(void*p,unsigned a,unsigned b){(void)p;(void)a;(void)b;return 1;}
static void piat(Sp11PeImage*i,uint64_t va,void*f){*(uintptr_t*)sp11_pe_ptr_for_va(i,va)=(uintptr_t)f;}
static void patch_runtime(Sp11PeImage*i){
 piat(i,0x1801070E0ULL,lock_noop); piat(i,0x1801070E8ULL,lock_noop);
 piat(i,0x180107190ULL,lock_noop); piat(i,0x180107198ULL,lock_true);
 piat(i,0x180107248ULL,lock_noop); piat(i,0x180107250ULL,initex_true);
}
static int read_exact(const char*p,void*d,size_t n){FILE*f=fopen(p,"rb");if(!f){perror(p);return -1;}size_t g=fread(d,1,n,f);fclose(f);return g==n?0:-1;}
static size_t diff_range(const uint8_t*a,const uint8_t*b,size_t off,size_t n){size_t d=0;for(size_t i=0;i<n;i++)d+=(a[off+i]!=b[off+i]);return d;}
static uint32_t u32(const uint8_t*s,size_t o){uint32_t v;memcpy(&v,s+o,4);return v;}
static int32_t i32(const uint8_t*s,size_t o){int32_t v;memcpy(&v,s+o,4);return v;}
static float f32(const uint8_t*s,size_t o){float v;memcpy(&v,s+o,4);return v;}

int main(int argc,char**argv){
 if(argc!=3){fprintf(stderr,"usage: %s DLL freshgraph_orch_state.bin\n",argv[0]);return 2;}
 Sp11PeImage img;if(sp11_pe_load(&img,argv[1]))return 3;patch_runtime(&img);
 void*arena=NULL;if(posix_memalign(&arena,32,ARENA_SIZE))return 4;memset(arena,0,ARENA_SIZE);
 uint8_t*s=((CtorFn)sp11_pe_ptr_for_va(&img,CTOR_VA))(256,48000,2,0,arena);if(!s)return 5;
 uint8_t cold[0x4000]; memcpy(cold,s,sizeof(cold));
 uint32_t empty_filter_blob[2]={0,0};
 int32_t group0[6]={20,0,32767,10,20,0}; const int32_t*groups[1]={group0};
 static const int32_t ao[40]={-16,18,16,30,16,-32,-16,-32,-16,-32,-48,-62,-64,-64,-16,-16,-16,16,80,48,0,32,32,45,16,0,-16,-16,-16,0,-32,-38,-48,-48,0,0,0,32,96,64};
 static const int32_t high[20]={-74,-112,-192,-237,-238,-226,-157,0,0,0,0,0,0,0,0,0,0,0,0,0};
 static const int32_t low[20]={-266,-304,-384,-429,-430,-418,-349,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192,-192};
 static const int32_t isolated[20]={1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0};
 int32_t stress[8]={216,216,0,0,0,0,0,0}; int32_t bass[5]={0,0,0,0,0};
 ((Pid5Fn)sp11_pe_ptr_for_va(&img,PID5_VA))(s,empty_filter_blob);
 printf("after_pid5 dirty=%u pending_c68=%u pending_c70=%u gate=%u\n",u32(s,0x66c),u32(s,0xc68),u32(s,0xc70),u32(s,0xc6c));
 ((ScalarFn)sp11_pe_ptr_for_va(&img,AO_ENABLE_VA))(s,1);
 ((ArrayCountFn)sp11_pe_ptr_for_va(&img,AO_GAINS_VA))(s,ao,40);
 ((Pid17Fn)sp11_pe_ptr_for_va(&img,PID17_VA))(s,1,groups);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,TARGET_POWER_VA))(s,-80);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,PEAK_LEVEL_VA))(s,0);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,POSTGAIN_VA))(s,0);
 ((Pid22Fn)sp11_pe_ptr_for_va(&img,PID22_VA))(s,8,stress);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,REG_SLOPE_VA))(s,14);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,REG_OVERDRIVE_VA))(s,0);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,REG_TIMBRE_VA))(s,12);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,REG_SPKDIST_VA))(s,1);
 ((ArrayPair20Fn)sp11_pe_ptr_for_va(&img,REG_TUNE_VA))(s,high,low);
 ((Array20Fn)sp11_pe_ptr_for_va(&img,REG_ISO_VA))(s,isolated);
 ((Pid31Fn)sp11_pe_ptr_for_va(&img,PID31_VA))(s,bass);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,SYSTEM_GAIN_VA))(s,0);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,NOISE_GATE_EN_VA))(s,0);
 ((ScalarFn)sp11_pe_ptr_for_va(&img,NOISE_GATE_TH_VA))(s,-1440);
 printf("before_apply dirty=%u pending_c68=%u pending_c70=%u gate=%u\n",u32(s,0x66c),u32(s,0xc68),u32(s,0xc70),u32(s,0xc6c));
 ((ApplyFn)sp11_pe_ptr_for_va(&img,APPLY_VA))(s,2);
 printf("after_apply dirty=%u pending_c68=%u pending_c70=%u gate=%u\n",u32(s,0x66c),u32(s,0xc68),u32(s,0xc70),u32(s,0xc6c));
 uint8_t win[0x4000];if(read_exact(argv[2],win,sizeof(win)))return 6;
 printf("cold_vs_windows regulator high=%zu/80 low=%zu/80 iso=%zu/80 stress=%zu/32\n",diff_range(cold,win,0x700,80),diff_range(cold,win,0x750,80),diff_range(cold,win,0x7a0,80),diff_range(cold,win,0x6d4,32));
 printf("full_vs_windows regulator high=%zu/80 low=%zu/80 iso=%zu/80 stress=%zu/32\n",diff_range(s,win,0x700,80),diff_range(s,win,0x750,80),diff_range(s,win,0x7a0,80),diff_range(s,win,0x6d4,32));
 printf("cold arrays first high=%d,%d,%d,%d low=%d,%d,%d,%d iso=%d,%d,%d,%d stress=%d,%d,%d,%d\n",i32(cold,0x700),i32(cold,0x704),i32(cold,0x708),i32(cold,0x70c),i32(cold,0x750),i32(cold,0x754),i32(cold,0x758),i32(cold,0x75c),i32(cold,0x7a0),i32(cold,0x7a4),i32(cold,0x7a8),i32(cold,0x7ac),i32(cold,0x6d4),i32(cold,0x6d8),i32(cold,0x6dc),i32(cold,0x6e0));
 printf("static_scalar_diffs ao_en=%zu slope=%zu overdrive=%zu timbre=%zu spkdist=%zu sysgain=%zu peak=%zu target=%zu noise_en=%zu noise_th=%zu\n",
   diff_range(s,win,0x98,4),diff_range(s,win,0x688,4),diff_range(s,win,0x674,4),diff_range(s,win,0x67c,4),diff_range(s,win,0x6fc,4),diff_range(s,win,0x94,4),diff_range(s,win,0xdd4,4),diff_range(s,win,0xde0,4),diff_range(s,win,0xd60,4),diff_range(s,win,0xd6c,4));
 printf("optimizer_region_diff=%zu/160 firstfloats=%g,%g,%g,%g win=%g,%g,%g,%g\n",diff_range(s,win,0xa0,160),f32(s,0xa0),f32(s,0xa4),f32(s,0xa8),f32(s,0xac),f32(win,0xa0),f32(win,0xa4),f32(win,0xa8),f32(win,0xac));
 printf("pid17_region_diff=%zu/96\n",diff_range(s,win,0xe94,96));
 printf("pid22_region_diff=%zu/68\n",diff_range(s,win,0x690,68));
 printf("pid31_region_diff=%zu/20\n",diff_range(s,win,0x10cc,20));
 printf("setter_state boundary=%d,%d,%d,%d floor=%d high=%d attack=%.9f release=%.9f mix=%d\n",
   i32(s,0xe94),i32(s,0xe98),i32(s,0xe9c),i32(s,0xea0),i32(s,0xea4),i32(s,0xeb4),f32(s,0xec4),f32(s,0xed4),i32(s,0xee4));
 printf("windows_state boundary=%d,%d,%d,%d floor=%d high=%d attack=%.9f release=%.9f mix=%d\n",
   i32(win,0xe94),i32(win,0xe98),i32(win,0xe9c),i32(win,0xea0),i32(win,0xea4),i32(win,0xeb4),f32(win,0xec4),f32(win,0xed4),i32(win,0xee4));
 printf("stress count=%u vals=%d,%d,%d,%d,%d,%d,%d,%d\n",u32(s,0x690),i32(s,0x6d4),i32(s,0x6d8),i32(s,0x6dc),i32(s,0x6e0),i32(s,0x6e4),i32(s,0x6e8),i32(s,0x6ec),i32(s,0x6f0));
 printf("dirty=%u pending_c68=%u pending_c70=%u gate=%u\n",u32(s,0x66c),u32(s,0xc68),u32(s,0xc70),u32(s,0xc6c));
 return 0;
}
