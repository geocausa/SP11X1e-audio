#define _GNU_SOURCE
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sp11_dolby_windows_chain_ladspa.c"

typedef struct { float *stereo; uint32_t frames, rate; } Wav;

static uint16_t r16(FILE *f){ uint16_t v=0; if(fread(&v,2,1,f)!=1) return 0; return v; }
static uint32_t r32(FILE *f){ uint32_t v=0; if(fread(&v,4,1,f)!=1) return 0; return v; }
static int read_wav16_stereo(const char *path,Wav *w){
    memset(w,0,sizeof(*w)); FILE *f=fopen(path,"rb"); if(!f)return -1;
    char id[4]; if(fread(id,1,4,f)!=4||memcmp(id,"RIFF",4)){fclose(f);return -2;} (void)r32(f);
    if(fread(id,1,4,f)!=4||memcmp(id,"WAVE",4)){fclose(f);return -3;}
    uint16_t fmt=0,ch=0,bits=0; uint32_t rate=0,data=0; long data_pos=0;
    while(fread(id,1,4,f)==4){ uint32_t n=r32(f); long next=ftell(f)+(long)n+(n&1u);
        if(!memcmp(id,"fmt ",4)){fmt=r16(f);ch=r16(f);rate=r32(f);(void)r32(f);(void)r16(f);bits=r16(f);}
        else if(!memcmp(id,"data",4)){data=n;data_pos=ftell(f);break;}
        if(fseek(f,next,SEEK_SET)){fclose(f);return -4;}
    }
    if(fmt!=1||ch!=2||bits!=16||rate!=48000||!data_pos){fclose(f);return -5;}
    uint32_t frames=data/4; int16_t *pcm=malloc(data); float *x=malloc((size_t)frames*2*sizeof(float));
    if(!pcm||!x){free(pcm);free(x);fclose(f);return -6;} fseek(f,data_pos,SEEK_SET);
    if(fread(pcm,4,frames,f)!=frames){free(pcm);free(x);fclose(f);return -7;}
    fclose(f); for(uint32_t i=0;i<frames*2;i++)x[i]=(float)pcm[i]/32768.0f; free(pcm);
    w->stereo=x;w->frames=frames;w->rate=rate;return 0;
}
static int load_target(const char *path,int32_t out[20]){
    FILE *f=fopen(path,"r"); if(!f)return -1; char line[256]; unsigned n=0;
    while(fgets(line,sizeof(line),f)&&n<20){
        /* CSV is offset,int32,hex; select text after first comma. */
        char *c=strchr(line,','); if(!c)continue; c++; while(*c=='"'||*c==' '||*c=='\t')c++;
        char *e=NULL; long v=strtol(c,&e,10); if(e==c)continue; out[n++]=(int32_t)v;
    }
    fclose(f); return n==20?0:-2;
}
static double mae20(const int32_t *a,const int32_t *b){double s=0;for(int i=0;i<20;i++)s+=fabs((double)a[i]-b[i]);return s/20.0;}
static double rmse20(const int32_t *a,const int32_t *b){double s=0;for(int i=0;i<20;i++){double z=(double)a[i]-b[i];s+=z*z;}return sqrt(s/20.0);}
static void read_vec(void *core,size_t off,int32_t out[20]){memcpy(out,(uint8_t*)core+off,20*sizeof(int32_t));}
static void print_vec(const char *tag,const int32_t v[20]){printf("%s",tag);for(int i=0;i<20;i++)printf("%s%d",i?",":"",v[i]);putchar('\n');}

int main(int ac,char **av){
    if(ac<4){fprintf(stderr,"usage: %s TONE.wav TARGET_GAIN.csv SCALE [CHUNK]\n",av[0]);return 2;}
    Wav w; if(read_wav16_stereo(av[1],&w)){fprintf(stderr,"wav read failed\n");return 3;}
    int32_t target[20]; if(load_target(av[2],target)){fprintf(stderr,"target read failed\n");return 4;}
    double scale=strtod(av[3],0); unsigned chunk=ac>4?(unsigned)strtoul(av[4],0,0):480; if(!chunk)chunk=480;
    setenv("SP11_DOLBY_PROFILE","dynamic",1);
    ChainInst *p=(ChainInst*)chain_instantiate(NULL,48000); if(!p){fprintf(stderr,"instantiate failed\n");return 5;}
    float bypass=0.0f; chain_connect(p,4,&bypass); chain_activate(p); if(!p->ready){fprintf(stderr,"activate failed\n");return 6;}
    float *il=malloc((size_t)chunk*sizeof(float)),*ir=malloc((size_t)chunk*sizeof(float));
    float *ol=malloc((size_t)chunk*sizeof(float)),*orr=malloc((size_t)chunk*sizeof(float));
    if(!il||!ir||!ol||!orr)return 7;
    void *core=(void*)(uintptr_t)vl_r64(p->vl_inner,0x28); if(!core)return 8;
    double best_mae=1e99,best_rmse=1e99; uint32_t best_end=0; int32_t best[20]={0},last[20]={0};
    uint32_t pos=0,calls=0;
    while(pos<w.frames){
        uint32_t n=w.frames-pos; if(n>chunk)n=chunk;
        for(uint32_t i=0;i<n;i++){il[i]=(float)(w.stereo[2*(pos+i)]*scale);ir[i]=(float)(w.stereo[2*(pos+i)+1]*scale);}
        chain_connect(p,0,il);chain_connect(p,1,ir);chain_connect(p,2,ol);chain_connect(p,3,orr);chain_run(p,n);
        int32_t v[20]; read_vec(core,0xc0c,v); double m=mae20(v,target),r=rmse20(v,target);
        if(m<best_mae){best_mae=m;best_rmse=r;best_end=pos+n;memcpy(best,v,sizeof(best));}
        memcpy(last,v,sizeof(last)); pos+=n;calls++;
    }
    printf("RESULT scale=%.9g chunk=%u frames=%u calls=%u best_end=%u best_time=%.9f best_mae=%.6f best_rmse=%.6f final_mae=%.6f final_rmse=%.6f\n",
           scale,chunk,w.frames,calls,best_end,best_end/48000.0,best_mae,best_rmse,mae20(last,target),rmse20(last,target));
    print_vec("TARGET=",target);print_vec("BEST=",best);print_vec("FINAL=",last);
    chain_cleanup(p); free(w.stereo);free(il);free(ir);free(ol);free(orr); return 0;
}
