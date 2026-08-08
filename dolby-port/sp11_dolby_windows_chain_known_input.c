#define _GNU_SOURCE
#include <dlfcn.h>
#include <ladspa.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef const LADSPA_Descriptor *(*DescFn)(unsigned long);

static int wav_open_pcm16_stereo(const char *path, FILE **fp, uint32_t *rate, uint32_t *frames){
 FILE*f=fopen(path,"rb"); if(!f)return -1; char id[4]; uint32_t sz;
 if(fread(id,1,4,f)!=4||memcmp(id,"RIFF",4)){fclose(f);return -2;} fread(&sz,4,1,f);
 if(fread(id,1,4,f)!=4||memcmp(id,"WAVE",4)){fclose(f);return -3;}
 uint16_t fmt=0,ch=0,bits=0; uint32_t sr=0,data_bytes=0; long data_pos=0;
 while(fread(id,1,4,f)==4 && fread(&sz,4,1,f)==1){ long next=ftell(f)+sz+(sz&1);
  if(!memcmp(id,"fmt ",4)){fread(&fmt,2,1,f);fread(&ch,2,1,f);fread(&sr,4,1,f);fseek(f,6,SEEK_CUR);fread(&bits,2,1,f);}
  else if(!memcmp(id,"data",4)){data_bytes=sz;data_pos=ftell(f);break;} fseek(f,next,SEEK_SET);
 }
 if(fmt!=1||ch!=2||bits!=16||!data_pos){fprintf(stderr,"unsupported wav fmt=%u ch=%u bits=%u\n",fmt,ch,bits);fclose(f);return -4;}
 *rate=sr;*frames=data_bytes/4;fseek(f,data_pos,SEEK_SET);*fp=f;return 0;
}

int main(int ac,char**av){
 const char *so=ac>1?av[1]:"./dolby-port/sp11_dolby_windows_chain.so";
 const char *inpath=ac>2?av[2]:"input.wav";
 const char *outpath=ac>3?av[3]:"/tmp/sp11_windows_chain_known.f32";
 unsigned chunk=ac>4?(unsigned)strtoul(av[4],0,0):480; if(!chunk)chunk=480;
 FILE *f=NULL;uint32_t rate=0,frames=0;if(wav_open_pcm16_stereo(inpath,&f,&rate,&frames)||rate!=48000){fprintf(stderr,"input open/rate failed\n");return 2;}
 void *lib=dlopen(so,RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 3;}
 DescFn df=(DescFn)dlsym(lib,"ladspa_descriptor");const LADSPA_Descriptor*d=df?df(0):0;if(!d)return 4;
 LADSPA_Handle h=d->instantiate(d,rate);if(!h){fprintf(stderr,"instantiate failed\n");return 5;}float bypass=0;d->connect_port(h,4,&bypass);if(d->activate)d->activate(h);
 FILE *o=fopen(outpath,"wb");if(!o){perror("output");return 6;}
 int16_t *pcm=malloc((size_t)chunk*2*sizeof(int16_t));float *il=malloc((size_t)chunk*sizeof(float)),*ir=malloc((size_t)chunk*sizeof(float)),*ol=malloc((size_t)chunk*sizeof(float)),*or=malloc((size_t)chunk*sizeof(float)),*inter=malloc((size_t)chunk*2*sizeof(float));if(!pcm||!il||!ir||!ol||!or||!inter)return 7;
 uint32_t done=0;double ss=0;float peak=0;size_t bad=0;
 while(done<frames){uint32_t n=frames-done;if(n>chunk)n=chunk;size_t got=fread(pcm,sizeof(int16_t)*2,n,f);if(got!=n){fprintf(stderr,"short read %zu/%u\n",got,n);return 8;}
  for(uint32_t i=0;i<n;i++){il[i]=pcm[2*i]/32768.0f;ir[i]=pcm[2*i+1]/32768.0f;ol[i]=or[i]=0;}
  d->connect_port(h,0,il);d->connect_port(h,1,ir);d->connect_port(h,2,ol);d->connect_port(h,3,or);d->run(h,n);
  for(uint32_t i=0;i<n;i++){float l=ol[i],r=or[i];if(!isfinite(l)||!isfinite(r))bad++;inter[2*i]=l;inter[2*i+1]=r;float a=fabsf(l)>fabsf(r)?fabsf(l):fabsf(r);if(a>peak)peak=a;ss+=(double)l*l+(double)r*r;}
  fwrite(inter,sizeof(float)*2,n,o);done+=n;
 }
 fprintf(stderr,"processed frames=%u seconds=%.6f chunk=%u bad=%zu rms=%.9g peak=%.9g peak_db=%.4f\n",done,done/(double)rate,chunk,bad,sqrt(ss/(2.0*done)),peak,peak>0?20*log10(peak):-999.0);
 fclose(o);fclose(f);if(d->cleanup)d->cleanup(h);dlclose(lib);free(pcm);free(il);free(ir);free(ol);free(or);free(inter);return bad?9:0;
}
