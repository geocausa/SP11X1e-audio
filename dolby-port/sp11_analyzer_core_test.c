/* sp11_analyzer_core_test.c — prove FUN_180023db0 (analyzer) runs in C and
 * reproduces the expected 20-band output. Reads analyzer_inputs.bin. */
#include "sp11_vlldp_pe_loader.h"
#include <math.h>

#define ANALYZER_VA      0x180023DB0ULL
#define RUNTIME_CONFIG_VA 0x180116C40ULL
#define N 20

typedef void (*AnaFn)(float, void*, void*, void*, void*, void*);

static uint32_t rdu32(FILE*f){uint32_t v;if(fread(&v,4,1,f)!=1)v=0;return v;}
static uint64_t rdu64(FILE*f){uint64_t v;if(fread(&v,8,1,f)!=1)v=0;return v;}

int main(int argc,char**argv){
    const char*dll=(argc>1)?argv[1]:
      "/run/media/ubi/Local Disk/Users/GEOCA/Documents/Research_Hub/Audio/SOURCE/Dolby/SpeakerDLLs/DolbyAPOvlldp150.dll";
    const char*binp=(argc>2)?argv[2]:"/tmp/sp11build/analyzer_inputs.bin";

    FILE*f=fopen(binp,"rb");
    if(!f){fprintf(stderr,"open %s failed\n",binp);return 1;}
    uint64_t wbase=rdu64(f);
    uint64_t table_ptr=rdu64(f);
    uint32_t nwin=rdu32(f),nband=rdu32(f),nover=rdu32(f),ngain=rdu32(f),nblk=rdu32(f);
    uint8_t state[0x40];
    if(fread(state,1,0x40,f)!=0x40){fprintf(stderr,"state read failed\n");return 1;}
    float *window=malloc(nwin*4),*band=malloc(nband*4),*overlap=malloc(nover*4),*gain=malloc(ngain*4);
    if(fread(window,4,nwin,f)!=nwin)return 1;
    if(fread(band,4,nband,f)!=nband)return 1;
    if(fread(overlap,4,nover,f)!=nover)return 1;
    if(fread(gain,4,ngain,f)!=ngain)return 1;
    float (*blocks)[256]=malloc((size_t)nblk*256*4);
    for(uint32_t b=0;b<nblk;b++) if(fread(blocks[b],4,256,f)!=256)return 1;
    float expected[N];
    if(fread(expected,4,N,f)!=N)return 1;
    fclose(f);

    Sp11PeImage img;
    if(sp11_pe_load(&img,dll)!=0){fprintf(stderr,"pe load failed\n");return 2;}

    /* build the 0x40 analyzer state struct with relocated/local pointers:
       0x00 -> window, 0x08 -> relocated table callback, 0x18 -> band,
       0x20 -> overlap, 0x28 -> gain */
    #define SETP(off,p) do{uint64_t _v=(uint64_t)(uintptr_t)(p);memcpy(state+(off),&_v,8);}while(0)
    /* relocate the captured 0x08 callback pointer into the fresh image */
    uint64_t cb = table_ptr;
    if(wbase && table_ptr>=wbase && table_ptr<wbase+img.size)
        cb = (uint64_t)(uintptr_t)sp11_pe_ptr_for_va(&img, img.image_base+(table_ptr-wbase));
    SETP(0x00, window);
    memcpy(state+0x08,&cb,8);
    SETP(0x18, band);
    SETP(0x20, overlap);
    SETP(0x28, gain);

    void* descriptor=(void*)(uintptr_t)sp11_pe_read_u64_va(&img, RUNTIME_CONFIG_VA);

    float output[N]={0};
    uint8_t *scratch=calloc(1,32768);
    AnaFn ana=(AnaFn)sp11_pe_ptr_for_va(&img, ANALYZER_VA);

    for(uint32_t b=0;b<nblk;b++){
        ana(1.1111111640930176f, state, descriptor, blocks[b], output, scratch);
    }

    long sum=0; double maxabs=0;
    printf("output_bands  =[");for(int i=0;i<N;i++)printf("%.6f%s",output[i],i<N-1?", ":"");printf("]\n");
    printf("expected_bands=[");for(int i=0;i<N;i++)printf("%.6f%s",expected[i],i<N-1?", ":"");printf("]\n");
    double sd=0; for(int i=0;i<N;i++){double d=fabs(output[i]-expected[i]);sd+=d;if(d>maxabs)maxabs=d;}
    (void)sum;
    printf("mae=%.3e max_abs=%.3e\n", sd/N, maxabs);
    printf("RESULT: %s\n", (sd/N)<1e-3?"PASS (C analyzer reproduces bands)":"FAIL");

    free(window);free(band);free(overlap);free(gain);free(blocks);free(scratch);
    sp11_pe_unload(&img);
    return (sd/N)<1e-3?0:3;
}
