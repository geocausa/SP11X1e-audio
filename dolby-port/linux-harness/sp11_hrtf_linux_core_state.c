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
#include <time.h>
#include <math.h>

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
static uint8_t *g_blobs[6]; static size_t g_blob_n[6];
static int32_t coll_item(FakeObj*o,uint32_t idx,void**out){(void)o;fprintf(stderr,"fake collection Item(%u)\n",idx);if(!out||idx>0)return (int32_t)0x80070057u;*out=&g_device;fu_add(&g_device);return 0;}
static int32_t dev_activate(FakeObj*o,const Guid*iid,uint32_t clsctx,void*params,void**out){(void)o;(void)iid;(void)clsctx;(void)params;fprintf(stderr,"fake device Activate sysfx\n");if(!out)return (int32_t)0x80004003u;*out=&g_sysfx;fu_add(&g_sysfx);return 0;}
static int32_t sysfx_open(FakeObj*o,uint32_t access,void**out){(void)o;(void)access;fprintf(stderr,"fake sysfx OpenPropertyStore\n");if(!out)return (int32_t)0x80004003u;*out=&g_store;fu_add(&g_store);return 0;}
typedef struct PropVar { uint16_t vt; uint16_t r1,r2,r3; union { void *ptr; uint64_t u64; uint32_t u32; }; uint64_t extra; } PropVar;
static uint16_t g_endpoint_guid[]= {'{','0','0','0','0','0','0','0','1','-','0','0','0','0','-','0','0','0','0','-','0','0','0','0','-','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','1','}',0};
static uint16_t g_friendly[]= {'S','P','1','1',' ','L','i','n','u','x',' ','O','r','a','c','l','e',0};
static int32_t ps_count(FakeObj*o,uint32_t*n){(void)o;if(n)*n=0;return 0;}
static int32_t ps_getat(FakeObj*o,uint32_t i,void*k){(void)o;(void)i;(void)k;return (int32_t)0x80070490u;}
static int guid_eq_raw(const void*k,const uint8_t*x){return memcmp(k,x,16)==0;}
static int32_t ps_get(FakeObj*o,const void*key,PropVar*pv){if(!pv)return (int32_t)0x80004003u;memset(pv,0,sizeof(*pv)); uint32_t pid=*(const uint32_t*)((const uint8_t*)key+16);
 static const uint8_t dahp[16]={0x55,0xab,0x4d,0x1b,0xfb,0xb1,0x8c,0x4d,0x83,0x17,0xf2,0xd4,0xa9,0x6e,0xfb,0xb8};
 if(o->kind==5){pv->vt=31;pv->ptr=(pid==14?g_friendly:g_endpoint_guid);fprintf(stderr,"endpoint GetValue pid=%u -> LPWSTR\n",pid);return 0;}
 if(guid_eq_raw(key,dahp) && pid<6 && g_blobs[pid]){pv->vt=65;pv->u32=(uint32_t)g_blob_n[pid];pv->extra=(uint64_t)(uintptr_t)g_blobs[pid];fprintf(stderr,"default GetValue DAHP pid=%u -> BLOB n=%zu first=%02x\n",pid,g_blob_n[pid],g_blobs[pid][0]);return 0;}
 const uint8_t*kb=key;fprintf(stderr,"default GetValue MISS guid=%02x%02x%02x%02x pid=%u\n",kb[3],kb[2],kb[1],kb[0],pid);return (int32_t)0x80070490u;
}
static int32_t ps_set(FakeObj*o,const void*k,const PropVar*pv){(void)o;(void)k;(void)pv;return 0;} static int32_t ps_commit(FakeObj*o){(void)o;return 0;}
static void *vt_collection[5]={(void*)fu_qi,(void*)fu_add,(void*)fu_rel,(void*)coll_count,(void*)coll_item};
static void *vt_device[7]={(void*)fu_qi,(void*)fu_add,(void*)fu_rel,(void*)dev_activate,NULL,NULL,NULL};
static void *vt_sysfx[10]={(void*)fu_qi,(void*)fu_add,(void*)fu_rel,(void*)sysfx_open,(void*)sysfx_open,(void*)sysfx_open,NULL,NULL,NULL,NULL};
static void *vt_store[8]={(void*)fu_qi,(void*)fu_add,(void*)fu_rel,(void*)ps_count,(void*)ps_getat,(void*)ps_get,(void*)ps_set,(void*)ps_commit};
static void fake_com_init(void){g_collection=(FakeObj){vt_collection,1,1};g_device=(FakeObj){vt_device,1,2};g_sysfx=(FakeObj){vt_sysfx,1,3};g_store=(FakeObj){vt_store,1,4};g_endpoint=(FakeObj){vt_store,1,5};
 for(int pid=1;pid<=5;pid++){if(pid==3)continue;char fn[256];snprintf(fn,sizeof(fn),"/tmp/sp11-dolby-asar-linux-lab/linux-harness/dahp_%d.blob",pid);FILE*f=fopen(fn,"rb");if(!f)continue;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);g_blobs[pid]=malloc(n);g_blob_n[pid]=n;fread(g_blobs[pid],1,n,f);fclose(f);}}

static int32_t win_init_propvariant_clsid(const Guid*g,PropVar*pv){if(!pv)return (int32_t)0x80004003u;memset(pv,0,sizeof(*pv));pv->vt=72;Guid*c=malloc(16);if(!c)return (int32_t)0x8007000eu;*c=*g;pv->ptr=c;fprintf(stderr,"shim InitPropVariantFromCLSID\n");return 0;}
static int sleep_cv(void*cv,void*lock,uint32_t ms,uint32_t flags){(void)cv;(void)lock;(void)ms;(void)flags;return 1;}
static void wake_cv(void*cv){(void)cv;}
static void wake_all_cv(void*cv){(void)cv;}
static void pp(Sp11PeImage*i,uint64_t va,void*f){*(uintptr_t*)sp11_pe_ptr_for_va(i,va)=(uintptr_t)f;}
static void pret(Sp11PeImage*i,uint64_t va){uint32_t*p=sp11_pe_ptr_for_va(i,va);*p=0xd65f03c0u;__builtin___clear_cache((char*)p,(char*)p+4);}
static void pret0(Sp11PeImage*i,uint64_t va){uint32_t*p=sp11_pe_ptr_for_va(i,va);p[0]=0xaa1f03e0u; p[1]=0xd65f03c0u;__builtin___clear_cache((char*)p,(char*)p+8);}


static uint64_t log_slot0(void*o,...){(void)o;return 0;}
static uint64_t log_enabled(void*o,uint32_t level,...){(void)o;(void)level;return 0;}
static void *g_log_vt[4]={(void*)log_slot0,(void*)log_enabled,(void*)log_slot0,(void*)log_slot0};
static struct { void **vt; uint64_t pad[3]; } g_log_obj={g_log_vt,{0,0,0}};
static void patch_dap_logger_factory(Sp11PeImage*i){
    /* FUN_180033928 -> return &g_log_obj. This function is diagnostic logging only. */
    uint32_t *p=(uint32_t*)sp11_pe_ptr_for_va(i,0x180033928ULL);
    p[0]=0x58000040u; /* ldr x0, [pc,#8] */
    p[1]=0xd65f03c0u; /* ret */
    *(uint64_t*)&p[2]=(uint64_t)(uintptr_t)&g_log_obj;
    __builtin___clear_cache((char*)p,(char*)p+16);
    fprintf(stderr,"patched DAP logger factory -> %p\n",(void*)&g_log_obj);
}
static void install_minimal_format_map(Sp11PeImage *i){
    uint8_t *head=calloc(1,0x48), *node=calloc(1,0x48);
    if(!head||!node){fprintf(stderr,"format map alloc failed\n");exit(90);}
    *(void**)(head+0)=node; *(void**)(head+8)=node; *(void**)(head+16)=node;
    head[0x18]=1; head[0x19]=1;
    *(void**)(node+0)=head; *(void**)(node+8)=head; *(void**)(node+16)=head;
    node[0x18]=0; node[0x19]=0;
    *(uint32_t*)(node+0x20)=0x200;
    *(uint16_t*)(node+0x28)='d'; *(uint16_t*)(node+0x2a)=0;
    *(uint64_t*)(node+0x38)=1; *(uint64_t*)(node+0x40)=7;
    *(void**)sp11_pe_ptr_for_va(i,0x180337ad8ULL)=head;
    fprintf(stderr,"minimal formatter map head=%p node=%p key=%#x suffix=d\n",head,node,*(uint32_t*)(node+0x20));
}

static void patch_known(Sp11PeImage*i){
 pp(i,0x1800e90f8ULL,win_init_propvariant_clsid);
 pp(i,0x1800e9400ULL,cs_void);pp(i,0x1800e9408ULL,cs_void);pp(i,0x1800e9410ULL,cs_void);pp(i,0x1800e9438ULL,cs_void);pp(i,0x1800e9440ULL,cs_try);
 pp(i,0x1800e92a8ULL,win_get_module_handle_ex); pp(i,0x1800e9128ULL,win_create_guid);pp(i,0x1800e9130ULL,win_string_from_guid);pp(i,0x1800e9138ULL,win_prop_variant_clear); pp(i,0x1800e92c8ULL,win_get_module_filename);pp(i,0x1800e92e0ULL,win_find_resource);pp(i,0x1800e9290ULL,win_load_resource);pp(i,0x1800e9288ULL,win_lock_resource);pp(i,0x1800e9280ULL,win_size_resource);
 pp(i,0x1800e9018ULL,win_thread_id); pp(i,0x1800e93b8ULL,win_qpc);pp(i,0x1800e93c0ULL,win_qpf);pp(i,0x1800e90b0ULL,win_tick64);
 pp(i,0x1800e9478ULL,srw_void);pp(i,0x1800e9480ULL,srw_void);pp(i,0x1800e9078ULL,srw_try);pp(i,0x1800e9028ULL,sleep_cv);pp(i,0x1800e9030ULL,wake_all_cv);pp(i,0x1800e9038ULL,wake_cv);
 pp(i,0x1800e93a0ULL,win_create_thread); pp(i,0x1800e9418ULL,win_set_event);pp(i,0x1800e9420ULL,win_create_event);pp(i,0x1800e9428ULL,win_release_mutex);pp(i,0x1800e9448ULL,win_open_event);pp(i,0x1800e9450ULL,win_open_mutex);pp(i,0x1800e9458ULL,win_create_mutex);pp(i,0x1800e9460ULL,win_wait_one);pp(i,0x1800e9488ULL,win_reset_event);pp(i,0x1800e94a8ULL,win_wait_many);pp(i,0x1800e90a8ULL,win_wait_one_ex);pp(i,0x1800e9320ULL,win_map_view);pp(i,0x1800e9328ULL,win_open_mapping);pp(i,0x1800e9330ULL,win_create_mapping);pp(i,0x1800e9340ULL,win_unmap_view);pp(i,0x1800e9220ULL,win_close_handle);pp(i,0x1800e9538ULL,win_sddl);pp(i,0x1800e9260ULL,win_local_free);pp(i,0x1800e9190ULL,win_get_last_error);pp(i,0x1800e9198ULL,win_set_last_error);pp(i,0x1800e9238ULL,heap_get);pp(i,0x1800e9248ULL,heap_alloc);pp(i,0x1800e9230ULL,heap_free);pp(i,0x1800e9250ULL,heap_realloc);pp(i,0x1800e9240ULL,heap_size);
 pret(i,0x1800010c0ULL);pret0(i,0x180033928ULL);pret0(i,0x180033c28ULL);pret(i,0x180002848ULL);
}

/* --- Original DolbyHrtfEnc.dll Linux host shims --- */
typedef Guid HGuid;
static int32_t hftm_qi(FakeObj*o,const HGuid*i,void**out){(void)i;if(!out)return (int32_t)0x80004003u;*out=o;fu_add(o);return 0;}
static void *hftm_vt[3]={(void*)hftm_qi,(void*)fu_add,(void*)fu_rel};
static FakeObj hftm={hftm_vt,1,99};
static int32_t h_CoCreateFreeThreadedMarshaler(void*outer,void**out){(void)outer;if(!out)return (int32_t)0x80004003u;*out=&hftm;fu_add(&hftm);return 0;}
static void *h_CoTaskMemAlloc(size_t n){return malloc(n?n:1);} static int h_callnewh(size_t n){(void)n;return 0;}
static void *h_malloc(size_t n){return malloc(n?n:1);} static void *h_calloc(size_t a,size_t b){return calloc(a,b);} static void h_free(void*p){free(p);}
static void *h_aligned_malloc(size_t n,size_t a){void*p=NULL;if(a<sizeof(void*))a=sizeof(void*);if(posix_memalign(&p,a,n?n:1))return NULL;return p;} static void h_aligned_free(void*p){free(p);}
static int h_initcs(void*p,unsigned spin,unsigned flags){(void)p;(void)spin;(void)flags;return 1;} static void h_void1(void*p){(void)p;}
static void *h_encode(void*p){return p;} static void *h_decode(void*p){return p;}
static uint64_t h_tick(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void h_filetime(uint64_t*out){struct timespec t;clock_gettime(CLOCK_REALTIME,&t);if(out)*out=(uint64_t)t.tv_sec*10000000ULL+t.tv_nsec/100+116444736000000000ULL;}
static int h_qpc(int64_t*out){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);if(out)*out=(int64_t)t.tv_sec*1000000000LL+t.tv_nsec;return 1;}
static uint32_t h_tid(void){return 1;} static uint32_t h_pid(void){return (uint32_t)getpid();}
static uint32_t h_eventreg(const void*a,void*b,void*c,uint64_t*d){(void)a;(void)b;(void)c;if(d)*d=1;return 0;} static uint32_t h_event0(void){return 0;}
static void h_setlast(uint32_t x){(void)x;} static uint32_t h_getlast(void){return 0;}
static int32_t h_regopen(void*a,const uint16_t*b,uint32_t c,uint32_t d,void**e){(void)a;(void)b;(void)c;(void)d;if(e)*e=NULL;return 2;} static int32_t h_regclose(void*a){(void)a;return 0;} static int32_t h_regquery(void*a,const uint16_t*b,void*c,uint32_t*d,uint8_t*e,uint32_t*f){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return 2;}
static int h_wcscmp(const uint16_t*a,const uint16_t*b){while(*a&&*a==*b){a++;b++;}return (int)*a-(int)*b;}
static int h_strcmp(const char*a,const char*b){return strcmp(a,b);} static int h_strcpy_s(char*d,size_t n,const char*s0){if(!d||!n)return 22;snprintf(d,n,"%s",s0?s0:"");return 0;}
static int64_t h_vsprintf(void*a,...){(void)a;return 0;} static float h_sinf(float x){return sinf(x);}
static void hpatch(Sp11PeImage*i,uint32_t rva,void*f){*(uintptr_t*)(i->base+rva)=(uintptr_t)f;}
static void patch_hrtf(Sp11PeImage*i){
 hpatch(i,0x18000,h_CoCreateFreeThreadedMarshaler);hpatch(i,0x18008,h_CoTaskMemAlloc);
 hpatch(i,0x18198,h_encode);hpatch(i,0x181a0,h_decode);
 hpatch(i,0x18180,h_tick);hpatch(i,0x18188,h_filetime);hpatch(i,0x180d8,h_qpc);hpatch(i,0x180c0,h_tid);hpatch(i,0x180c8,h_pid);
 hpatch(i,0x18128,h_void1);hpatch(i,0x18130,h_void1);hpatch(i,0x18138,h_void1);hpatch(i,0x18140,h_void1);hpatch(i,0x18148,h_void1);hpatch(i,0x18150,h_initcs);hpatch(i,0x18158,h_void1);hpatch(i,0x18160,h_void1);
 hpatch(i,0x182e0,h_eventreg);hpatch(i,0x182e8,h_event0);hpatch(i,0x182f0,h_event0);hpatch(i,0x182f8,h_event0);
 hpatch(i,0x18020,h_setlast);hpatch(i,0x18030,h_getlast);
 hpatch(i,0x180e8,h_regopen);hpatch(i,0x180f0,h_regclose);hpatch(i,0x180f8,h_regquery);
 hpatch(i,0x181f8,h_aligned_free);hpatch(i,0x18200,h_aligned_malloc);hpatch(i,0x18208,h_callnewh);hpatch(i,0x18210,h_calloc);hpatch(i,0x18218,h_malloc);hpatch(i,0x18220,h_free);
 hpatch(i,0x182c0,h_wcscmp);hpatch(i,0x182c8,h_strcpy_s);hpatch(i,0x182d0,h_strcmp);hpatch(i,0x182a8,h_vsprintf);hpatch(i,0x182b0,h_vsprintf);hpatch(i,0x18230,h_sinf);
}
extern uint64_t sp11_win_call9(void*,void*,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
__asm__(
".text\n.align 2\n.global sp11_win_call9\n.type sp11_win_call9,%function\n"
"sp11_win_call9:\n"
" stp x29,x30,[sp,#-32]!\n str x18,[sp,#16]\n mov x29,sp\n mov x16,x0\n mov x18,x1\n mov x0,x2\n mov x1,x3\n mov x2,x4\n mov x3,x5\n mov x4,x6\n mov x5,x7\n ldr x6,[sp,#32]\n ldr x7,[sp,#40]\n ldr x8,[sp,#48]\n sub sp,sp,#16\n str x8,[sp]\n blr x16\n add sp,sp,#16\n ldr x18,[sp,#16]\n ldp x29,x30,[sp],#32\n ret\n"
".size sp11_win_call9,.-sp11_win_call9\n");

extern uint64_t sp11_win_call_mix(void*,void*,void*,float*,uint32_t,uint32_t,float,uint32_t);
__asm__(
".text\n.align 2\n.global sp11_win_call_mix\n.type sp11_win_call_mix,%function\n"
"sp11_win_call_mix:\n"
" stp x29,x30,[sp,#-32]!\n str x18,[sp,#16]\n mov x29,sp\n mov x16,x0\n mov x18,x1\n mov x0,x2\n mov x1,x3\n mov x2,x4\n mov x3,x5\n mov x4,x6\n blr x16\n ldr x18,[sp,#16]\n ldp x29,x30,[sp],#32\n ret\n"
".size sp11_win_call_mix,.-sp11_win_call_mix\n");

extern uint64_t sp11_win_call_obj(void*,void*,void*,uint64_t,int*,float*,uint32_t,float);
__asm__(
".text\n.align 2\n.global sp11_win_call_obj\n.type sp11_win_call_obj,%function\n"
"sp11_win_call_obj:\n"
" stp x29,x30,[sp,#-32]!\n str x18,[sp,#16]\n mov x29,sp\n mov x16,x0\n mov x18,x1\n mov x0,x2\n mov x1,x3\n mov x2,x4\n mov x3,x5\n mov x4,x6\n blr x16\n ldr x18,[sp,#16]\n ldp x29,x30,[sp],#32\n ret\n"
".size sp11_win_call_obj,.-sp11_win_call_obj\n");
static int32_t hqi(void*o,const HGuid*g,void**out){void**v=*(void***)o;return ((int32_t(*)(void*,const HGuid*,void**))v[0])(o,g,out);}

int main(int ac,char**av){
 hs(); if(ac<5){fprintf(stderr,"usage: %s DAP.dll HRTF.dll freq amp\n",av[0]);return 2;} double freq=strtod(av[3],NULL),amp=strtod(av[4],NULL);
 /* DAP */ Sp11PeImage img; if(sp11_pe_load(&img,av[1]))return 3; g_res_data=sp11_pe_ptr_for_va(&img,RESOURCE_VA);g_module_handle=img.base;patch_known(&img);WinTlsCtx wt;setup_win_tls(&img,&wt);install_minimal_format_map(&img);patch_dap_logger_factory(&img);fake_com_init();
 void*dap=NULL;int32_t hr=((ParentFactoryFn)sp11_pe_ptr_for_va(&img,PARENT_FACTORY_VA))(NULL,&dap);fprintf(stderr,"DAP factory hr=%#x obj=%p\n",(unsigned)hr,dap);if(hr<0||!dap)return 4;
 /* HRTF */ Sp11PeImage hi;if(sp11_pe_load(&hi,av[2]))return 5;patch_hrtf(&hi);uint32_t flags=1;void*fac=NULL,*hinst=NULL;typedef int32_t(*FC)(uint32_t*,void*,const HGuid*,void**);FC fc=(FC)(hi.base+0x3c10);hr=fc(&flags,hi.base+0x19fa8,(const HGuid*)(hi.base+0x1a150),&fac);fprintf(stderr,"HRTF factory hr=%#x fac=%p\n",(unsigned)hr,fac);if(hr<0||!fac)return 6;void**fvt=*(void***)fac;hr=((int32_t(*)(void*,void**))fvt[6])(fac,&hinst);fprintf(stderr,"HRTF activate hr=%#x obj=%p\n",(unsigned)hr,hinst);if(hr<0||!hinst)return 7;
 HGuid setupid={0xdc57ddb4,0xe086,0x49ec,{0xb1,0x3d,0xec,0xcd,0xd5,0x12,0x99,0x0c}}, asarid={0xdeff1192,0xf581,0x4d77,{0x9c,0x1b,0x3e,0x59,0x6b,0x0c,0xa9,0x89}};void*setup=NULL,*asar=NULL;hr=hqi(hinst,&setupid,&setup);if(hr<0)return 8;hr=hqi(hinst,&asarid,&asar);if(hr<0)return 9;
 void**sv=*(void***)setup; hr=(int32_t)sp11_win_call4(sv[3],wt.teb,(uint64_t)setup,(uint64_t)dap,0,0);fprintf(stderr,"SetEncoderEngine hr=%#x\n",(unsigned)hr);if(hr<0)return 10;
 typedef struct{uint64_t q[10];}Apo3;Apo3 ctx;memset(&ctx,0,sizeof(ctx));ctx.q[0]=0x50;ctx.q[3]=(uint64_t)(uintptr_t)&g_endpoint;ctx.q[5]=(uint64_t)(uintptr_t)&g_collection;hr=(int32_t)sp11_win_call4(sv[4],wt.teb,(uint64_t)setup,(uint64_t)&ctx,0,0);fprintf(stderr,"HRTF SetAPOContext hr=%#x\n",(unsigned)hr);if(hr<0)return 11;
 HGuid zero={0};void**avt=*(void***)asar;fprintf(stderr,"calling real IAsarEncoder2::Initialize rva=%#llx with 256/48000/objectmask=probe/stereo\n",(unsigned long long)((uint8_t*)avt[3]-hi.base));
 uint64_t r=sp11_win_call9(avt[3],wt.teb,(uint64_t)asar,256,48000,0xFFFFE,2,3,0,(uint64_t)&zero,0);hr=(int32_t)r;
 fprintf(stderr,"HRTF Initialize hr=%#x stereoBypass=%u engine=%p\n",(unsigned)hr,*((uint8_t*)hinst+0x58),*(void**)((uint8_t*)hinst+0x60));
 uint8_t*dapi=(uint8_t*)dap+8;fprintf(stderr,"DAP after HRTF phase=%u ready=%u dapvr=%p aide=%p oar=%p vlldp=%p frame=%u total=%u\n",dapi[0x2e0],dapi[0x2e1],*(void**)(dapi+0x538),*(void**)(dapi+0x648),*(void**)(dapi+0x690),*(void**)(dapi+0x740),*(uint32_t*)(dapi+0x30),*(uint32_t*)(dapi+0x4c));
 {uint8_t*w=*(uint8_t**)(dapi+0x538);uint8_t*c=*(uint8_t**)(w+0x50);fprintf(stderr,"LINUX_CORE_STATE wrapper=%p core=%p lvl_amount=%d lvl_enable=%u lvl_drc=%u lvl_out=%g lvl_in=%g reg_enable=%u reg_timbre=%d reg_overdrive=%d reg_relax=%d reg_spkdist=%u dirty=%u\n",w,c,*(int*)(c+0x7bc),*(unsigned*)(c+0x7c4),*(unsigned*)(c+0x7d4),*(float*)(c+0x72c),*(float*)(c+0x734),*(unsigned*)(c+0xf38),*(int*)(c+0xf10),*(int*)(c+0xf1c),*(int*)(c+0xf28),*(unsigned*)(c+0xf30),*(unsigned*)(c+0x13b0));}

 if(hr<0){fprintf(stderr,"HRTF_FULL_INIT_RESULT FAIL\n");return 12;} fprintf(stderr,"HRTF_FULL_INIT_RESULT PASS\n"); return 0;
 /* Live VirtualSurround exposure: 19 static ASAR objects; FL/FR carry stereo, 17 silent. */
 enum{N=256, BLOCKS=400, OBJS=19}; float in[N*2],objbuf[OBJS][N],outpcm[N*2];
 int desc[OBJS][8]; memset(desc,0,sizeof(desc));
 for(unsigned j=0;j<OBJS;j++){desc[j][0]=(int)(j+1);desc[j][2]=(int)(2u<<j);desc[j][3]=1;desc[j][7]=0x3f800000;}
 float last_pk=0,last_diff=0,max_pk=0,tail_pk=0;uint64_t total_prod=0;int fail=0;
 for(unsigned b=0;b<BLOCKS;b++){
   for(unsigned i=0;i<N;i++){uint64_t n=(uint64_t)b*N+i;float x=(float)amp*sinf((float)(2.0*M_PI*freq*(double)n/48000.0));in[2*i]=x;in[2*i+1]=x;objbuf[0][i]=x;objbuf[1][i]=x;for(unsigned j=2;j<OBJS;j++)objbuf[j][i]=0.0f;outpcm[2*i]=outpcm[2*i+1]=0.0f;}
   for(unsigned j=0;j<OBJS;j++){uint64_t o=sp11_win_call_obj(avt[4],wt.teb,asar,j,desc[j],objbuf[j],N,1.0f);if((int32_t)o<0){fprintf(stderr,"OBJ19_SET_FAIL block=%u obj=%u type=%#x hr=%#x\n",b,j,desc[j][2],(unsigned)(uint32_t)o);fail=1;break;}}
   if(fail)break;
   uint64_t mr=sp11_win_call_mix(avt[6],wt.teb,asar,in,N,2,1.0f,0);uint32_t produced=0;uint32_t cap=sizeof(outpcm);uint64_t pr=sp11_win_call4(avt[7],wt.teb,(uint64_t)asar,cap,(uint64_t)outpcm,(uint64_t)&produced);
   if((int32_t)mr<0||(int32_t)pr<0||produced!=cap){fprintf(stderr,"OBJ19_FAIL block=%u mix=%#x proc=%#x prod=%u\n",b,(unsigned)(uint32_t)mr,(unsigned)(uint32_t)pr,produced);fail=1;break;}
   float pk=0,md=0;for(unsigned i=0;i<N*2;i++){float a=fabsf(outpcm[i]);if(a>pk)pk=a;float d=fabsf(outpcm[i]-in[i]);if(d>md)md=d;}last_pk=pk;last_diff=md;if(pk>max_pk)max_pk=pk;if(b>=BLOCKS-50&&pk>tail_pk)tail_pk=pk;total_prod+=produced;
 }
 fprintf(stderr,"OBJ19_CURVE_RESULT freq=%.3f amp=%.6f mask=0xffffe objects=19 active=FL+FR blocks=%d seconds=%.3f last_peak=%.9f max_peak=%.9f tail50_peak=%.9f last_diff=%.9f bytes=%llu result=%s\n",freq,amp,BLOCKS,(double)BLOCKS*N/48000.0,last_pk,max_pk,tail_pk,last_diff,(unsigned long long)total_prod,fail?"FAIL":"PASS");return fail?13:0;
}
