#include "ubig/ubig.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_sched(const char *s,size_t *v,size_t cap,size_t *n){
    *n=0;while(*s){if(*n>=cap)return -1;char*e=NULL;unsigned long x=strtoul(s,&e,10);if(e==s||x==0)return -1;v[(*n)++]=(size_t)x;if(!*e)break;if(*e!=',')return -1;s=e+1;}return *n?0:-1;
}
int main(int ac,char**av){
    if(ac<4){fprintf(stderr,"usage: %s input.f32le output.f32le profile [schedule]\n",av[0]);return 2;}
    ubig_profile p;if(ubig_profile_parse(av[3],&p)){fprintf(stderr,"unknown profile\n");return 2;}
    size_t sched[64]={480},ns=1;if(ac>4&&parse_sched(av[4],sched,64,&ns)){fprintf(stderr,"bad schedule\n");return 2;}
    FILE*fi=fopen(av[1],"rb"),*fo=fopen(av[2],"wb");if(!fi||!fo){perror("open");return 2;}
    if(fseek(fi,0,SEEK_END)){perror("seek");return 2;}long bytes=ftell(fi);rewind(fi);if(bytes<0||bytes%(2*sizeof(float))){fprintf(stderr,"bad input size\n");return 2;}
    size_t frames=(size_t)bytes/(2*sizeof(float));float*inter=malloc(frames*2*sizeof(float));float*out=malloc(frames*2*sizeof(float));float*l=malloc(frames*sizeof(float));float*r=malloc(frames*sizeof(float));float*ol=malloc(frames*sizeof(float));float*or_=malloc(frames*sizeof(float));if(!inter||!out||!l||!r||!ol||!or_)return 3;
    if(fread(inter,sizeof(float)*2,frames,fi)!=frames){fprintf(stderr,"short read\n");return 3;}fclose(fi);for(size_t i=0;i<frames;i++){l[i]=inter[2*i];r[i]=inter[2*i+1];}
    ubig_engine_config cfg={UBIG_ABI_VERSION,UBIG_SAMPLE_RATE,UBIG_CHANNELS,p};ubig_engine*e=ubig_engine_create(&cfg);if(!e)return 4;
    size_t pos=0,si=0;while(pos<frames){size_t n=sched[si++%ns];if(n>frames-pos)n=frames-pos;if(ubig_engine_process(e,l+pos,r+pos,ol+pos,or_+pos,n))return 5;pos+=n;}
    for(size_t i=0;i<frames;i++){out[2*i]=ol[i];out[2*i+1]=or_[i];}if(fwrite(out,sizeof(float)*2,frames,fo)!=frames){perror("write");return 6;}fclose(fo);ubig_engine_destroy(e);free(or_);free(ol);free(r);free(l);free(out);free(inter);return 0;
}
