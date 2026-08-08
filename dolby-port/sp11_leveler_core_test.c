/* sp11_leveler_core_test.c
 * Proves the C native bridge reproduces FUN_180021e80 b60 (target MAE ~3).
 * Reads leveler_inputs.bin (dumped by dump_leveler_inputs.py), maps the DLL
 * with sp11_vlldp_pe_loader.h, calls the native leveler, compares to target. */
#include "sp11_vlldp_pe_loader.h"
#include <math.h>

#define PROCESS_VA 0x180021E80ULL
#define N 20

/* descriptor: {uint32 count; pad; uint64 ptr_to_ptr_array} = 16 bytes */
typedef struct { uint32_t count; uint32_t pad; uint64_t ptrs; } Desc;

typedef void (*ProcFn)(void*,void*,void*,void*,void*,void*,void*,
                       uint32_t,float,float,float,void*,void*,void*,void*);

static uint32_t rdu32(FILE*f){uint32_t v;fread(&v,4,1,f);return v;}
static uint64_t rdu64(FILE*f){uint64_t v;fread(&v,8,1,f);return v;}
static float    rdf32(FILE*f){float v;fread(&v,4,1,f);return v;}

int main(int argc,char**argv){
    const char*dll=(argc>1)?argv[1]:
      "/usr/lib/sp11-dolby/DolbyAPOvlldp150.dll";
    const char*binp=(argc>2)?argv[2]:"/tmp/sp11build/leveler_inputs.bin";

    FILE*f=fopen(binp,"rb");
    if(!f){fprintf(stderr,"cannot open %s\n",binp);return 1;}
    uint32_t state_size=rdu32(f);
    uint32_t ch=rdu32(f);
    uint32_t native_count=rdu32(f);
    uint64_t wbase=rdu64(f);
    float s9=rdf32(f),s10=rdf32(f),s11=rdf32(f);

    uint8_t*state=malloc(state_size);
    fread(state,1,state_size,f);
    float side_a[N],side_b[N],runtime[5],p12[N];
    int32_t mask[N];
    for(int i=0;i<N;i++)side_a[i]=rdf32(f);
    for(int i=0;i<N;i++)side_b[i]=rdf32(f);
    for(int i=0;i<N;i++){int32_t v;fread(&v,4,1,f);mask[i]=v;}
    for(int i=0;i<5;i++)runtime[i]=rdf32(f);
    for(int i=0;i<N;i++)p12[i]=rdf32(f);
    float in_rows[8][N];
    for(uint32_t c=0;c<ch;c++)for(int b=0;b<N;b++)in_rows[c][b]=rdf32(f);
    int32_t target_b60[N];
    for(int i=0;i<N;i++){int32_t v;fread(&v,4,1,f);target_b60[i]=v;}
    fclose(f);

    /* pad input/output rows to native_count */
    uint32_t nrows = native_count>ch?native_count:ch;
    float out_rows[8][N];
    for(uint32_t c=0;c<nrows;c++)for(int b=0;b<N;b++){
        if(c>=ch)in_rows[c][b]=in_rows[ch-1][b];
        out_rows[c][b]=0.0f;
    }

    /* map DLL */
    Sp11PeImage img;
    if(sp11_pe_load(&img,dll)!=0){fprintf(stderr,"pe load failed\n");return 2;}

    /* relocate embedded windows pointers inside captured state */
    int patched=0;
    for(uint32_t off=0;off+8<=state_size;off+=8){
        uint64_t v=sp11_rd64(state+off);
        if(v>=wbase && v<wbase+img.size){
            uint64_t reloc=(uint64_t)(uintptr_t)sp11_pe_ptr_for_va(&img,img.image_base+(v-wbase));
            state[off+0]=(uint8_t)reloc;     state[off+1]=(uint8_t)(reloc>>8);
            state[off+2]=(uint8_t)(reloc>>16);state[off+3]=(uint8_t)(reloc>>24);
            state[off+4]=(uint8_t)(reloc>>32);state[off+5]=(uint8_t)(reloc>>40);
            state[off+6]=(uint8_t)(reloc>>48);state[off+7]=(uint8_t)(reloc>>56);
            patched++;
        }
    }
    fprintf(stderr,"relocated %d embedded pointers\n",patched);

    /* build descriptors */
    uint64_t in_ptrs[8],out_ptrs[8];
    for(uint32_t c=0;c<nrows;c++){in_ptrs[c]=(uint64_t)(uintptr_t)in_rows[c];out_ptrs[c]=(uint64_t)(uintptr_t)out_rows[c];}
    Desc in_desc={nrows,0,(uint64_t)(uintptr_t)in_ptrs};
    Desc out_desc={nrows,0,(uint64_t)(uintptr_t)out_ptrs};

    int32_t b60[N]={0}, side_scratch[8*N]={0}, export_count[1]={0};

    ProcFn proc=(ProcFn)sp11_pe_ptr_for_va(&img,PROCESS_VA);
    proc(state, side_a, side_b, mask, runtime,
         &in_desc, &out_desc, native_count,
         s9, s10, s11, p12, b60, side_scratch, export_count);

    /* compare */
    long sum=0; int maxabs=0;
    printf("predicted_b60=[");
    for(int i=0;i<N;i++){printf("%d%s",b60[i],i<N-1?", ":"");}
    printf("]\n");
    printf("target_b60   =[");
    for(int i=0;i<N;i++){printf("%d%s",target_b60[i],i<N-1?", ":"");}
    printf("]\n");
    printf("diff=[");
    for(int i=0;i<N;i++){int d=b60[i]-target_b60[i];printf("%d%s",d,i<N-1?", ":"");if(d<0)d=-d;sum+=d;if(d>maxabs)maxabs=d;}
    printf("]\n");
    double mae=(double)sum/N;
    printf("mae=%.2f max_abs=%d\n",mae,maxabs);
    printf("RESULT: %s\n", mae<=10.0?"PASS (C native bridge reproduces b60)":"FAIL");

    sp11_pe_unload(&img);
    free(state);
    return mae<=10.0?0:3;
}
