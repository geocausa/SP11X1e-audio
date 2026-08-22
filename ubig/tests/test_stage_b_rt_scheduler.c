#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x8c6a8e17u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float scale){return ((int32_t)(ru()>>8))*(scale/8388608.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    /* First exercise the cadence gate independently over a long pseudo-random
       sequence. Its private DLL differential covers the same six-word state. */
    UbigStageBRtSchedulerClock clock={0,7,3,5,1,0};
    uint64_t h=1469598103934665603ULL;
    for(unsigned i=0;i<100000u;i++){
        if((i%997u)==0u){
            clock.upper_period=1u+ru()%17u;
            clock.upper_count=ru()%clock.upper_period;
            clock.lower_period=1u+ru()%17u;
            clock.lower_count=ru()%20u;
            clock.lower_reset=ru()%4u;
            clock.lower_toggle=ru()&1u;
        }
        const uint32_t actions=ubig_stage_b_rt_scheduler_step(&clock);
        h=h64(h,&actions,sizeof actions);h=h64(h,&clock,sizeof clock);
    }

    uint32_t bounds[9]={0,9,18,27,36,46,56,66,77};
    float variation_weights[8]={0.75f,0.875f,1.0f,1.125f,0.9375f,1.0625f,0.8125f,1.1875f};
    UbigStageBRtVariationConfig variation={8u,bounds,variation_weights};
    UbigStageBRtSegmentRatioConfig ratio={bounds};
    UbigStageBRtFeatureHistoryConfig feature={bounds,32u};

    float projection_weights[19][4];
    UbigStageBRtProjectionConfig projection;
    for(unsigned band=0;band<19u;band++){
        projection.bands[band].start=(band*3u)%72u;
        projection.bands[band].count=4u;
        projection.bands[band].weights=projection_weights[band];
        for(unsigned lane=0;lane<4u;lane++)projection_weights[band][lane]=0.25f+0.03125f*(float)((band+lane)%9u);
    }
    float lut[76];for(unsigned i=0;i<76u;i++)lut[i]=fr(0.5f);projection.projection_lut=lut;
    UbigStageBRtUniversalConfig config={&feature,&variation,&ratio,&projection,5u,5u};

    UbigStageBRtUniversalAnalysis state;memset(&state,0,sizeof state);
    UbigStageBRtUniversalOutput output;memset(&output,0,sizeof output);
    state.clock.upper_period=4u;
    state.clock.lower_period=5u;
    state.clock.lower_count=5u; /* initial lower-A seed before first upper tick */
    state.segment_ratio_cursor.step=5u;
    state.variation_cursor.step=5u;
    state.spectral_change_cursor.step=5u;
    state.feature_change_cursor.step=5u;
    state.peak_residual_cursor.step=5u;

    UbigStageBRtSpectralExport input;input.count=77u;
    for(unsigned call=0;call<12000u;call++){
        input.exponent=(int32_t)(ru()%13u)-6;
        input.aggregate=0.25f+(float)(ru()%4096u)*(1.0f/2048.0f);
        for(unsigned bin=0;bin<input.count;bin++){
            float v=0.05f+(float)(ru()%8192u)*(1.0f/4096.0f);
            if((ru()%43u)==0u)v=0.0f;
            input.bins[bin]=v;
        }
        ubig_stage_b_rt_universal_analysis_process(&state,&config,&input,&output);
        h=h64(h,&state,sizeof state);h=h64(h,&output,sizeof output);
    }
    if(h!=0x6b917f1f081076f3ULL){fprintf(stderr,"Stage-B universal-scheduler hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT universal-scheduler regression");return 0;
}
