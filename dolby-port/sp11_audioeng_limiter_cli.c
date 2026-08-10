#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sp11_audioeng_limiter.h"

static int parse_pattern(const char *text,unsigned *out,size_t cap,size_t *count){
    char *copy=strdup(text),*save=NULL,*tok;if(!copy)return -1;*count=0;
    for(tok=strtok_r(copy,",",&save);tok;tok=strtok_r(NULL,",",&save)){
        char *end=NULL;unsigned long v=strtoul(tok,&end,0);
        if(!*tok||*end||v==0||v>10000000||*count>=cap){free(copy);return -1;}
        out[(*count)++]=(unsigned)v;
    }
    free(copy);return *count?0:-1;
}

int main(int argc,char **argv){
    if(argc<3||argc>4){fprintf(stderr,"usage: %s input.f32 output.f32 [chunk,pattern]\n",argv[0]);return 2;}
    FILE *fi=fopen(argv[1],"rb"),*fo=fopen(argv[2],"wb");if(!fi||!fo){perror("open");return 3;}
    if(fseek(fi,0,SEEK_END)||ftell(fi)<0){return 4;}long bytes=ftell(fi);rewind(fi);
    if(bytes%8){fprintf(stderr,"input is not stereo float32\n");return 5;}
    size_t frames=(size_t)bytes/8;float *input=malloc((size_t)bytes);if(!input)return 6;
    if(fread(input,8,frames,fi)!=frames)return 7;
    fclose(fi);
    unsigned pattern[32]={480};size_t np=1;if(argc==4&&parse_pattern(argv[3],pattern,32,&np))return 8;
    Sp11AudioEngLimiter l;sp11_audioeng_limiter_init(&l);size_t pos=0,pi=0;
    while(pos<frames+SP11_AUDIOENG_LIMITER_LOOKAHEAD){
        unsigned take=pattern[pi++%np];size_t remain=frames+SP11_AUDIOENG_LIMITER_LOOKAHEAD-pos;if(take>remain)take=(unsigned)remain;
        for(unsigned j=0;j<take;j++,pos++){
            float il=pos<frames?input[2*pos]:0.0f,ir=pos<frames?input[2*pos+1]:0.0f,ol,orr;
            sp11_audioeng_limiter_process_frame(&l,il,ir,&ol,&orr);
            if(fwrite(&ol,4,1,fo)!=1||fwrite(&orr,4,1,fo)!=1)return 9;
        }
    }
    fclose(fo);free(input);return l.catastrophic_guard?10:0;
}
