/* sp11_synth_core_test.c — prove FUN_1800240e0 (synthesis) runs in C and
 * reproduces the expected output samples. Reads synth_inputs.bin. */
#include "sp11_vlldp_pe_loader.h"
#include <math.h>

#define SYNTHESIS_VA   0x1800240E0ULL
#define DESCRIPTOR_VA  0x180116A20ULL
#define FFT_SPAN 4096

typedef uint64_t (*SynFn)(void*,void*,void*,void*,void*);
static uint32_t rdu32(FILE*f){uint32_t v;if(fread(&v,4,1,f)!=1)v=0;return v;}

int main(int argc,char**argv){
    const char*dll=(argc>1)?argv[1]:
      "/run/media/ubi/Local Disk/Users/GEOCA/Documents/Research_Hub/Audio/SOURCE/Dolby/SpeakerDLLs/DolbyAPOvlldp150.dll";
    const char*binp=(argc>2)?argv[2]:"/tmp/sp11build/synth_inputs.bin";
    FILE*f=fopen(binp,"rb"); if(!f){fprintf(stderr,"open fail\n");return 1;}
    uint32_t phase_count=rdu32(f),matrix_span=rdu32(f),output_span=rdu32(f),band_count=rdu32(f);
    uint32_t nbd=rdu32(f),ngn=rdu32(f);
    float*band_data=malloc((size_t)nbd*4),*gains=malloc((size_t)ngn*4),*overlap=malloc((size_t)matrix_span*4);
    if(fread(band_data,4,nbd,f)!=nbd)return 1;
    if(fread(gains,4,ngn,f)!=ngn)return 1;
    if(fread(overlap,4,matrix_span,f)!=matrix_span)return 1;
    uint32_t nexp=rdu32(f);
    float*expected=malloc((size_t)nexp*4);
    if(fread(expected,4,nexp,f)!=nexp)return 1;
    fclose(f);
    (void)phase_count;(void)band_count;

    Sp11PeImage img;
    if(sp11_pe_load(&img,dll)!=0){fprintf(stderr,"pe load failed\n");return 2;}

    float*output=malloc((size_t)output_span*4);
    for(uint32_t i=0;i<output_span;i++)output[i]=-12345.0f;
    float*scratch=malloc((size_t)FFT_SPAN*4);
    for(int i=0;i<FFT_SPAN;i++)scratch[i]=7777.0f;

    uint8_t state[0x40]; memset(state,0,0x40);
    uint64_t p;
    p=(uint64_t)(uintptr_t)band_data; memcpy(state+0x18,&p,8);
    p=(uint64_t)(uintptr_t)overlap;   memcpy(state+0x20,&p,8);
    p=(uint64_t)(uintptr_t)gains;     memcpy(state+0x28,&p,8);

    void*descriptor=sp11_pe_ptr_for_va(&img,DESCRIPTOR_VA);
    SynFn syn=(SynFn)sp11_pe_ptr_for_va(&img,SYNTHESIS_VA);
    uint64_t result=syn(state, descriptor, NULL, output, scratch);

    double sd=0,maxabs=0;
    for(uint32_t i=0;i<output_span;i++){double d=fabs(output[i]-expected[i]);sd+=d;if(d>maxabs)maxabs=d;}
    printf("synth result=0x%llx\n",(unsigned long long)result);
    printf("output[0:8]  =[");for(int i=0;i<8;i++)printf("%.4f%s",output[i],i<7?", ":"");printf("]\n");
    printf("expected[0:8]=[");for(int i=0;i<8;i++)printf("%.4f%s",expected[i],i<7?", ":"");printf("]\n");
    printf("mae=%.3e max_abs=%.3e over %u samples\n",sd/output_span,maxabs,output_span);
    printf("RESULT: %s\n",(sd/output_span)<1e-3?"PASS (C synthesis reproduces output)":"FAIL");

    free(band_data);free(gains);free(overlap);free(output);free(scratch);free(expected);
    sp11_pe_unload(&img);
    return (sd/output_span)<1e-3?0:3;
}
