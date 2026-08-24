#include "stage_a_core.h"
#include <stdint.h>
#include <stdio.h>

static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
static uint32_t rng=0x6a09e667u;
static float rf(void){rng=rng*1664525u+1013904223u;int32_t v=(int32_t)(rng>>8)-(1<<23);return(float)v/(float)(1<<21);}

int main(void){
    float base[2][20];
    for(unsigned ch=0;ch<2;ch++)for(unsigned i=0;i<20;i++)base[ch][i]=-0.03f+(float)(ch*3u+i)*0.00075f;
    UbigStageACoreConfig c={0};
    c.input_scale=1.1111111640930176f;
    c.limiter_ceiling=0.9998999834060669f;
    c.analysis_enable=1u;
    c.base_rows[0]=base[0];c.base_rows[1]=base[1];
    UbigStageACoreState s;
    if(ubig_stage_a_core_init(&s,&c))return 2;
    uint64_t h=1469598103934665603ULL;
    for(unsigned blk=0;blk<6u;blk++){
        float li[256],ri[256],lo[256],ro[256];
        for(unsigned i=0;i<256u;i++){li[i]=rf()*0.125f;ri[i]=rf()*0.125f;}
        if(ubig_stage_a_core_process_256(&s,&c,li,ri,lo,ro))return 3;
        h=h64(h,lo,sizeof lo);h=h64(h,ro,sizeof ro);
    }
    for(unsigned ch=0;ch<2u;ch++){
        h=h64(h,&s.channel[ch].phase_index,sizeof s.channel[ch].phase_index);
        h=h64(h,s.channel[ch].history,sizeof s.channel[ch].history);
        h=h64(h,s.channel[ch].band_data,sizeof s.channel[ch].band_data);
        h=h64(h,s.channel[ch].overlap,sizeof s.channel[ch].overlap);
        h=h64(h,s.channel[ch].gains,sizeof s.channel[ch].gains);
    }
    h=h64(h,&s.limiter_feedback,sizeof s.limiter_feedback);
    h=h64(h,&s.startup_blend_index,sizeof s.startup_blend_index);
    h=h64(h,&s.startup_blend_active,sizeof s.startup_blend_active);
    h=h64(h,s.export_state,sizeof s.export_state);h=h64(h,s.export_raw,sizeof s.export_raw);
    h=h64(h,s.analyzed,sizeof s.analyzed);h=h64(h,s.main_rows,sizeof s.main_rows);h=h64(h,s.companion_rows,sizeof s.companion_rows);
    if(h!=0x5675539e0cba96e6ULL){fprintf(stderr,"Stage A core six-block hash %016llx\\n",(unsigned long long)h);return 4;}
    if(s.channel[0].phase_index!=1u||s.channel[1].phase_index!=1u||s.startup_blend_index!=0u||s.startup_blend_active!=0u)return 5;
    puts("PASS Stage A cold six-block lifecycle regression");return 0;
}
