#include "sp11_vlldp_pe_loader.h"
int main(int argc,char **argv){
    const char *dll=(argc>1)?argv[1]:"/run/media/ubi/Local Disk/Users/GEOCA/Documents/Research_Hub/Audio/SOURCE/Dolby/SpeakerDLLs/DolbyAPOvlldp150.dll";
    Sp11PeImage img;
    int rc=sp11_pe_load(&img,dll);
    if(rc!=0){fprintf(stderr,"load failed rc=%d\n",rc);return 1;}
    printf("loaded ok\n");
    printf("  mapped base = %p\n",(void*)img.base);
    printf("  size        = 0x%zx (%zu bytes)\n",img.size,img.size);
    printf("  image_base  = 0x%llx\n",(unsigned long long)img.image_base);
    printf("  delta       = 0x%llx\n",(unsigned long long)(uint64_t)img.delta);
    if(img.image_base!=SP11_PE_IMAGE_BASE_EXPECTED)printf("  WARNING: image_base != 0x180000000\n");
    uint64_t analyzer_va=0x180023DB0ULL;
    uint32_t first_insn=sp11_rd32((const uint8_t*)sp11_pe_ptr_for_va(&img,analyzer_va));
    printf("  FUN_180023db0 first insn = 0x%08x\n",first_insn);
    if(first_insn==0){printf("  ERROR: analyzer entry is zero\n");sp11_pe_unload(&img);return 2;}
    printf("  FUN_180021e80 first insn = 0x%08x\n",sp11_rd32((const uint8_t*)sp11_pe_ptr_for_va(&img,0x180021E80ULL)));
    printf("  FUN_1800240e0 first insn = 0x%08x\n",sp11_rd32((const uint8_t*)sp11_pe_ptr_for_va(&img,0x1800240E0ULL)));
    printf("  FUN_18001de90 first insn = 0x%08x\n",sp11_rd32((const uint8_t*)sp11_pe_ptr_for_va(&img,0x18001DE90ULL)));
    printf("running resolver checks...\n");
    int chk=sp11_pe_check_resolvers(&img);
    printf("resolver checks %s\n",chk==0?"PASSED":"FAILED");
    sp11_pe_unload(&img);
    return chk==0?0:3;
}
