#include "stage_a_math.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
int main(void){
    struct {float x;uint32_t out;} l[]={
        {0.0001f,0xc1548defu},{0.001f,0xc11f7db7u},{0.01f,0xc0d4e3bdu},
        {0.1f,0xc0547ae2u},{0.25f,0xc0000000u},{0.5f,0xbf800001u},
        {0.9998999834060669f,0xb90bf555u},{1.0f,0xb4000000u},
        {2.0f,0x3f7ffffeu},{4.0f,0x3fffffffu},{16.0f,0x40800000u}
    };
    for(unsigned i=0;i<sizeof(l)/sizeof(l[0]);i++){
        uint32_t got=bits(ubig_stage_a_log2_approx(l[i].x));
        if(got!=l[i].out){fprintf(stderr,"log regression %u got=%08x exp=%08x\n",i,got,l[i].out);return 2;}
    }
    const float in[20]={-4.75f,-4.0f,-3.5f,-2.75f,-2.0f,-1.25f,-0.5f,-0.125f,0,0.125f,0.5f,0.75f,1,1.25f,1.75f,2,2.5f,3,3.75f,4.5f};
    float out[20];ubig_stage_a_exp2_scaled(out,in,20,0.04631230608f);
    const uint32_t exp[20]={
      0x3f5bf150u,0x3f614cd7u,0x3f64f0adu,0x3f6a8105u,0x3f7031b5u,
      0x3f760338u,0x3f7bf60bu,0x3f7efc18u,0x3f800000u,0x3f808277u,
      0x3f820d3cu,0x3f831742u,0x3f842393u,0x3f853233u,0x3f875673u,
      0x3f886c1du,0x3f8a9e9cu,0x3f8cdac6u,0x3f90477au,0x3f93cac1u};
    for(unsigned i=0;i<20;i++)if(bits(out[i])!=exp[i]){fprintf(stderr,"exp regression %u got=%08x exp=%08x\n",i,bits(out[i]),exp[i]);return 3;}
    puts("PASS Stage A math proven-regression vectors");return 0;
}
