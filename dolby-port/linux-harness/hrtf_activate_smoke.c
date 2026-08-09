#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>

typedef struct { uint32_t d1; uint16_t d2,d3; uint8_t d4[8]; } Guid;
typedef struct FakeUnknown FakeUnknown;
typedef struct { int32_t(*qi)(FakeUnknown*,const Guid*,void**); uint32_t(*addref)(FakeUnknown*); uint32_t(*release)(FakeUnknown*); } FakeVtbl;
struct FakeUnknown { FakeVtbl *vt; uint32_t refs; };
static int32_t fake_qi(FakeUnknown*o,const Guid*i,void**out){(void)i;if(!out)return (int32_t)0x80004003u;*out=o;o->refs++;return 0;}
static uint32_t fake_addref(FakeUnknown*o){return ++o->refs;}
static uint32_t fake_release(FakeUnknown*o){if(o->refs)o->refs--;return o->refs;}
static FakeVtbl g_fvt={fake_qi,fake_addref,fake_release};
static FakeUnknown g_ftm={&g_fvt,1};
static int32_t shim_CoCreateFreeThreadedMarshaler(void *outer, void **out){(void)outer;if(!out)return (int32_t)0x80004003u;g_ftm.refs++;*out=&g_ftm;return 0;}
static void *shim_CoTaskMemAlloc(size_t n){return malloc(n?n:1);}
static void *shim_malloc(size_t n){return malloc(n?n:1);}
static void shim_free(void*p){free(p);}
static int shim_callnewh(size_t n){(void)n;return 0;}

static void crash(int s,siginfo_t*si,void*ctx){
#if defined(__aarch64__)
 ucontext_t*u=ctx; fprintf(stderr,"CRASH sig=%d addr=%p pc=%p lr=%p x0=%#llx x1=%#llx x2=%#llx x3=%#llx x15=%#llx\n",s,si->si_addr,(void*)u->uc_mcontext.pc,(void*)u->uc_mcontext.regs[30],(unsigned long long)u->uc_mcontext.regs[0],(unsigned long long)u->uc_mcontext.regs[1],(unsigned long long)u->uc_mcontext.regs[2],(unsigned long long)u->uc_mcontext.regs[3],(unsigned long long)u->uc_mcontext.regs[15]);
#endif
 _Exit(128+s);
}
static void hs(void){struct sigaction a={0};a.sa_sigaction=crash;a.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&a,0);sigaction(SIGBUS,&a,0);sigaction(SIGILL,&a,0);sigaction(SIGFPE,&a,0);}
static void patchq(Sp11PeImage*i,uint32_t rva,void*f){*(uintptr_t*)(i->base+rva)=(uintptr_t)f;}
static uint64_t q(void*p,size_t o){uint64_t v;memcpy(&v,(char*)p+o,8);return v;}
static int32_t qi(void *obj,const Guid*iid,void**out){void **vt=*(void***)obj; return ((int32_t(*)(void*,const Guid*,void**))vt[0])(obj,iid,out);}
static void show_iface(Sp11PeImage*i,void*obj,const char*name,const Guid*g){void*p=0;int32_t hr=qi(obj,g,&p);printf("QI %-10s hr=0x%08x ptr=%p",name,(uint32_t)hr,p);if(p){printf(" delta=%td vtable_rva=0x%llx",(char*)p-(char*)obj,(unsigned long long)((uint8_t*)*(void**)p-i->base));}putchar('\n');}
int main(int ac,char**av){
 hs(); if(ac<2){fprintf(stderr,"dll path\n");return 2;} Sp11PeImage im; int rc=sp11_pe_load(&im,av[1]); if(rc){fprintf(stderr,"load rc=%d\n",rc);return 3;}
 /* HRTF IATs used by activation */
 patchq(&im,0x18000,(void*)shim_CoCreateFreeThreadedMarshaler);
 patchq(&im,0x18008,(void*)shim_CoTaskMemAlloc);
 patchq(&im,0x18208,(void*)shim_callnewh);
 patchq(&im,0x18218,(void*)shim_malloc);
 patchq(&im,0x18220,(void*)shim_free);
 printf("mapped=%p size=0x%zx delta=%lld guard=%p\n",im.base,im.size,(long long)im.delta,(void*)(uintptr_t)q(im.base,0x18308));
 /* speaker activation map record 0x19fa8; constructor 0x3c10; IActivationFactory IID 0x1a150 */
 uint32_t flags=1; void *factory=0;
 typedef int32_t(*FactoryCtor)(uint32_t*,void*,const Guid*,void**);
 FactoryCtor fc=(FactoryCtor)(im.base+0x3c10);
 int32_t hr=fc(&flags,im.base+0x19fa8,(const Guid*)(im.base+0x1a150),&factory);
 printf("factory ctor hr=0x%08x flags=0x%x factory=%p\n",(uint32_t)hr,flags,factory);
 if(hr<0||!factory)return 10;
 void **fvt=*(void***)factory; printf("factory vtable_rva=0x%llx activate_rva=0x%llx\n",(unsigned long long)((uint8_t*)fvt-im.base),(unsigned long long)((uint8_t*)fvt[6]-im.base));
 void *inst=0; typedef int32_t(*Activate)(void*,void**); hr=((Activate)fvt[6])(factory,&inst); printf("ActivateInstance hr=0x%08x instance=%p\n",(uint32_t)hr,inst); if(hr<0||!inst)return 11;
 printf("instance vtable_rva=0x%llx\n",(unsigned long long)((uint8_t*)*(void**)inst-im.base));
 const Guid i_asar={0xdeff1192,0xf581,0x4d77,{0x9c,0x1b,0x3e,0x59,0x6b,0x0c,0xa9,0x89}};
 const Guid i_setup1={0xdc57ddb4,0xe086,0x49ec,{0xb1,0x3d,0xec,0xcd,0xd5,0x12,0x99,0x0c}};
 const Guid i_setup2={0x54151d15,0x066e,0x441c,{0x81,0xe7,0xd8,0x94,0xd8,0xa0,0xab,0xc7}};
 const Guid i_dep={0x98f37dac,0xd0b6,0x49f5,{0x89,0x6a,0xaa,0x4d,0x16,0x9a,0x4c,0x48}};
 show_iface(&im,inst,"IAsar2",&i_asar); show_iface(&im,inst,"SetupA",&i_setup1); show_iface(&im,inst,"SetupB",&i_setup2); show_iface(&im,inst,"Dep",&i_dep);
 return 0;
}
