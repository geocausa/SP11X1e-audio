#include "ubig/ubig.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    ubig_engine_config cfg={UBIG_ABI_VERSION,UBIG_SAMPLE_RATE,UBIG_CHANNELS,UBIG_PROFILE_MOVIE};
    ubig_engine *e=ubig_engine_create(&cfg);if(!e)return 2;
    if(ubig_engine_profile(e)!=UBIG_PROFILE_MOVIE)return 3;
    if(ubig_engine_set_profile(e,UBIG_PROFILE_MUSIC))return 4;
    if(ubig_engine_profile(e)!=UBIG_PROFILE_MUSIC)return 5;
    int32_t eq[UBIG_EQ_BANDS]={0};eq[0]=-192;eq[19]=192;
    if(ubig_engine_set_custom_eq(e,eq))return 6;
    eq[4]=193;if(ubig_engine_set_custom_eq(e,eq)!=UBIG_EINVAL)return 7;
    ubig_engine_destroy(e);
    puts("PASS engine ABI/profile lifecycle");return 0;
}
