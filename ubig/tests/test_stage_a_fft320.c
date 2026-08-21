#include "stage_a_fft320.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    float *in=calloc(640,sizeof(float)),*out=calloc(640,sizeof(float)),*norm=calloc(640,sizeof(float));
    if(!in||!out||!norm)return 2;
    in[0]=1.0f;
    ubig_stage_a_fft320(out,in,320,NULL);
    for(int k=0;k<320;k++)if(out[2*k]!=1.0f||out[2*k+1]!=0.0f){fprintf(stderr,"fft impulse k=%d %.9g %.9g\n",k,out[2*k],out[2*k+1]);return 3;}
    ubig_stage_a_fft320_norm320(norm,in,320,NULL);
    const float q=0x1.99999ap-9f;
    for(int k=0;k<320;k++)if(norm[2*k]!=q||norm[2*k+1]!=0.0f){fprintf(stderr,"fft norm impulse k=%d %.9g %.9g\n",k,norm[2*k],norm[2*k+1]);return 4;}
    memset(in,0,640*sizeof(float));in[2]=1.0f;
    ubig_stage_a_fft320(out,in,320,NULL);
    if(out[2]!=0.999807f||out[3]!=-0.0196339991f){fprintf(stderr,"fft root1 %.9g %.9g\n",out[2],out[3]);return 5;}

    uint32_t r=0x243f6a88u;
    for(unsigned i=0;i<640;i++){r=r*1664525u+1013904223u;int32_t v=(int32_t)(r>>8)-(1<<23);in[i]=(float)v/(float)(1<<21);}
    ubig_stage_a_fft320(out,in,320,NULL);
    ubig_stage_a_fft320_norm320(norm,in,320,NULL);
    uint64_t h=1469598103934665603ULL;h=h64(h,out,640*sizeof(float));h=h64(h,norm,640*sizeof(float));
    if(h!=0xd040429d49cb7dadULL){fprintf(stderr,"fft proven hash %016llx != d040429d49cb7dad\n",(unsigned long long)h);return 6;}
    puts("PASS Stage A FFT320 exact mixed-radix regression");return 0;
}
