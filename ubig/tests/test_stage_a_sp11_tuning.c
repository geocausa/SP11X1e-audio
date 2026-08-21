#include "stage_a_sp11_tuning.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){
    UbigStageACoreConfig c;ubig_stage_a_sp11_dynamic_config(&c);
    if(!c.compressor_config||!c.compressor_distribution||!c.base_rows[0]||!c.base_rows[1]||!c.side_a||!c.side_b||!c.mask||!c.runtime5||!c.channel_mix)return 2;
    const UbigStageAProfileFamilyState *common=ubig_stage_a_sp11_profile_family_state(UBIG_STAGE_A_PROFILE_FAMILY_COMMON);
    const UbigStageAProfileFamilyState *mm=ubig_stage_a_sp11_profile_family_state(UBIG_STAGE_A_PROFILE_FAMILY_MOVIE_MUSIC);
    if(!common||!mm||common->group_count!=1u||common->channel_deviation!=0||common->slow_gain_enable!=0u||common->slow_gain_mix!=256)return 3;
    if(mm->group_count!=4u||mm->channel_deviation!=96||mm->slow_gain_enable!=1u||mm->slow_gain_mix!=103)return 4;
    static const int32_t common_groups[6]={20,0,32767,10,20,0};
    static const int32_t mm_groups[24]={2,-256,12980,3,20,64,7,-160,16366,10,20,64,16,0,32767,10,20,0,20,0,32767,10,20,0};
    if(memcmp(common->groups,common_groups,sizeof common_groups)||memcmp(mm->groups,mm_groups,sizeof mm_groups))return 5;
    unsigned char cfg[0x98];memcpy(cfg,c.compressor_config,sizeof cfg);uint64_t severity;memcpy(&severity,cfg+0x88,8);memset(cfg+0x88,0,8);
    uint64_t h=1469598103934665603ULL;
    h=h64(h,&c.input_scale,4);h=h64(h,&c.limiter_ceiling,4);h=h64(h,&c.analysis_enable,4);h=h64(h,&c.compressor_enable,4);h=h64(h,&c.compressor_mode,4);h=h64(h,&c.native_count,4);
    h=h64(h,cfg,sizeof cfg);h=h64(h,(const void*)(uintptr_t)severity,20u*4u);h=h64(h,c.compressor_distribution,20u*4u);h=h64(h,c.base_rows[0],40u*4u);h=h64(h,c.side_a,20u*4u);h=h64(h,c.side_b,20u*4u);h=h64(h,c.mask,20u*4u);h=h64(h,c.runtime5,5u*4u);h=h64(h,&c.drive_state,4);h=h64(h,&c.controller_drive,4);h=h64(h,c.channel_mix,8u*4u);
    h=h64(h,&common->group_count,4);h=h64(h,common->groups,common->group_count*6u*4u);h=h64(h,&common->channel_deviation,4);h=h64(h,&common->slow_gain_enable,4);h=h64(h,&common->slow_gain_mix,4);
    h=h64(h,&mm->group_count,4);h=h64(h,mm->groups,mm->group_count*6u*4u);h=h64(h,&mm->channel_deviation,4);h=h64(h,&mm->slow_gain_enable,4);h=h64(h,&mm->slow_gain_mix,4);
    if(h!=0xab5ecd9bfff80604ULL){fprintf(stderr,"SP11 Stage-A tuning/family hash %016llx\n",(unsigned long long)h);return 6;}
    puts("PASS SP11 Stage-A tuning/profile-family regression");return 0;
}
