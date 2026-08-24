#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x7b2f0c31u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}

int main(void){
    UbigStageBRtUniversalOutput probe={0};float packed[UBIG_STAGE_B_RT_UNIVERSAL_FEATURES];
    for(unsigned i=0;i<UBIG_STAGE_B_RT_FEATURE_CADENCE_OUTPUTS;i++)probe.feature_cadence[i]=(float)i;
    for(unsigned i=0;i<8u;i++){probe.variation_mean[i]=1000.0f+(float)i;probe.variation_deviation[i]=1100.0f+(float)i;probe.segment_ratio_mean[i]=1200.0f+(float)i;probe.segment_ratio_deviation[i]=1300.0f+(float)i;}
    for(unsigned i=0;i<10u;i++)probe.peak_rank[i]=1400.0f+(float)i;
    for(unsigned i=0;i<30u;i++)probe.projection_cadence[i]=1500.0f+(float)i;
    probe.feature_change[0]=1600.0f;probe.feature_change[1]=1601.0f;probe.spectral_change[0]=1700.0f;probe.spectral_change[1]=1701.0f;
    ubig_stage_b_rt_universal_pack_features(&probe,packed);
    if(packed[0]!=0.0f||packed[185]!=185.0f||packed[186]!=1000.0f||packed[194]!=1100.0f||packed[202]!=1200.0f||packed[210]!=1300.0f||packed[218]!=1400.0f||packed[228]!=1500.0f||packed[258]!=1600.0f||packed[260]!=1700.0f)return 2;
    UbigStageBRtUniversalOutput roundtrip={0};ubig_stage_b_rt_universal_unpack_features(&roundtrip,packed);if(memcmp(&probe,&roundtrip,sizeof probe))return 3;

    uint32_t bounds[9]={0,9,18,27,36,46,56,66,77};
    float variation_weights[8]={0.75f,0.875f,1.0f,1.125f,0.9375f,1.0625f,0.8125f,1.1875f};
    UbigStageBRtVariationConfig variation={8u,bounds,variation_weights};
    UbigStageBRtSegmentRatioConfig ratio={bounds};
    UbigStageBRtFeatureHistoryConfig feature={bounds,32u};
    float projection_weights[19][4];UbigStageBRtProjectionConfig projection;
    for(unsigned band=0;band<19u;band++){projection.bands[band].start=(band*3u)%72u;projection.bands[band].count=4u;projection.bands[band].weights=projection_weights[band];for(unsigned lane=0;lane<4u;lane++)projection_weights[band][lane]=0.25f+0.03125f*(float)((band+lane)%9u);}
    float lut[76];for(unsigned i=0;i<76u;i++)lut[i]=fr(-0.5f,0.5f);projection.projection_lut=lut;
    UbigStageBRtUniversalConfig analysis_cfg={&feature,&variation,&ratio,&projection,5u,5u};

    UbigStageBRtControlTerm control_terms[5][18];UbigStageBRtControlDescriptor desc[5];UbigStageBRtControlCadenceConfig control_cfg;
    const uint32_t slots[4]={1u,2u,6u,5u};
    for(unsigned g=0;g<5u;g++){
        desc[g].term_count=18u;desc[g].transfer_gain=-0.52f-0.01f*(float)g;desc[g].transfer_bias=0.07f-0.02f*(float)g;desc[g].terms=control_terms[g];
        for(unsigned i=0;i<18u;i++){control_terms[g][i].feature_index=(uint16_t)(2u+(17u*i+29u*g)%260u);if(g==4u&&(i%6u)==0u)control_terms[g][i].feature_index=(uint16_t)(292u+(i/6u)%4u);control_terms[g][i].exponent=(uint16_t)(4u+(i+g)%11u);control_terms[g][i].scale=0.4f+0.03f*(float)((i+2u*g)%7u);control_terms[g][i].weight=-0.35f+0.05f*(float)((i+g)%9u);control_terms[g][i].center=-0.15f+0.02f*(float)((3u*i+g)%13u);}
    }
    for(unsigned g=0;g<4u;g++){control_cfg.groups[g].output_index=slots[g];control_cfg.groups[g].descriptor=&desc[g];}control_cfg.secondary=&desc[4];
    UbigStageBRtAnalysisControllerConfig cfg={&analysis_cfg,&control_cfg};

    UbigStageBRtAnalysisController state;memset(&state,0,sizeof state);
    state.spectral.period=16u;state.spectral.exponent_offset=0;state.spectral.output_scale=1.0f;
    state.analysis.clock.upper_period=16u;state.analysis.clock.lower_count=27u;state.analysis.clock.lower_period=32u;state.analysis.clock.lower_reset=27u;
    state.analysis.segment_ratio_cursor.step=5u;state.analysis.variation_cursor.step=5u;state.analysis.spectral_change_cursor.step=5u;state.analysis.feature_change_cursor.step=5u;state.analysis.peak_residual_cursor.step=5u;
    state.control.period=16u;state.control.cycle=27u;state.control.target=32u;state.control.reset=27u;state.control.armed=1u;

    float row0[154],row1[154];uint64_t h=1469598103934665603ULL;
    for(unsigned call=0;call<5000u;call++){
        for(unsigned i=0;i<154u;i++){row0[i]=fr(-0.75f,0.75f);row1[i]=fr(-0.75f,0.75f);}
        ubig_stage_b_rt_analysis_controller_process(&state,&cfg,row0,row1);
        h=h64(h,&state.spectral_export,sizeof state.spectral_export);h=h64(h,&state.analysis_output,sizeof state.analysis_output);h=h64(h,&state.control,sizeof state.control);
    }
    if(h!=0x2ff63042c8ab1dd4ULL){fprintf(stderr,"Stage-B analysis-controller hash %016llx\n",(unsigned long long)h);return 4;}
    puts("PASS Stage-B RT analysis-controller regression");return 0;
}
