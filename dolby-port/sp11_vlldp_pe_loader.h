#ifndef SP11_VLLDP_PE_LOADER_H
#define SP11_VLLDP_PE_LOADER_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#define SP11_PE_IMAGE_BASE_EXPECTED 0x180000000ULL
#define SP11_PE_RELOC_DIR64 10
typedef struct { uint8_t *base; size_t size; uint64_t image_base; int64_t delta; } Sp11PeImage;
static inline uint16_t sp11_rd16(const uint8_t *p){return (uint16_t)(p[0]|(p[1]<<8));}
static inline uint32_t sp11_rd32(const uint8_t *p){return (uint32_t)(p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24));}
static inline uint64_t sp11_rd64(const uint8_t *p){return (uint64_t)sp11_rd32(p)|((uint64_t)sp11_rd32(p+4)<<32);}
static int sp11_pe_load(Sp11PeImage *img, const char *dll_path){
    memset(img,0,sizeof(*img));
    FILE *f=fopen(dll_path,"rb");
    if(!f){fprintf(stderr,"[pe] cannot open %s\n",dll_path);return -1;}
    fseek(f,0,SEEK_END); long fsize=ftell(f); fseek(f,0,SEEK_SET);
    if(fsize<=0x200){fclose(f);return -2;}
    uint8_t *raw=(uint8_t*)malloc((size_t)fsize);
    if(!raw){fclose(f);return -3;}
    if(fread(raw,1,(size_t)fsize,f)!=(size_t)fsize){free(raw);fclose(f);return -4;}
    fclose(f);
    if(sp11_rd16(raw)!=0x5A4D){free(raw);return -5;}
    uint32_t e_lfanew=sp11_rd32(raw+0x3C);
    const uint8_t *nt=raw+e_lfanew;
    if(sp11_rd32(nt)!=0x00004550){free(raw);return -6;}
    const uint8_t *coff=nt+4;
    uint16_t num_sections=sp11_rd16(coff+2);
    uint16_t opt_size=sp11_rd16(coff+16);
    const uint8_t *opt=coff+20;
    uint16_t magic=sp11_rd16(opt);
    if(magic!=0x20B){free(raw);return -7;}
    uint32_t size_of_image=sp11_rd32(opt+56);
    uint32_t size_of_headers=sp11_rd32(opt+60);
    uint64_t image_base=sp11_rd64(opt+24);
    uint32_t reloc_rva=sp11_rd32(opt+112+5*8);
    uint32_t reloc_size=sp11_rd32(opt+112+5*8+4);
    void *mapped=mmap(NULL,size_of_image,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(mapped==MAP_FAILED){free(raw);return -8;}
    memset(mapped,0,size_of_image);
    uint8_t *base=(uint8_t*)mapped;
    memcpy(base,raw,size_of_headers);
    const uint8_t *sec=opt+opt_size;
    for(uint16_t i=0;i<num_sections;i++){
        const uint8_t *sh=sec+(size_t)i*40;
        uint32_t va=sp11_rd32(sh+12);
        uint32_t raw_size=sp11_rd32(sh+16);
        uint32_t raw_ptr=sp11_rd32(sh+20);
        if(raw_size==0)continue;
        if((long)(raw_ptr+raw_size)>fsize)raw_size=(uint32_t)(fsize-raw_ptr);
        if(va+raw_size>size_of_image)continue;
        memcpy(base+va,raw+raw_ptr,raw_size);
    }
    free(raw);
    int64_t delta=(int64_t)((uint64_t)(uintptr_t)base-image_base);
    if(delta!=0&&reloc_rva!=0&&reloc_size!=0){
        uint32_t off=0;
        while(off+8<=reloc_size){
            const uint8_t *blk=base+reloc_rva+off;
            uint32_t page_rva=sp11_rd32(blk);
            uint32_t block_size=sp11_rd32(blk+4);
            if(block_size<8||off+block_size>reloc_size)break;
            uint32_t n=(block_size-8)/2;
            const uint8_t *ent=blk+8;
            for(uint32_t k=0;k<n;k++){
                uint16_t e=sp11_rd16(ent+(size_t)k*2);
                uint16_t type=(uint16_t)(e>>12);
                uint16_t voff=(uint16_t)(e&0x0FFF);
                if(type==0)continue;
                if(type!=SP11_PE_RELOC_DIR64){fprintf(stderr,"[pe] unsupported reloc type %u\n",type);munmap(mapped,size_of_image);return -9;}
                uint8_t *target=base+page_rva+voff;
                uint64_t cur=sp11_rd64(target);
                cur=(uint64_t)((int64_t)cur+delta);
                target[0]=(uint8_t)(cur);target[1]=(uint8_t)(cur>>8);target[2]=(uint8_t)(cur>>16);target[3]=(uint8_t)(cur>>24);
                target[4]=(uint8_t)(cur>>32);target[5]=(uint8_t)(cur>>40);target[6]=(uint8_t)(cur>>48);target[7]=(uint8_t)(cur>>56);
            }
            off+=block_size;
        }
    }
    img->base=base; img->size=size_of_image; img->image_base=image_base; img->delta=delta;
    return 0;
}
static inline void *sp11_pe_ptr_for_va(const Sp11PeImage *img,uint64_t va){return (void*)(img->base+(va-img->image_base));}
static inline uint64_t sp11_pe_read_u64_va(const Sp11PeImage *img,uint64_t va){return sp11_rd64((const uint8_t*)sp11_pe_ptr_for_va(img,va));}
static void sp11_pe_unload(Sp11PeImage *img){if(img->base){munmap(img->base,img->size);img->base=NULL;}}
typedef uint64_t (*Sp11ResolverFn)(uint32_t key);
static int sp11_pe_check_resolvers(const Sp11PeImage *img){
    struct{uint64_t resolver_va;uint32_t key;uint64_t expected_va;const char *name;}checks[]={
        {0x1800B98F8ULL,0xA0,0x1800D6450ULL,"FFT fallback key 0xa0"},
        {0x1800B98F8ULL,0x140,0x1800D7E20ULL,"FFT fallback key 0x140"},
        {0x1800B9C58ULL,0x140,0x1800D7D70ULL,"IFFT fallback key 0x140"}};
    int ok=1;
    for(int i=0;i<3;i++){
        Sp11ResolverFn fn=(Sp11ResolverFn)sp11_pe_ptr_for_va(img,checks[i].resolver_va);
        uint64_t got_runtime=fn(checks[i].key);
        uint64_t got_va=got_runtime-(uint64_t)img->delta;
        if(got_va!=checks[i].expected_va){fprintf(stderr,"[pe] resolver FAILED: %s got 0x%llx expected 0x%llx\n",checks[i].name,(unsigned long long)got_va,(unsigned long long)checks[i].expected_va);ok=0;}
        else{fprintf(stderr,"[pe] resolver ok: %s -> 0x%llx\n",checks[i].name,(unsigned long long)got_va);}
    }
    return ok?0:-1;
}
#endif
