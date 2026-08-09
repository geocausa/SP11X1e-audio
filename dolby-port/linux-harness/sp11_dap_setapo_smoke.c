#define _GNU_SOURCE
#include "sp11_vlldp_pe_loader.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <malloc.h>

#define DLL_DEFAULT "/home/geoca/Documents/SP11-PROJECT/00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/dolby/dolby-qualcomm-dissection-local/runtime-live/DolbyAudioProcessing.dll"
#define PARENT_FACTORY_VA 0x180005380ULL
#define INITIALIZE_VA     0x1800158C0ULL
#define CONFIGURE_VA      0x180017DF0ULL
#define RESOURCE_VA       0x18033D0A0ULL
#define RESOURCE_SIZE     17260U
#define TLS_TEMPLATE_VA   0x180329B58ULL
#define TLS_INDEX_VA      0x1803367B8ULL

typedef int32_t (*ParentFactoryFn)(void*,void**);
typedef int32_t (*InitializeFn)(void*,void*,void*);
typedef int32_t (*ConfigureFn)(void*,const uint32_t*,uint32_t,void*);
typedef uint64_t (*Raw4Fn)(void*,void*,void*,void*);
extern uint64_t sp11_win_call4(void *fn, void *teb, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3);
__asm__(
".text\n"
".align 2\n"
".global sp11_win_call4\n"
"sp11_win_call4:\n"
"  stp x29, x30, [sp, #-32]!\n"
"  mov x29, sp\n"
"  str x18, [sp, #16]\n"
"  mov x16, x0\n"
"  mov x18, x1\n"
"  mov x0, x2\n"
"  mov x1, x3\n"
"  mov x2, x4\n"
"  mov x3, x5\n"
"  blr x16\n"
"  ldr x18, [sp, #16]\n"
"  ldp x29, x30, [sp], #32\n"
"  ret\n"
);

typedef struct WinTlsCtx {
    uint8_t teb[0x100];
    void *slots[64];
    uint8_t block[0x100];
} WinTlsCtx;
static void setup_win_tls(Sp11PeImage *img, WinTlsCtx *w){
    memset(w,0,sizeof(*w));
    memcpy(w->block,sp11_pe_ptr_for_va(img,TLS_TEMPLATE_VA),8);
    w->slots[0]=w->block;
    *(void**)(w->teb+0x58)=w->slots;
    *(uint32_t*)sp11_pe_ptr_for_va(img,TLS_INDEX_VA)=0;
    fprintf(stderr,"TLS teb=%p slots=%p slot0=%p template=%016llx index=%u\n",w->teb,w->slots,w->block,(unsigned long long)*(uint64_t*)w->block,*(uint32_t*)sp11_pe_ptr_for_va(img,TLS_INDEX_VA));
}
static void crash(int sig,siginfo_t*si,void*vp){ucontext_t*uc=vp;
#if defined(__aarch64__)
 fprintf(stderr,"CRASH sig=%d addr=%p pc=%#llx lr=%#llx x0=%#llx x1=%#llx x2=%#llx x3=%#llx x19=%#llx x20=%#llx\n",sig,si->si_addr,(unsigned long long)uc->uc_mcontext.pc,(unsigned long long)uc->uc_mcontext.regs[30],(unsigned long long)uc->uc_mcontext.regs[0],(unsigned long long)uc->uc_mcontext.regs[1],(unsigned long long)uc->uc_mcontext.regs[2],(unsigned long long)uc->uc_mcontext.regs[3],(unsigned long long)uc->uc_mcontext.regs[19],(unsigned long long)uc->uc_mcontext.regs[20]);
#endif
 _Exit(128+sig);
}
static void hs(void){struct sigaction sa={0};sa.sa_sigaction=crash;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,0);sigaction(SIGBUS,&sa,0);sigaction(SIGILL,&sa,0);sigaction(SIGABRT,&sa,0);sigaction(SIGFPE,&sa,0);}
static void cs_void(void*p){(void)p;} static int cs_try(void*p){(void)p;return 1;}
static void* heap_get(void){return (void*)1;}
static uint32_t win_get_last_error(void){return 0;}
static void win_set_last_error(uint32_t e){(void)e;}
static int win_sddl(const uint16_t*s,uint32_t rev,void**out,uint32_t*sz){(void)s;(void)rev;if(out)*out=calloc(1,32);if(sz)*sz=32;return out&&*out;}
static void* win_local_free(void*p){free(p);return NULL;}

typedef struct FakeHandle { uint64_t magic; size_t size; void *mem; } FakeHandle;
static void* win_create_mapping(void*file,void*sa,uint32_t prot,uint32_t hi,uint32_t lo,const uint16_t*name){(void)file;(void)sa;(void)prot;(void)name;size_t n=((uint64_t)hi<<32)|lo;if(!n)n=4096;FakeHandle*h=calloc(1,sizeof(*h));if(!h)return NULL;h->magic=0x53503131;h->size=n;fprintf(stderr,"shim CreateFileMapping size=%zu -> %p\n",n,(void*)h);return h;}
static void* win_open_mapping(uint32_t access,int inherit,const uint16_t*name){(void)access;(void)inherit;(void)name;return NULL;}
static void* win_map_view(void*vh,uint32_t access,uint32_t hi,uint32_t lo,size_t n){(void)access;(void)hi;(void)lo;FakeHandle*h=vh;if(!h||h->magic!=0x53503131)return NULL;if(!h->mem)h->mem=calloc(1,n?n:h->size);return h->mem;}
static int win_unmap_view(void*p){(void)p;return 1;}
static int win_close_handle(void*vh){FakeHandle*h=vh;if(h&&h->magic==0x53503131){free(h->mem);h->magic=0;free(h);}return 1;}

static void* win_new_handle(void){FakeHandle*h=calloc(1,sizeof(*h));if(h){h->magic=0x53503131;h->size=0;}return h;}
static void* win_create_mutex(void*sa,int owner,const uint16_t*name){(void)sa;(void)owner;(void)name;return win_new_handle();}
static void* win_open_mutex(uint32_t access,int inherit,const uint16_t*name){(void)access;(void)inherit;(void)name;return NULL;}
static int win_release_mutex(void*h){(void)h;return 1;}
static uint32_t win_wait_one(void*h,uint32_t ms){(void)h;(void)ms;return 0;}
static uint32_t win_wait_one_ex(void*h,uint32_t ms,int alert){(void)h;(void)ms;(void)alert;return 0;}
static void* win_create_event(void*sa,int manual,int initial,const uint16_t*name){(void)sa;(void)manual;(void)initial;(void)name;return win_new_handle();}
static void* win_open_event(uint32_t access,int inherit,const uint16_t*name){(void)access;(void)inherit;(void)name;return NULL;}
static int win_set_event(void*h){(void)h;return 1;} static int win_reset_event(void*h){(void)h;return 1;}
static uint32_t win_wait_many(uint32_t n,const void*hs,int all,uint32_t ms){(void)n;(void)hs;(void)all;(void)ms;return 0;}
static void* win_create_thread(void*sa,size_t stack,void*start,void*arg,uint32_t flags,uint32_t*tid){(void)sa;(void)stack;(void)start;(void)arg;(void)flags;if(tid)*tid=1;fprintf(stderr,"shim CreateThread start=%p arg=%p flags=%u (not launched)\n",start,arg,flags);return win_new_handle();}
static void* heap_alloc(void*h,uint32_t flags,size_t n){(void)h;void*p=(flags&8)?calloc(1,n):malloc(n?n:1);fprintf(stderr,"shim HeapAlloc n=%zu -> %p\n",n,p);return p;}
static int heap_free(void*h,uint32_t flags,void*p){(void)h;(void)flags;fprintf(stderr,"shim HeapFree %p\n",p);free(p);return 1;}
static void* heap_realloc(void*h,uint32_t flags,void*p,size_t n){(void)h;(void)flags;void*q=realloc(p,n?n:1);fprintf(stderr,"shim HeapReAlloc %p n=%zu -> %p\n",p,n,q);return q;}
static size_t heap_size(void*h,uint32_t flags,void*p){(void)h;(void)flags;return p?malloc_usable_size(p):(size_t)-1;}

static const void *g_res_data;
static void *g_module_handle;
static void* win_find_resource(void*h,const void*name,const void*type){(void)h;if((uintptr_t)name==254 && (uintptr_t)type==255)return (void*)0x254ff;return NULL;}
static void* win_load_resource(void*h,void*r){(void)h;return r;}
static void* win_lock_resource(void*r){(void)r;return (void*)g_res_data;}
static uint32_t win_size_resource(void*h,void*r){(void)h;(void)r;return RESOURCE_SIZE;}
static int win_qpc(int64_t *v){static int64_t t=1000000000;if(v){t+=53333;*v=t;}return 1;}
static int win_qpf(int64_t *v){if(v)*v=10000000;return 1;}
static uint64_t win_tick64(void){static uint64_t t=1000;return ++t;}
static uint32_t win_thread_id(void){return 1;}
static uint32_t win_get_module_filename(void*h,uint16_t*buf,uint32_t n){(void)h;if(buf&&n)buf[0]=0;return 0;}
static int win_get_module_handle_ex(uint32_t flags,const void*name,void**out){(void)flags;(void)name;if(out)*out=g_module_handle;return out?1:0;}
static int32_t win_create_guid(void *g){static uint64_t n=1;if(!g)return (int32_t)0x80070057u;memset(g,0,16);memcpy(g,&n,8);n++;return 0;}
static int win_string_from_guid(const void*g,uint16_t*out,int cch){(void)g;static const char a[]="{00000001-0000-0000-0000-000000000000}";int need=(int)sizeof(a);if(!out||cch<need)return 0;for(int i=0;i<need;i++)out[i]=(uint8_t)a[i];return need;}
static int32_t win_prop_variant_clear(void*p){if(p)memset(p,0,24);return 0;}
static void srw_void(void*p){(void)p;} static int srw_try(void*p){(void)p;return 1;}


typedef struct Guid { uint32_t d1; uint16_t d2,d3; uint8_t d4[8]; } Guid;
typedef struct FakeObj { void **vt; uint32_t refs; int kind; } FakeObj;
static int32_t fu_qi(FakeObj*o,const Guid*i,void**out){(void)i;if(!out)return (int32_t)0x80004003u;*out=o;o->refs++;return 0;}
static uint32_t fu_add(FakeObj*o){return ++o->refs;}
static uint32_t fu_rel(FakeObj*o){if(o->refs)o->refs--;return o->refs;}
static int32_t coll_count(FakeObj*o,uint32_t*n){(void)o;if(!n)return (int32_t)0x80004003u;*n=1;fprintf(stderr,"fake collection GetCount ->1\n");return 0;}
static FakeObj g_device,g_sysfx,g_store,g_endpoint,g_collection;
static int32_t coll_item(FakeObj*o,uint32_t idx,void**out){(void)o;fprintf(stderr,"fake collection Item(%u)\n",idx);if(!out||idx>0)return (int32_t)0x80070057u;*out=&g_device;fu_add(&g_device);return 0;}
static int32_t dev_activate(FakeObj*o,const Guid*iid,uint32_t clsctx,void*params,void**out){(void)o;(void)iid;(void)clsctx;(void)params;fprintf(stderr,"fake device Activate sysfx\n");if(!out)return (int32_t)0x80004003u;*out=&g_sysfx;fu_add(&g_sysfx);return 0;}
static int32_t sysfx_open(FakeObj*o,uint32_t access,void**out){(void)o;(void)access;fprintf(stderr,"fake sysfx OpenPropertyStore\n");if(!out)return (int32_t)0x80004003u;*out=&g_store;fu_add(&g_store);return 0;}
typedef struct PropVar { uint16_t vt; uint16_t r1,r2,r3; union { void *ptr; uint64_t u64; uint32_t u32; }; uint64_t extra; } PropVar;
static uint16_t g_endpoint_guid[]= {'{','0','0','0','0','0','0','0','1','-','0','0','0','0','-','0','0','0','0','-','0','0','0','0','-','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','1','}',0};
static uint16_t g_friendly[]= {'S','P','1','1',' ','L','i','n','u','x',' ','O','r','a','c','l','e',0};
static int32_t ps_count(FakeObj*o,uint32_t*n){(void)o;if(n)*n=0;return 0;}
static int32_t ps_getat(FakeObj*o,uint32_t i,void*k){(void)o;(void)i;(void)k;return (int32_t)0x80070490u;}
static int32_t ps_get(FakeObj*o,const void*key,PropVar*pv){(void)o;if(!pv)return (int32_t)0x80004003u;memset(pv,0,sizeof(*pv));const uint32_t*d=key; /* PROPERTYKEY pid at +16 */
 uint32_t pid=*(const uint32_t*)((const uint8_t*)key+16); pv->vt=31; pv->ptr=(pid==14?g_friendly:g_endpoint_guid); fprintf(stderr,"fake IPropertyStore::GetValue pid=%u -> LPWSTR %p\n",pid,pv->ptr);return 0;}
static int32_t ps_set(FakeObj*o,const void*k,const PropVar*pv){(void)o;(void)k;(void)pv;return 0;} static int32_t ps_commit(FakeObj*o){(void)o;return 0;}
static void *vt_collection[5]={(void*)fu_qi,(void*)fu_add,(void*)fu_rel,(void*)coll_count,(void*)coll_item};
static void *vt_device[7]={(void*)fu_qi,(void*)fu_add,(void*)fu_rel,(void*)dev_activate,NULL,NULL,NULL};
static void *vt_sysfx[10]={(void*)fu_qi,(void*)fu_add,(void*)fu_rel,(void*)sysfx_open,(void*)sysfx_open,(void*)sysfx_open,NULL,NULL,NULL,NULL};
static void *vt_store[8]={(void*)fu_qi,(void*)fu_add,(void*)fu_rel,(void*)ps_count,(void*)ps_getat,(void*)ps_get,(void*)ps_set,(void*)ps_commit};
static void fake_com_init(void){g_collection=(FakeObj){vt_collection,1,1};g_device=(FakeObj){vt_device,1,2};g_sysfx=(FakeObj){vt_sysfx,1,3};g_store=(FakeObj){vt_store,1,4};g_endpoint=(FakeObj){vt_store,1,5};}
static int32_t win_init_propvariant_clsid(const Guid*g,PropVar*pv){if(!pv)return (int32_t)0x80004003u;memset(pv,0,sizeof(*pv));pv->vt=72;Guid*c=malloc(16);if(!c)return (int32_t)0x8007000eu;*c=*g;pv->ptr=c;fprintf(stderr,"shim InitPropVariantFromCLSID\n");return 0;}
static int sleep_cv(void*cv,void*lock,uint32_t ms,uint32_t flags){(void)cv;(void)lock;(void)ms;(void)flags;return 1;}
static void wake_cv(void*cv){(void)cv;}
static void wake_all_cv(void*cv){(void)cv;}
static void pp(Sp11PeImage*i,uint64_t va,void*f){*(uintptr_t*)sp11_pe_ptr_for_va(i,va)=(uintptr_t)f;}
static void pret(Sp11PeImage*i,uint64_t va){uint32_t*p=sp11_pe_ptr_for_va(i,va);*p=0xd65f03c0u;__builtin___clear_cache((char*)p,(char*)p+4);}
static void pret0(Sp11PeImage*i,uint64_t va){uint32_t*p=sp11_pe_ptr_for_va(i,va);p[0]=0xaa1f03e0u; p[1]=0xd65f03c0u;__builtin___clear_cache((char*)p,(char*)p+8);}
static void patch_known(Sp11PeImage*i){
 pp(i,0x1800e90f8ULL,win_init_propvariant_clsid);
 pp(i,0x1800e9400ULL,cs_void);pp(i,0x1800e9408ULL,cs_void);pp(i,0x1800e9410ULL,cs_void);pp(i,0x1800e9438ULL,cs_void);pp(i,0x1800e9440ULL,cs_try);
 pp(i,0x1800e92a8ULL,win_get_module_handle_ex); pp(i,0x1800e9128ULL,win_create_guid);pp(i,0x1800e9130ULL,win_string_from_guid);pp(i,0x1800e9138ULL,win_prop_variant_clear); pp(i,0x1800e92c8ULL,win_get_module_filename);pp(i,0x1800e92e0ULL,win_find_resource);pp(i,0x1800e9290ULL,win_load_resource);pp(i,0x1800e9288ULL,win_lock_resource);pp(i,0x1800e9280ULL,win_size_resource);
 pp(i,0x1800e9018ULL,win_thread_id); pp(i,0x1800e93b8ULL,win_qpc);pp(i,0x1800e93c0ULL,win_qpf);pp(i,0x1800e90b0ULL,win_tick64);
 pp(i,0x1800e9478ULL,srw_void);pp(i,0x1800e9480ULL,srw_void);pp(i,0x1800e9078ULL,srw_try);pp(i,0x1800e9028ULL,sleep_cv);pp(i,0x1800e9030ULL,wake_all_cv);pp(i,0x1800e9038ULL,wake_cv);
 pp(i,0x1800e93a0ULL,win_create_thread); pp(i,0x1800e9418ULL,win_set_event);pp(i,0x1800e9420ULL,win_create_event);pp(i,0x1800e9428ULL,win_release_mutex);pp(i,0x1800e9448ULL,win_open_event);pp(i,0x1800e9450ULL,win_open_mutex);pp(i,0x1800e9458ULL,win_create_mutex);pp(i,0x1800e9460ULL,win_wait_one);pp(i,0x1800e9488ULL,win_reset_event);pp(i,0x1800e94a8ULL,win_wait_many);pp(i,0x1800e90a8ULL,win_wait_one_ex);pp(i,0x1800e9320ULL,win_map_view);pp(i,0x1800e9328ULL,win_open_mapping);pp(i,0x1800e9330ULL,win_create_mapping);pp(i,0x1800e9340ULL,win_unmap_view);pp(i,0x1800e9220ULL,win_close_handle);pp(i,0x1800e9538ULL,win_sddl);pp(i,0x1800e9260ULL,win_local_free);pp(i,0x1800e9190ULL,win_get_last_error);pp(i,0x1800e9198ULL,win_set_last_error);pp(i,0x1800e9238ULL,heap_get);pp(i,0x1800e9248ULL,heap_alloc);pp(i,0x1800e9230ULL,heap_free);pp(i,0x1800e9250ULL,heap_realloc);pp(i,0x1800e9240ULL,heap_size);
 pret(i,0x1800010c0ULL);pret0(i,0x180033928ULL);pret0(i,0x180033c28ULL);pret(i,0x180002848ULL);
}
int main(int ac,char**av){
 hs();const char*dll=ac>1?av[1]:DLL_DEFAULT;Sp11PeImage img;
 if(sp11_pe_load(&img,dll)){fprintf(stderr,"load failed\n");return 2;}
 g_res_data=sp11_pe_ptr_for_va(&img,RESOURCE_VA);g_module_handle=img.base;
 fprintf(stderr,"mapped=%p size=%#zx delta=%lld factory=%p res=%p/%u\n",img.base,img.size,(long long)img.delta,sp11_pe_ptr_for_va(&img,PARENT_FACTORY_VA),g_res_data,RESOURCE_SIZE);
 patch_known(&img);WinTlsCtx wt;setup_win_tls(&img,&wt);void*out=(void*)0x1111111111111111ULL;
 int32_t hr=((ParentFactoryFn)sp11_pe_ptr_for_va(&img,PARENT_FACTORY_VA))(NULL,&out);
 fprintf(stderr,"FACTORY returned hr=%#x out=%p\n",(unsigned)hr,out);if(!out||hr<0)return 3;
 uint64_t*q=out;for(int i=0;i<12;i++)fprintf(stderr,"q[%d]=%#llx\n",i,(unsigned long long)q[i]);

 fake_com_init();
 uint8_t *iface=(uint8_t*)out+8; uint8_t host[0x200] __attribute__((aligned(16))); memset(host,0,sizeof(host));
 int32_t ir=((InitializeFn)sp11_pe_ptr_for_va(&img,INITIALIZE_VA))(iface,NULL,host);
 fprintf(stderr,"INIT hr=%#x phase=%u ready=%u\n",(unsigned)ir,iface[0x2e0],iface[0x2e1]);
 typedef struct { uint64_t q[10]; } Apo3; Apo3 ctx; memset(&ctx,0,sizeof(ctx));
 ctx.q[0]=0x50; ctx.q[3]=(uint64_t)(uintptr_t)&g_endpoint; ctx.q[5]=(uint64_t)(uintptr_t)&g_collection;
 /* QI DAP base for 67b16434... */
 Guid pre={0x67b16434,0x1ef3,0x47bc,{0x8d,0x2b,0xf4,0x85,0xee,0x14,0x8c,0x32}}; void *preif=NULL;
 void **bvt=*(void***)out; int32_t qhr=((int32_t(*)(void*,const Guid*,void**))bvt[0])(out,&pre,&preif);
 fprintf(stderr,"QI preconfig hr=%#x iface=%p\n",(unsigned)qhr,preif); if(qhr<0||!preif)return 10;
 void **pvt=*(void***)preif;
 int32_t sr=(int32_t)sp11_win_call4(pvt[3],wt.teb,(uint64_t)preif,(uint64_t)&ctx,0,0);
 fprintf(stderr,"SetAPOInitParameters hr=%#x phase=%u ready=%u shared=%p endpoint=%p defaultstore=%p\n",(unsigned)sr,iface[0x2e0],iface[0x2e1],*(void**)(iface+0x530),*(void**)(iface+0x438),*(void**)(iface+0x430));
 if(sr<0 || iface[0x2e0]!=1){fprintf(stderr,"SETAPO_RESULT FAIL\n");return 11;}
 fprintf(stderr,"SETAPO_RESULT PASS\n");return 0;
}
