#define _GNU_SOURCE
#include <dlfcn.h>
#include <ladspa.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

typedef const LADSPA_Descriptor *(*DescFn)(unsigned long);

static uint64_t fnv1a(const void *vp,size_t n){
    const unsigned char *p=vp; uint64_t h=1469598103934665603ULL;
    for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ULL;} return h;
}
static uint32_t rng=0x31415926u;
static float noise(void){rng=rng*1664525u+1013904223u;return ((float)((rng>>8)&0xffffffu)/(float)0x800000u)-1.0f;}

static int run_block(const LADSPA_Descriptor*d,LADSPA_Handle h,const float *x,unsigned n,float *out){
    float il[480],ir[480],ol[480],or_[480],bypass=0.0f;
    if(n>480)return -1;
    for(unsigned i=0;i<n;i++){il[i]=x[2*i];ir[i]=x[2*i+1];ol[i]=or_[i]=0.0f;}
    d->connect_port(h,0,il);d->connect_port(h,1,ir);d->connect_port(h,2,ol);d->connect_port(h,3,or_);d->connect_port(h,4,&bypass);d->run(h,n);
    if(out)for(unsigned i=0;i<n;i++){out[2*i]=ol[i];out[2*i+1]=or_[i];}
    return 0;
}

int main(int ac,char **av){setenv("SP11_DOLBY_CONTROL_PATH","off",1);
    const char *so=ac>1?av[1]:"./sp11_dolby_windows_chain.so";
    void *lib=dlopen(so,RTLD_NOW|RTLD_LOCAL); if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
    DescFn df=(DescFn)dlsym(lib,"ladspa_descriptor"); const LADSPA_Descriptor *d=df?df(0):NULL; if(!d)return 3;
    LADSPA_Handle a=d->instantiate(d,48000),b=d->instantiate(d,48000); if(!a||!b)return 4;
    if(d->activate){d->activate(a);d->activate(b);}

    /* Warm both instances identically for 70 seconds.  This intentionally
       exercises the shipped Dolby long-memory leveler/regulator state. */
    const uint64_t warm_frames=70ULL*48000ULL; uint64_t pos=0; float in[960];
    while(pos<warm_frames){
        unsigned n=(unsigned)((warm_frames-pos)>480?480:(warm_frames-pos));
        for(unsigned i=0;i<n;i++){
            double t=(double)(pos+i)/48000.0; float z=.035f*noise();
            in[2*i]=.18f*sinf((float)(2*M_PI*113*t))+.12f*sinf((float)(2*M_PI*997*t))+z;
            in[2*i+1]=.15f*sinf((float)(2*M_PI*181*t))+.10f*sinf((float)(2*M_PI*701*t))-.7f*z;
        }
        if(run_block(d,a,in,n,NULL)||run_block(d,b,in,n,NULL))return 5;
        pos+=n;
    }

    /* Give reference A an explicit 1776-frame zero drain. PipeWire pauses
       immediately and calls LADSPA activate(), so B must perform the same
       discarded drain internally without reconstructing either long-memory
       Dolby core. */
    float silence[960]={0};
    uint32_t drain_left=1776;
    while(drain_left){
        unsigned n=drain_left>480?480:drain_left;
        if(run_block(d,a,silence,n,NULL))return 8;
        drain_left-=n;
    }
    if(d->activate)d->activate(b);

    /* The first block after a wake is often silence before the notification
       waveform. It must not expose a different (undrained) media history. */
    float wake_a[512],wake_b[512];
    if(run_block(d,a,silence,256,wake_a)||run_block(d,b,silence,256,wake_b))return 9;
    size_t wake_diff=0;float wake_peak=0;
    for(size_t i=0;i<512;i++){
        if(memcmp(wake_a+i,wake_b+i,sizeof(float)))wake_diff++;
        float q=fabsf(wake_b[i]);if(q>wake_peak)wake_peak=q;
    }

    const uint64_t probe_frames=3ULL*48000ULL; float *oa=calloc(probe_frames*2,sizeof(float)),*ob=calloc(probe_frames*2,sizeof(float));
    if(!oa||!ob)return 6;
    pos=0;
    while(pos<probe_frames){
        unsigned n=(unsigned)((probe_frames-pos)>480?480:(probe_frames-pos));
        for(unsigned i=0;i<n;i++){
            double t=(double)(pos+i)/48000.0;
            in[2*i]=.16f*sinf((float)(2*M_PI*997*t))+.04f*sinf((float)(2*M_PI*113*t));
            in[2*i+1]=.13f*sinf((float)(2*M_PI*701*t))+.035f*sinf((float)(2*M_PI*181*t));
        }
        if(run_block(d,a,in,n,oa+2*pos)||run_block(d,b,in,n,ob+2*pos))return 7;
        pos+=n;
    }

    size_t diff=0; float maxd=0; double sa=0,sb=0;
    for(size_t i=0;i<probe_frames*2;i++){
        if(memcmp(oa+i,ob+i,sizeof(float))){diff++;float q=fabsf(oa[i]-ob[i]);if(q>maxd)maxd=q;}
        sa+=(double)oa[i]*oa[i]; sb+=(double)ob[i]*ob[i];
    }
    printf("warm_frames=%"PRIu64" warm_seconds=%.3f\n",warm_frames,(double)warm_frames/48000.0);
    printf("reference_hash=%016"PRIx64" rms=%.9f\n",fnv1a(oa,probe_frames*2*sizeof(float)),sqrt(sa/(probe_frames*2)));
    printf("pause_callback_hash=%016"PRIx64" rms=%.9f\n",fnv1a(ob,probe_frames*2*sizeof(float)),sqrt(sb/(probe_frames*2)));
    printf("diff_samples=%zu max_abs_diff=%.9g\n",diff,maxd);
    printf("pause_drain_frames=1776\n");
    printf("wake_diff_samples=%zu wake_peak=%.9g\n",wake_diff,wake_peak);
    int ok=(diff==0&&wake_diff==0);
    printf("LIFECYCLE_RESULT %s\n",ok?"PASS":"FAIL");
    if(d->cleanup){d->cleanup(a);d->cleanup(b);} free(oa);free(ob);dlclose(lib); return ok?0:20;
}
