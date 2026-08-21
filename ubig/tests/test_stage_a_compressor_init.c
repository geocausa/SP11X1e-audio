#include "stage_a_compressor.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){unsigned char cfg[0xc0]={0},store[0x910];int32_t d[20];uint32_t sr=48000,b=20;memcpy(cfg,&sr,4);memcpy(cfg+4,&b,4);for(int i=0;i<20;i++)d[i]=(i*i*37+11*i+3)%4000;memset(store,0xa5,sizeof store);void*p=ubig_stage_a_compressor_init(cfg,d,store+3);ptrdiff_t o=(unsigned char*)p-store;memset((unsigned char*)p+0x10,0,8);uint64_t h=1469598103934665603ULL;h=h64(h,&o,sizeof o);h=h64(h,store,sizeof store);if(o!=8||h!=0x3fea31461291d74fULL){fprintf(stderr,"compressor init off=%td hash=%016llx\n",o,(unsigned long long)h);return 2;}puts("PASS Stage A compressor cold-constructor regression");return 0;}
