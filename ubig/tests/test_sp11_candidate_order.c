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
typedef struct {const LADSPA_Descriptor*d;LADSPA_Handle h;float bypass,profile;} Inst;
static int make(void*lib,const char*ctrl,const char*pack,Inst*x){
 setenv("UBIG_CONTROL_PATH",ctrl,1);setenv("UBIG_SP11_STAGEB_PACK",pack,1);setenv("UBIG_PROFILE","movie",1);setenv("UBIG_GEQ","flat",1);
 DescriptorFn f=(DescriptorFn)dlsym(lib,"ladspa_descriptor");x->d=f?f(0):0;if(!x->d)return -1;x->h=x->d->instantiate(x->d,48000);if(!x->h)return -2;
 x->bypass=0;x->profile=0;x->d->connect_port(x->h,4,&x->bypass);x->d->connect_port(x->h,5,&x->profile);if(x->d->activate)x->d->activate(x->h);return 0;}
static void run(Inst*x,float*il,float*ir,float*ol,float*orr,unsigned n){x->d->connect_port(x->h,0,il);x->d->connect_port(x->h,1,ir);x->d->connect_port(x->h,2,ol);x->d->connect_port(x->h,3,orr);x->d->run(x->h,n);}
typedef struct {double c500[2],s500[2],c997[2],s997[2];uint64_t n;} Acc;
static void add(Acc*a,const float*l,const float*r,uint64_t base,unsigned n){for(unsigned i=0;i<n;i++){double t=(double)(base+i)/48000.0;double c5=cos(2*M_PI*500*t),s5=sin(2*M_PI*500*t),c9=cos(2*M_PI*997*t),s9=sin(2*M_PI*997*t);double y[2]={l[i],r[i]};for(int ch=0;ch<2;ch++){a->c500[ch]+=y[ch]*c5;a->s500[ch]+=y[ch]*s5;a->c997[ch]+=y[ch]*c9;a->s997[ch]+=y[ch]*s9;}a->n++;}}
static double amp(double c,double s,uint64_t n){return 2.0*hypot(c,s)/(double)n;}
static void report(const char*name,Acc*a){double A5[2],A9[2];for(int ch=0;ch<2;ch++){A5[ch]=amp(a->c500[ch],a->s500[ch],a->n);A9[ch]=amp(a->c997[ch],a->s997[ch],a->n);}double m5=sqrt((A5[0]*A5[0]+A5[1]*A5[1])/2),m9=sqrt((A9[0]*A9[0]+A9[1]*A9[1])/2);printf("%s amp500=%.12f amp997=%.12f ratio=%.9f\n",name,m5,m9,20*log10(m5/m9));}
int main(int ac,char**av){if(ac!=3){fprintf(stderr,"usage: %s CANDIDATE_SO PRIVATE_PACK\n",av[0]);return 2;}char ctrl[128];snprintf(ctrl,sizeof ctrl,"/tmp/ubig-order-%ld",(long)getpid());unlink(ctrl);ubig_control_handle pre;if(ubig_control_open(&pre,ctrl,1)||ubig_control_request_postgain(&pre,-545))return 3;ubig_control_close(&pre);
 void*lib=dlopen(av[1],RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 4;}Inst x={0};if(make(lib,ctrl,av[2],&x))return 5;
 enum{N=480};float il[N],ir[N],ol[N],orr[N];uint64_t base=0;Acc a10={0},a50={0};
 // 20 s at 10%, measure final 4 s
 for(int b=0;b<2000;b++){for(int i=0;i<N;i++){double t=(double)(base+i)/48000.0;float v=.12f*(sinf(2*M_PI*500*t)+sinf(2*M_PI*997*t));il[i]=ir[i]=v;}run(&x,il,ir,ol,orr,N);if(b>=1600)add(&a10,ol,orr,base,N);base+=N;}
 ubig_control_handle c;if(ubig_control_open(&c,ctrl,0)||ubig_control_request_postgain(&c,-167))return 6;
 // 10 s at 50%, measure final 4 s
 for(int b=0;b<1000;b++){for(int i=0;i<N;i++){double t=(double)(base+i)/48000.0;float v=.12f*(sinf(2*M_PI*500*t)+sinf(2*M_PI*997*t));il[i]=ir[i]=v;}run(&x,il,ir,ol,orr,N);if(b>=600)add(&a50,ol,orr,base,N);base+=N;}
 ubig_control_page pg;ubig_control_snapshot(&c,&pg);printf("ack postgain=%d req/ack=%u/%u profile=%u error=%d\n",pg.active_postgain,pg.postgain_request_generation,pg.postgain_ack_generation,pg.active_profile,pg.last_error);report("10",&a10);report("50",&a50);
 double a5_10=sqrt((pow(amp(a10.c500[0],a10.s500[0],a10.n),2)+pow(amp(a10.c500[1],a10.s500[1],a10.n),2))/2),a9_10=sqrt((pow(amp(a10.c997[0],a10.s997[0],a10.n),2)+pow(amp(a10.c997[1],a10.s997[1],a10.n),2))/2),a5_50=sqrt((pow(amp(a50.c500[0],a50.s500[0],a50.n),2)+pow(amp(a50.c500[1],a50.s500[1],a50.n),2))/2),a9_50=sqrt((pow(amp(a50.c997[0],a50.s997[0],a50.n),2)+pow(amp(a50.c997[1],a50.s997[1],a50.n),2))/2);
 double d500=20*log10(a5_50/a5_10),d997=20*log10(a9_50/a9_10),shape=20*log10((a5_50/a9_50)/(a5_10/a9_10));
 printf("candidate order delta500=%+.9f delta997=%+.9f shape=%+.9f\n",d500,d997,shape);
 /* Native Windows same-process 10%%->50%% oracle is -2.72771/-0.17188 dB,
  * shape -2.55583 dB.  The obsolete VLLDP->VR order produces only about
  * -0.56 dB shape change, so this wide deterministic gate catches an order
  * regression without overfitting tiny compiler/state differences. */
 int bad=!(d500<-2.60 && d500>-2.90 && d997<-0.10 && d997>-0.25 && shape<-2.40 && shape>-2.75);
 ubig_control_close(&c);x.d->cleanup(x.h);unlink(ctrl);dlclose(lib);
 if(bad){fprintf(stderr,"FAIL candidate sample order regression\n");return 7;}
 puts("PASS candidate VR->VLLDP high-volume order regression");return 0;}
