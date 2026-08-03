/* sp11_init_state_test.c — prove FUN_180021DA8 inits optimizer state from
 * the DLL's own 48k tables, with NO capture file. This is the live-init path. */
#include "sp11_vlldp_pe_loader.h"

#define STATE_INIT_VA   0x180021DA8ULL
#define TABLE_48K_256   0x180116C40ULL
#define STATE_SIZE      0x900

typedef void* (*InitFn)(void*, void*, void*);

int main(int argc,char**argv){
    const char*dll=(argc>1)?argv[1]:
      "/run/media/ubi/Local Disk/Users/GEOCA/Documents/Research_Hub/Audio/SOURCE/Dolby/SpeakerDLLs/DolbyAPOvlldp150.dll";
    Sp11PeImage img;
    if(sp11_pe_load(&img,dll)!=0){fprintf(stderr,"pe load failed\n");return 1;}

    /* read 8 table pointers from TABLE_48K_256 (already relocated by loader) */
    uint64_t tbl[8];
    for(int i=0;i<8;i++) tbl[i]=sp11_pe_read_u64_va(&img, TABLE_48K_256 + (uint64_t)i*8);

    printf("table_ptrs (post-reloc):\n");
    for(int i=0;i<8;i++) printf("  [%d] = 0x%llx\n", i, (unsigned long long)tbl[i]);

    void* leveler_config = (void*)(uintptr_t)tbl[2];
    void* distribution   = (void*)(uintptr_t)tbl[6];

    /* sanity: both should land inside the mapped image */
    uint64_t lo=(uint64_t)(uintptr_t)img.base, hi=lo+img.size;
    printf("leveler_config in image: %s\n",
        ((uint64_t)(uintptr_t)leveler_config>=lo && (uint64_t)(uintptr_t)leveler_config<hi)?"yes":"NO");
    printf("distribution  in image: %s\n",
        ((uint64_t)(uintptr_t)distribution>=lo && (uint64_t)(uintptr_t)distribution<hi)?"yes":"NO");

    uint8_t* state=calloc(1,STATE_SIZE);
    InitFn init=(InitFn)sp11_pe_ptr_for_va(&img, STATE_INIT_VA);
    void* ret=init(leveler_config, distribution, state);

    printf("init returned %p, state addr %p, match=%s\n",
        ret,(void*)state, (ret==(void*)state)?"YES":"NO");

    /* show a few non-zero fields the init wrote */
    int nonzero=0;
    for(int off=0;off<STATE_SIZE;off+=8){
        uint64_t v=sp11_rd64(state+off);
        if(v!=0) nonzero++;
    }
    printf("state non-zero qwords after init: %d / %d\n", nonzero, STATE_SIZE/8);

    /* dump first 0x40 bytes as a fingerprint */
    printf("state[0x00..0x40]:\n  ");
    for(int i=0;i<0x40;i++){printf("%02x ",state[i]); if((i&15)==15)printf("\n  ");}
    printf("\n");

    printf("RESULT: %s\n", (ret==(void*)state && nonzero>0)?"PASS (live state init works in C)":"FAIL");
    free(state); sp11_pe_unload(&img);
    return (ret==(void*)state && nonzero>0)?0:2;
}
