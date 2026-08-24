#include "stage_a_limiter.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
int main(void){
    ubig_stage_a_limiter s;ubig_stage_a_limiter_init(&s);float l[256],r[256];uint64_t h=1469598103934665603ULL;
    for(unsigned b=0;b<80;b++){
        for(unsigned i=0;i<256;i++){
            double t=(b*256u+i)/48000.0;float env=(b<8?0.18f:(b<24?0.82f:(b<40?1.25f:(b<56?0.04f:0.62f))));
            l[i]=env*(0.84f*sinf((float)(6.283185307179586476925286766559*75.0*t))+0.16f*sinf((float)(6.283185307179586476925286766559*997.0*t)));
            r[i]=env*(0.78f*sinf((float)(6.283185307179586476925286766559*75.0*t+0.21))+0.13f*sinf((float)(6.283185307179586476925286766559*1553.0*t+0.37)));
            if(((b*256u+i)%4093u)==0){l[i]+=0.71f;r[i]-=0.49f;}
        }
        ubig_stage_a_limiter_process_256(&s,b<32?0.7079442739486694f:0.9998999834060669f,l,r);
        h=fnv(h,l,sizeof(l));h=fnv(h,r,sizeof(r));
    }
    h=fnv(h,&s.delay_pos,sizeof(s.delay_pos));h=fnv(h,&s.history_pos,sizeof(s.history_pos));
    h=fnv(h,&s.envelope_primary,sizeof(s.envelope_primary));h=fnv(h,&s.envelope_secondary,sizeof(s.envelope_secondary));
    h=fnv(h,&s.current_gain,sizeof(s.current_gain));h=fnv(h,&s.previous_gain,sizeof(s.previous_gain));h=fnv(h,&s.target_gain,sizeof(s.target_gain));
    h=fnv(h,s.delay,sizeof(s.delay));h=fnv(h,s.peak_history,sizeof(s.peak_history));h=fnv(h,s.predictor_history,sizeof(s.predictor_history));
    if(h!=0xdfdbc87c960f4715ULL || s.delay_pos!=0 || s.history_pos!=0 ||
       bits(s.envelope_primary)!=0x3f46bb28u || bits(s.envelope_secondary)!=0x3f032553u ||
       bits(s.current_gain)!=0x3f800000u || bits(s.previous_gain)!=0x3f800000u || bits(s.target_gain)!=0x3f800000u){
        fprintf(stderr,"Stage A limiter regression hash/state mismatch: %016llx\n",(unsigned long long)h);return 2;
    }
    puts("PASS Stage A limiter proven-regression vector");
    return 0;
}
