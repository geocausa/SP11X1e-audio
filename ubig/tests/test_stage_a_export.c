#include "stage_a_export.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
int main(void){
    float prev[20],src[40],out[20];int32_t raw[20];
    for(int b=0;b<20;b++){prev[b]=(b-10)*0.03125f;src[b]=(b-8)*0.0234375f;src[20+b]=(9-b)*0.01953125f;}
    const uint32_t exp_bits[20]={0x3db3498f,0x3d8b498f,0x3d46931e,0x3ced263c,0x3c1a4c78,0xbc25b388,0xbcf2d9c4,0xbd496ce2,0xbd20db0e,0xbc812413,0x3c597906,0x3d2d4e8c,0x3d921f6b,0x3dcd9791,0x3e0487db,0x3e2243ee,0x3e400000,0x3e5fb5bf,0x3e7f6b7e,0x3e8f909e};
    const int32_t exp_raw[20]={182,141,100,60,19,-22,-62,-103,-82,-33,27,88,148,208,269,329,390,454,518,583};
    ubig_stage_a_export(prev,src,2,out,raw);
    for(int b=0;b<20;b++)if(bits(out[b])!=exp_bits[b]||raw[b]!=exp_raw[b]){fprintf(stderr,"export regression b=%d state=%08x/%08x raw=%d/%d\n",b,bits(out[b]),exp_bits[b],raw[b],exp_raw[b]);return 2;}
    puts("PASS Stage A export proven-regression vector");return 0;
}
