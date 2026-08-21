#include "stage_a_fft320.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
    float *in=calloc(640,sizeof(float)),*out=calloc(640,sizeof(float));
    if(!in||!out)return 2;
    in[0]=1.0f;
    ubig_stage_a_fft320(out,in,320,NULL);
    for(int k=0;k<320;k++)if(out[2*k]!=1.0f||out[2*k+1]!=0.0f){fprintf(stderr,"fft impulse k=%d %.9g %.9g\n",k,out[2*k],out[2*k+1]);return 3;}
    ubig_stage_a_fft320_norm320(out,in,320,NULL);
    const float q=0x1.99999ap-9f;
    for(int k=0;k<320;k++)if(out[2*k]!=q||out[2*k+1]!=0.0f){fprintf(stderr,"fft norm impulse k=%d %.9g %.9g\n",k,out[2*k],out[2*k+1]);return 4;}
    memset(in,0,640*sizeof(float));in[2]=1.0f;
    ubig_stage_a_fft320(out,in,320,NULL);
    if(fabsf(out[2]-0.99980724f)>2e-7f||fabsf(out[3]+0.019633692f)>2e-7f)return 5;
    puts("PASS Stage A FFT320 mathematical contract");return 0;
}
