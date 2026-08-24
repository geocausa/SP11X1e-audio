#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng=0x54a48a55u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float rf(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void *p,size_t n){const unsigned char *b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
#define H(x) h=h64(h,&(x),sizeof(x))
#define HA(x) h=h64(h,(x),sizeof(x))

int main(void){
    enum {R=2,B=20,CD=3,WD=3,ID=4,CM=3};
    float rows_data[R][B],work_data[R][B],*rows_ptr[R]={rows_data[0],rows_data[1]},*work_ptr[R]={work_data[0],work_data[1]};
    int32_t telemetry[B]={0};
    UbigStageBRtBandRows rows={R,B,rows_ptr,B},work={R,B,work_ptr,B};

    UbigStageBRtCurveRecord fall[CM],rise[CM];
    float tail_weights[B],chain[5],gate_ref[B],gate_slope[B];
    for(unsigned i=0;i<CM;i++){
        fall[i]=(UbigStageBRtCurveRecord){rf(-.15f,.05f),rf(.05f,.25f),rf(.72f,.98f)};
        rise[i]=(UbigStageBRtCurveRecord){rf(-.1f,.08f),rf(.04f,.22f),rf(.75f,1.0f)};
    }
    for(unsigned i=0;i<B;i++){tail_weights[i]=rf(-.25f,.25f);gate_ref[i]=rf(-.55f,-.05f);gate_slope[i]=rf(.25f,1.1f);}
    for(unsigned i=0;i<5;i++)chain[i]=rf(.04f,.18f);
    UbigStageBRtMultibandTuning tuning={fall,rise,tail_weights,chain,gate_ref,gate_slope};

    UbigStageBRtMultibandState s;memset(&s,0,sizeof s);
    float corr_p[R][CD*B],corr_s[R][CD*B],corr_i[R][ID];uint32_t corr_status[R][CD];
    float win_a[R][WD*B],win_b[R][WD*B],stereo_hist[B];
    for(unsigned r=0;r<R;r++){
        for(unsigned i=0;i<CD*B;i++){corr_p[r][i]=rf(-.6f,.6f);corr_s[r][i]=rf(-.6f,.6f);}
        for(unsigned i=0;i<CD;i++)corr_status[r][i]=ru()%8u;
        for(unsigned i=0;i<ID;i++)corr_i[r][i]=rf(-.1f,.1f);
        s.correlation[r].primary=(UbigStageBRtRowHistory){CD,ru()%CD,corr_p[r]};
        s.correlation[r].secondary_depth=CD;s.correlation[r].secondary_index=ru()%CD;s.correlation[r].secondary_buffer=corr_s[r];s.correlation[r].secondary_status=corr_status[r];
        s.correlation[r].integrator_depth=ID;s.correlation[r].integrator_index=ru()%ID;s.correlation[r].correlation_scale=rf(.03f,.18f);s.correlation[r].accumulator_a=rf(-.08f,.08f);s.correlation[r].accumulator_b=rf(-.08f,.08f);s.correlation[r].integrator_ring=corr_i[r];s.correlation[r].output_state=rf(-.1f,.1f);
        for(unsigned i=0;i<WD*B;i++){win_a[r][i]=rf(-.4f,.4f);win_b[r][i]=rf(-.4f,.4f);}
        s.window[r].input_window=(UbigStageBRtWindowSum){WD,ru()%WD,win_a[r],rf(.04f,.16f),{0},{0}};
        s.window[r].rms_window=(UbigStageBRtWindowSum){WD,ru()%WD,win_b[r],rf(.04f,.16f),{0},{0}};
        s.window[r].rms_scale=rf(.1f,.8f);s.window[r].blend_bias=rf(-.05f,.12f);s.window[r].blend_scale=rf(.08f,.45f);
    }
    s.optional_mix=(UbigStageBRtMixSmootherConfig){.12f,.38f};
    s.blend_alpha=.85f;s.tail=(UbigStageBRtTailState){0.0f,-.65f};
    s.gate_decay_step=.0015f;s.gate_correction_step=.002f;s.gate_keep=.92f;s.gate_inject=.11f;
    s.stereo_alpha=.88f;
    s.stereo.counter_scale=.65f;s.stereo.output_scale=.3f;s.stereo.input_state0=-.04f;s.stereo.input_state1=-.03f;s.stereo.gate_state=.15f;s.stereo.input_mix=.8f;s.stereo.adaptive_mix=.82f;s.stereo.history=stereo_hist;
    s.crossfade=(UbigStageBRtCrossfadeState){.28f,.45f};
    for(unsigned r=0;r<R;r++)for(unsigned i=0;i<B;i++){s.curve_rows[r][i]=rf(-.55f,.05f);s.post_rows[r][i]=rf(-.45f,.05f);s.blend_rows[r][i]=rf(-.5f,.05f);s.gate_rows[r].value[i]=rf(0,.08f);s.gate_rows[r].counter[i]=ru()%100u;}
    for(unsigned i=0;i<B;i++){s.stereo_row[i]=rf(-.2f,.04f);s.stereo.adaptive[i]=rf(-.18f,0);s.stereo.smoothed[i]=rf(-.5f,.05f);s.stereo.counter[i]=(int32_t)(ru()%65u)-32;stereo_hist[i]=rf(-.1f,.1f);}

    uint64_t h=1469598103934665603ULL;
    for(unsigned n=0;n<6000;n++){
        for(unsigned r=0;r<R;r++)for(unsigned i=0;i<B;i++){rows_data[r][i]=rf(-.62f,.08f);work_data[r][i]=rf(-.35f,.12f);}
        float optional=rf(0.0f,1.0f);const float *op=(n%3u)?&optional:NULL;
        s.curve_mode=n%CM;
        const float enable=(n%5u)?0.0f:rf(-.04f,.04f);
        const uint32_t mode=(n%7u);
        ubig_stage_b_rt_multiband_process(enable,rf(-.08f,.08f),&s,mode,op,&rows,&work,telemetry,&tuning);

        HA(s.curve_rows);H(s.curve_mode);H(s.optional_mix);HA(s.post_rows);HA(s.blend_rows);H(s.blend_alpha);H(s.tail);
        H(s.gate_decay_step);H(s.gate_correction_step);H(s.gate_keep);H(s.gate_inject);HA(s.gate_rows);
        H(s.stereo_alpha);HA(s.stereo_row);HA(s.stereo.counter);H(s.stereo.counter_scale);H(s.stereo.output_scale);HA(s.stereo.adaptive);HA(s.stereo.smoothed);H(s.stereo.input_state0);H(s.stereo.input_state1);H(s.stereo.gate_state);H(s.stereo.input_mix);H(s.stereo.adaptive_mix);H(s.crossfade);H(s.active_mode);H(s.enable_value);
        for(unsigned r=0;r<R;r++){
            H(s.correlation[r].primary.depth);H(s.correlation[r].primary.index);H(s.correlation[r].secondary_depth);H(s.correlation[r].secondary_index);H(s.correlation[r].integrator_depth);H(s.correlation[r].integrator_index);H(s.correlation[r].correlation_scale);H(s.correlation[r].accumulator_a);H(s.correlation[r].accumulator_b);H(s.correlation[r].output_state);
            h=h64(h,corr_p[r],sizeof corr_p[r]);h=h64(h,corr_s[r],sizeof corr_s[r]);h=h64(h,corr_status[r],sizeof corr_status[r]);h=h64(h,corr_i[r],sizeof corr_i[r]);
            H(s.window[r].input_window.depth);H(s.window[r].input_window.index);H(s.window[r].input_window.scale);HA(s.window[r].input_window.accumulator);HA(s.window[r].input_window.window_sum);H(s.window[r].rms_window.depth);H(s.window[r].rms_window.index);H(s.window[r].rms_window.scale);HA(s.window[r].rms_window.accumulator);HA(s.window[r].rms_window.window_sum);H(s.window[r].rms_scale);H(s.window[r].blend_bias);H(s.window[r].blend_scale);h=h64(h,win_a[r],sizeof win_a[r]);h=h64(h,win_b[r],sizeof win_b[r]);
        }
        h=h64(h,stereo_hist,sizeof stereo_hist);h=h64(h,rows_data,sizeof rows_data);h=h64(h,work_data,sizeof work_data);h=h64(h,telemetry,sizeof telemetry);
    }
    if(h!=0xf0c5c8963e2cc6b4ULL){fprintf(stderr,"Stage-B RT multiband parent hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B RT deployed stereo multiband parent regression");
    return 0;
}
