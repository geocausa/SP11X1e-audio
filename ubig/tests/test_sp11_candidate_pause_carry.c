#define _GNU_SOURCE
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
    const LADSPA_Descriptor *d;
    LADSPA_Handle h;
    float in_l[480],in_r[480],out_l[480],out_r[480];
    float bypass,profile;
} Inst;

static int make_inst(void *lib,const char *pack,const char *control,Inst *x)
{
    setenv("UBIG_SP11_STAGEB_PACK",pack,1);
    setenv("UBIG_CONTROL_PATH",control,1);
    setenv("UBIG_PROFILE","movie",1);
    setenv("UBIG_GEQ","off",1);
    unlink(control);
    DescriptorFn get=(DescriptorFn)dlsym(lib,"ladspa_descriptor");
    x->d=get?get(0):NULL;
    if(!x->d)return -1;
    x->h=x->d->instantiate(x->d,48000);
    if(!x->h)return -2;
    x->bypass=0.0f;x->profile=0.0f;
    x->d->connect_port(x->h,0,x->in_l);x->d->connect_port(x->h,1,x->in_r);
    x->d->connect_port(x->h,2,x->out_l);x->d->connect_port(x->h,3,x->out_r);
    x->d->connect_port(x->h,4,&x->bypass);x->d->connect_port(x->h,5,&x->profile);
    if(x->d->activate)x->d->activate(x->h);
    return 0;
}

static void tone(Inst *x,unsigned n,uint64_t base)
{
    for(unsigned i=0;i<n;i++){
        float s=.20f*sinf(2.f*(float)M_PI*997.f*(float)(base+i)/48000.f);
        x->in_l[i]=x->in_r[i]=s;
    }
}
static void zero_in(Inst *x,unsigned n){memset(x->in_l,0,n*sizeof(float));memset(x->in_r,0,n*sizeof(float));}
static double max_abs(const float *p,unsigned n){double m=0;for(unsigned i=0;i<n;i++){double a=fabs((double)p[i]);if(a>m)m=a;}return m;}
static uint64_t fnv1a(uint64_t h,const void *vp,size_t n){const unsigned char*p=vp;for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;}

int main(int argc,char **argv)
{
    if(argc!=3){fprintf(stderr,"usage: %s CANDIDATE_SO PRIVATE_PACK\n",argv[0]);return 2;}
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 3;}
    char ca[128],cb[128];snprintf(ca,sizeof ca,"/tmp/ubig-pause-a-%ld",(long)getpid());snprintf(cb,sizeof cb,"/tmp/ubig-pause-b-%ld",(long)getpid());
    Inst a={0},b={0};if(make_inst(lib,argv[2],ca,&a)||make_inst(lib,argv[2],cb,&b))return 4;

    uint64_t base=0;
    /* Match the historical Golden lifecycle regression: warm persistent adaptive
       state for 70 seconds before testing the pause boundary. */
    for(unsigned block=0;block<7000;block++){
        tone(&a,480,base);tone(&b,480,base);base+=480;
        a.d->run(a.h,480);b.d->run(b.h,480);
        if(memcmp(a.out_l,b.out_l,sizeof a.out_l)||memcmp(a.out_r,b.out_r,sizeof a.out_r)){
            fprintf(stderr,"instances diverged during warmup at block %u\n",block);return 5;
        }
    }

    /* Reference A receives the accepted explicit 1776-frame zero drain. */
    unsigned left=1776;
    while(left){unsigned n=left>480?480:left;zero_in(&a,n);a.d->run(a.h,n);left-=n;}
    /* Candidate B must perform exactly the same discarded drain on activate(). */
    if(b.d->activate)b.d->activate(b.h);

    uint64_t ha=UINT64_C(1469598103934665603),hb=ha;
    double wake_peak=0,maxdiff=0;
    for(unsigned block=0;block<300;block++){
        zero_in(&a,480);zero_in(&b,480);
        a.d->run(a.h,480);b.d->run(b.h,480);
        ha=fnv1a(ha,a.out_l,sizeof a.out_l);ha=fnv1a(ha,a.out_r,sizeof a.out_r);
        hb=fnv1a(hb,b.out_l,sizeof b.out_l);hb=fnv1a(hb,b.out_r,sizeof b.out_r);
        for(unsigned i=0;i<480;i++){
            double dl=fabs((double)a.out_l[i]-(double)b.out_l[i]);
            double dr=fabs((double)a.out_r[i]-(double)b.out_r[i]);
            if(dl>maxdiff)maxdiff=dl;
            if(dr>maxdiff)maxdiff=dr;
        }
        if(block==0){double p=max_abs(b.out_l,480);double q=max_abs(b.out_r,480);wake_peak=p>q?p:q;}
    }
    printf("pause-drain reference_hash=%016llx activate_hash=%016llx maxdiff=%.9g wake_peak=%.9g frames=1776\n",
           (unsigned long long)ha,(unsigned long long)hb,maxdiff,wake_peak);
    int bad=(ha!=hb)||(maxdiff!=0.0)||(wake_peak>=1e-3);
    a.d->cleanup(a.h);b.d->cleanup(b.h);unlink(ca);unlink(cb);dlclose(lib);
    if(bad){fprintf(stderr,"FAIL pause-drain lifecycle parity\n");return 6;}
    puts("PASS candidate 1776-frame pause-drain lifecycle parity");
    return 0;
}
