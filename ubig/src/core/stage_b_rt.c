#include "stage_b_rt.h"
#include "stage_a_math.h"
#include <math.h>
#include <string.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif

static float f32_bits(uint32_t u){float f;memcpy(&f,&u,4);return f;}


float ubig_stage_b_rt_hysteresis_process(UbigStageBRtHysteresisState *s)
{
    if(!s)return 0.0f;
    float smooth=s->input*f32_bits(0x3b23d700u);
    smooth=fmaf(s->smoothed_input,f32_bits(0x3f7f5c29u),smooth);
    s->smoothed_input=smooth;

    if(s->countdown>0){
        if(s->toggle){
            if(f32_bits(0x3f0ccccdu)<smooth)s->countdown=-1;
            else s->countdown--;
        }else{
            if(smooth<f32_bits(0x3ee66666u))s->countdown=-1;
            else s->countdown--;
        }
    }else if(s->countdown==0){
        s->toggle=(s->toggle==0u);
        s->countdown=-1;
    }else{
        const int trigger=s->toggle?(smooth<0.25f):(f32_bits(0x3f19999au)<smooth);
        if(trigger){
            const float delta=0.5f-smooth;
            const float magnitude=(-delta>delta)?-delta:delta;
            float value=fmaf(magnitude,s->countdown_scale,s->countdown_bias);
            value*=f32_bits(0x47000000u);
            value=floorf(value);
            int32_t next=(int32_t)value;
            if(next>32767)next=32767;
            next>>=3;
            s->countdown=next;
        }
    }

    const float target=s->toggle?1.0f:0.0f;
    const float add=target*(1.0f-s->toggle_keep);
    s->toggle_state=fmaf(s->toggle_state,s->toggle_keep,add);

    const float one_minus=1.0f-s->response_a;
    float result=fmaf(-one_minus,s->response_b,1.0f);
    result*=1.0f-s->toggle_state;
    float upper=s->response_c;
    if(upper<s->response_b)upper=s->response_b;
    return fmaf(1.0f-upper,s->toggle_state,result);
}

float ubig_stage_b_rt_complex_energy(float *const *rows,
                                     uint32_t row_count,
                                     uint32_t begin,
                                     uint32_t end)
{
    double real_sum=0.0,imag_sum=0.0;
    for(uint32_t r=0;r<row_count;r++){
        const float *row=rows[r];
        for(uint32_t k=begin;k<end;k++){
            const double re=(double)row[2u*k];
            const double im=(double)row[2u*k+1u];
            real_sum=fma(re,re,real_sum);
            imag_sum=fma(im,im,imag_sum);
        }
    }
    return (float)(real_sum+imag_sum);
}

void ubig_stage_b_rt_band_log_process(float offset0,
                                      float offset1,
                                      const UbigStageBRtComplexGroups *main_groups,
                                      const UbigStageBRtExtraGroups *extra_groups,
                                      const uint32_t *band_ends,
                                      const int32_t *group_to_output,
                                      UbigStageBRtBandRows *output,
                                      UbigStageBRtTelemetryRows *telemetry)
{
    if(!output||!telemetry||!output->rows||!telemetry->rows||
       !band_ends||!group_to_output)return;
    if(output->band_count>UBIG_STAGE_B_RT_MAX_BANDS||output->capacity<output->band_count)return;
    const float log_scale=f32_bits(0x3cbdb1f9u);
    const float log_floor=f32_bits(0xbfb13b14u);
    const float upper=f32_bits(0x3e6c4ec5u);
    const float offset=offset0+offset1;
    for(uint32_t out_row=0;out_row<output->row_count;out_row++){
        float *selected[UBIG_STAGE_B_RT_MAX_SELECTED_ROWS];
        uint32_t selected_count=0;
        if(main_groups&&main_groups->groups){
            for(uint32_t g=0;g<main_groups->group_count;g++){
                if(group_to_output[g]!=(int32_t)out_row)continue;
                if(selected_count+main_groups->vectors_per_group>UBIG_STAGE_B_RT_MAX_SELECTED_ROWS)return;
                for(uint32_t v=0;v<main_groups->vectors_per_group;v++)
                    selected[selected_count++]=main_groups->groups[g][v];
                if(extra_groups&&extra_groups->groups&&g<2u){
                    if(selected_count+extra_groups->vectors_per_group>UBIG_STAGE_B_RT_MAX_SELECTED_ROWS)return;
                    for(uint32_t v=0;v<extra_groups->vectors_per_group;v++)
                        selected[selected_count++]=extra_groups->groups[g][v];
                }
            }
        }
        float *dst=output->rows[out_row];
        int32_t *tele=telemetry->rows[out_row];
        uint32_t begin=0;
        for(uint32_t band=0;band<output->band_count;band++){
            const uint32_t end=band_ends[band];
            const float energy=ubig_stage_b_rt_complex_energy(selected,selected_count,begin,end);
            float value=log_floor;
            if(energy>0.0f){
                value=ubig_stage_a_log2_approx(energy)*log_scale;
                if(value<log_floor)value=log_floor;
            }
            dst[band]=value;
            begin=end;
        }
        for(uint32_t band=0;band<output->band_count;band++){
            float value=dst[band]-offset;
            if(value<-1.0f)value=-1.0f;
            if(value>upper)value=upper;
            dst[band]=value;
            tele[band]=0;
        }
        for(uint32_t band=output->band_count;band<output->capacity;band++){
            dst[band]=-1.0f;
            tele[band]=0;
        }
    }
}

static float stage_b_rt_exp2_horner(float x)
{
    const float c0=f32_bits(0x3d714000u);
    const float c1=f32_bits(0x3e827800u);
    const float c2=f32_bits(0x3f2fb000u);
    int32_t exponent=(int32_t)x;
    if((float)exponent>x)exponent--;
    const float frac=x-(float)exponent;
    float p=fmaf(frac,c0,c1);
    p=fmaf(p,frac,c2);
    p=fmaf(p,frac,1.0f);
    uint32_t bits;
    memcpy(&bits,&p,sizeof bits);
    bits+=(uint32_t)exponent<<23;
    memcpy(&p,&bits,sizeof p);
    return p;
}

void ubig_stage_b_rt_output_shape(float row_offset,
                                  float linked_ceiling,
                                  const UbigStageBRtBandRows *input,
                                  UbigStageBRtBandRows *output,
                                  const uint32_t *object_to_row,
                                  const uint32_t *band_ends,
                                  UbigStageBRtTargetSet *targets)
{
    if(!input||!output||!targets||!input->rows||!output->rows||
       !object_to_row||!band_ends||!targets->objects)return;
    if(input->row_count!=UBIG_STAGE_B_RT_SP11_ROWS||output->row_count!=UBIG_STAGE_B_RT_SP11_ROWS||
       input->band_count!=UBIG_STAGE_B_RT_SP11_BANDS||output->band_count!=UBIG_STAGE_B_RT_SP11_BANDS||
       targets->object_count!=UBIG_STAGE_B_RT_SP11_ROWS||targets->bin_count!=UBIG_STAGE_B_RT_SP11_BINS)return;
    for(uint32_t band=0;band<UBIG_STAGE_B_RT_SP11_BANDS;band++){
        float maximum=input->rows[0][band];
        if(input->rows[1][band]>maximum)maximum=input->rows[1][band];
        if(linked_ceiling*0.5f<maximum*0.5f){
            for(uint32_t row=0;row<UBIG_STAGE_B_RT_SP11_ROWS;row++){
                float value=(linked_ceiling*0.5f-maximum*0.5f)+output->rows[row][band]*0.5f;
                value=value+value;
                if(value<-1.0f)value=-1.0f;
                if(value>1.0f)value=1.0f;
                output->rows[row][band]=value;
            }
        }
        output->rows[0][band]+=row_offset;
        output->rows[1][band]+=row_offset;
    }
    for(uint32_t row=0;row<UBIG_STAGE_B_RT_SP11_ROWS;row++){
        float band_gain[UBIG_STAGE_B_RT_SP11_BANDS];
        for(uint32_t band=0;band<UBIG_STAGE_B_RT_SP11_BANDS;band++)
            band_gain[band]=stage_b_rt_exp2_horner(output->rows[row][band]*f32_bits(0x41acbe00u));
        float gain[UBIG_STAGE_B_RT_SP11_BINS];
        uint32_t position=0;
        for(uint32_t band=0;band<UBIG_STAGE_B_RT_SP11_BANDS;band++){
            uint32_t end=band_ends[band];
            if(end>UBIG_STAGE_B_RT_SP11_BINS)end=UBIG_STAGE_B_RT_SP11_BINS;
            if(position<end){
                for(uint32_t bin=position;bin<end;bin++)gain[bin]=band_gain[band];
                position=end;
            }
        }
        if(position<UBIG_STAGE_B_RT_SP11_BINS){
            const float tail=band_gain[UBIG_STAGE_B_RT_SP11_BANDS-1u];
            for(uint32_t bin=position;bin<UBIG_STAGE_B_RT_SP11_BINS;bin++)gain[bin]=tail;
        }
        for(uint32_t object=0;object<targets->object_count;object++){
            if(object_to_row[object]!=row)continue;
            UbigStageBRtTargetObject *target=&targets->objects[object];
            for(uint32_t bin=0;bin<UBIG_STAGE_B_RT_SP11_BINS;bin++){
                const float g=gain[bin];
                for(uint32_t plane=0;plane<UBIG_STAGE_B_RT_TARGET_PLANES;plane++){
                    float *samples=target->plane[plane]+2u*bin;
                    for(uint32_t component=0;component<2u;component++){
                        float value=samples[component]*g;
                        if(value<-1.0f)value=-1.0f;
                        if(value>1.0f)value=1.0f;
                        samples[component]=value;
                    }
                }
            }
        }
    }
}

void ubig_stage_b_rt_zero_band_tail(UbigStageBRtBandRows *rows)
{
    if(!rows||!rows->rows||rows->band_count>UBIG_STAGE_B_RT_MAX_BANDS)return;
    for(uint32_t r=0;r<rows->row_count;r++)
        for(uint32_t band=rows->band_count;band<UBIG_STAGE_B_RT_MAX_BANDS;band++)
            rows->rows[r][band]=0.0f;
}

void ubig_stage_b_rt_mix_smooth(const UbigStageBRtMixSmootherConfig *config,
                                float control,
                                const float *source,
                                float *state,
                                uint32_t count)
{
    if(!config||!source||!state)return;
    const float mix=fmaf(config->control_scale,control,config->bias);
    const float keep=1.0f-mix;
    const float floor=f32_bits(0xbf313b14u);
    const uint32_t vector_prefix=count&~7u;
    for(uint32_t i=0;i<vector_prefix;i++){
        float value;
        if(state[i]>source[i]){
            const float base=source[i]*0.1f;
            value=fmaf(state[i],0.9f,base);
        }else{
            const float base=state[i]*mix;
            value=fmaf(source[i],keep,base);
        }
        if(value<floor)value=floor;
        state[i]=value;
    }
    for(uint32_t i=vector_prefix;i<count;i++){
        float value;
        if(state[i]>source[i]){
            const float base=state[i]*0.9f;
            value=fmaf(source[i],0.1f,base);
        }else{
            const float base=source[i]*keep;
            value=fmaf(state[i],mix,base);
        }
        if(value<floor)value=floor;
        state[i]=value;
    }
}

void ubig_stage_b_rt_curve_smooth(float offset,
                                  float *const *input_rows,
                                  float *state_rows,
                                  uint32_t row_count,
                                  const UbigStageBRtCurveRecord *fall,
                                  const UbigStageBRtCurveRecord *rise)
{
    if(!input_rows||!state_rows||!fall||!rise)return;
    const float floor=f32_bits(0xbf6c4ec5u);
    const float high=f32_bits(0x3d9d89d9u);
    const float capped=f32_bits(0x3d7c0fc1u);
    const float dead=f32_bits(0x3c7c0fc1u);
    for(uint32_t row=0;row<row_count;row++){
        float *state=state_rows+row*UBIG_STAGE_B_RT_MAX_BANDS;
        const float *input=input_rows[row];
        for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++){
            float target=input[lane]+offset;
            if(target<floor)target=floor;
            const float old=state[lane];
            float delta=target-old;
            const UbigStageBRtCurveRecord *record;
            if(delta<0.0f){record=fall;delta=-delta;}else record=rise;
            float x;
            if(delta>high)x=capped;
            else if(delta<dead)x=0.0f;
            else x=delta-dead;
            x*=4.0f;
            float coeff=fmaf(x,record->linear,record->constant);
            x*=4.0f;
            x*=x;
            coeff=fmaf(x,record->quadratic,coeff);
            const float prior=(coeff-1.0f)*old;
            state[lane]=fmaf(coeff,target,-prior);
        }
    }
}

void ubig_stage_b_rt_exp_rows(float *output,
                              uint32_t *row_status,
                              const float *input,
                              uint32_t active_width,
                              uint32_t row_count)
{
    if(!output||!row_status||!input||active_width>UBIG_STAGE_B_RT_MAX_BANDS)return;
    for(uint32_t row=0;row<row_count;row++){
        for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++)
            output[row*UBIG_STAGE_B_RT_MAX_BANDS+lane]=
                stage_b_rt_exp2_horner(input[row*UBIG_STAGE_B_RT_MAX_BANDS+lane]*f32_bits(0x422cbe00u));
        row_status[row]=0u;
        for(uint32_t lane=active_width;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++)
            output[row*UBIG_STAGE_B_RT_MAX_BANDS+lane]=0.0f;
    }
}

int ubig_stage_b_rt_row_history_update(UbigStageBRtRowHistory *state,
                                       float output[UBIG_STAGE_B_RT_MAX_BANDS],
                                       const float input[UBIG_STAGE_B_RT_MAX_BANDS])
{
    if(!state||!output||!input||!state->buffer||!state->depth)return -1;
    const uint32_t index=state->index;
    float *slot=state->buffer+(size_t)index*UBIG_STAGE_B_RT_MAX_BANDS;
    for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++)slot[lane]=input[lane];
    for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++)output[lane]=0.0f;
    for(uint32_t row=0;row<state->depth;row++){
        const float *src=state->buffer+(size_t)row*UBIG_STAGE_B_RT_MAX_BANDS;
        for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++)output[lane]+=src[lane];
    }
    state->index=(index+1u>=state->depth)?0u:index+1u;
    return 0;
}


static float stage_b_rt_correlation_step(UbigStageBRtCorrelationState *state,
                                         const float input[UBIG_STAGE_B_RT_MAX_BANDS])
{
    float summed[UBIG_STAGE_B_RT_MAX_BANDS];
    float previous[UBIG_STAGE_B_RT_MAX_BANDS];
    (void)ubig_stage_b_rt_row_history_update(&state->primary,summed,input);
    const uint32_t secondary_index=state->secondary_index;
    state->secondary_status[secondary_index]=0u;
    float *secondary_slot=state->secondary_buffer+(size_t)secondary_index*UBIG_STAGE_B_RT_MAX_BANDS;
    memcpy(previous,secondary_slot,sizeof previous);
    memcpy(secondary_slot,summed,sizeof summed);
    state->secondary_index=(secondary_index+1u>=state->secondary_depth)?0u:secondary_index+1u;

    float old_sq[4]={0.0f,0.0f,0.0f,0.0f};
    float new_sq[4]={0.0f,0.0f,0.0f,0.0f};
    float dot[4]={0.0f,0.0f,0.0f,0.0f};
    for(uint32_t group=0;group<5u;group++){
        for(uint32_t lane=0;lane<4u;lane++){
            const uint32_t i=group*4u+lane;
            old_sq[lane]=fmaf(previous[i],previous[i],old_sq[lane]);
            new_sq[lane]=fmaf(summed[i],summed[i],new_sq[lane]);
            dot[lane]=fmaf(previous[i],summed[i],dot[lane]);
        }
    }
    const float dot_sum=((dot[0]+dot[1])+dot[2])+dot[3];
    const float norm0=old_sq[0]+new_sq[0];
    const float norm1=old_sq[1]+new_sq[1];
    const float norm2=old_sq[2]+new_sq[2];
    const float norm3=old_sq[3]+new_sq[3];
    const float norm_sum=((norm0+norm1)+norm2)+norm3;
    float ratio=(float)((double)dot_sum/(double)norm_sum);
    ratio=ratio+ratio;
    const float error=(1.0f-ratio)*state->correlation_scale;

    const uint32_t index=state->integrator_index;
    const uint32_t next=index+1u;
    if(next<state->integrator_depth){
        const float prior=state->integrator_ring[index];
        state->accumulator_a=state->accumulator_a+error;
        state->accumulator_b=(error-prior)+state->accumulator_b;
        state->integrator_ring[index]=error;
        state->integrator_index=next;
    }else{
        const float prior=state->accumulator_a;
        state->accumulator_a=0.0f;
        state->accumulator_b=prior+error;
        state->integrator_ring[index]=error;
        state->integrator_index=0u;
    }
    float integrated=state->accumulator_b;
    if(!(integrated<0.5f))integrated=0.5f;
    integrated=(integrated-0.25f)*f32_bits(0x3d23d70au);
    state->output_state=fmaf(state->output_state,f32_bits(0x3f7d70a4u),integrated);
    return state->output_state;
}

void ubig_stage_b_rt_correlation_process(UbigStageBRtCorrelationState *states,
                                         uint32_t row_count,
                                         const float *input_rows,
                                         float *output)
{
    if(!states||!input_rows||!output)return;
    for(uint32_t row=0;row<row_count;row++)
        output[row]=stage_b_rt_correlation_step(&states[row],input_rows+(size_t)row*UBIG_STAGE_B_RT_MAX_BANDS);
}


float *ubig_stage_b_rt_window_sum_update(UbigStageBRtWindowSum *state,
                                         const float input[UBIG_STAGE_B_RT_MAX_BANDS])
{
    if(!state||!input||!state->history||!state->depth)return NULL;
    const uint32_t index=state->index;
    const uint32_t next=index+1u;
    float *slot=state->history+(size_t)index*UBIG_STAGE_B_RT_MAX_BANDS;
    if(next<state->depth){
        for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++){
            const float new_scaled=input[lane]*state->scale;
            const float old_scaled=slot[lane]*state->scale;
            state->window_sum[lane]=state->window_sum[lane]+(new_scaled-old_scaled);
            state->accumulator[lane]=state->accumulator[lane]+new_scaled;
            slot[lane]=input[lane];
        }
        state->index=next;
    }else{
        for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++){
            const float new_scaled=input[lane]*state->scale;
            state->window_sum[lane]=state->accumulator[lane]+new_scaled;
            state->accumulator[lane]=0.0f;
            slot[lane]=input[lane];
        }
        state->index=0u;
    }
    return state->window_sum;
}

void ubig_stage_b_rt_rms_deviation(float scale,
                                   float output[UBIG_STAGE_B_RT_MAX_BANDS],
                                   const float current[UBIG_STAGE_B_RT_MAX_BANDS],
                                   const float *history,
                                   uint32_t active_width,
                                   uint32_t depth)
{
    float sum_sq[UBIG_STAGE_B_RT_MAX_BANDS]={0};
    if(active_width>UBIG_STAGE_B_RT_MAX_BANDS)active_width=UBIG_STAGE_B_RT_MAX_BANDS;
#if defined(__aarch64__)
    for(uint32_t row=0;row<depth;row++){
        const float *src=history+(size_t)row*UBIG_STAGE_B_RT_MAX_BANDS;
        for(uint32_t group=0;group<5u;group++){
            float32x4_t acc=vld1q_f32(sum_sq+4u*group);
            const float32x4_t cur=vld1q_f32(current+4u*group);
            const float32x4_t old=vld1q_f32(src+4u*group);
            const float32x4_t delta=vsubq_f32(cur,old);
            acc=vfmaq_f32(acc,delta,delta);
            vst1q_f32(sum_sq+4u*group,acc);
        }
    }
    uint32_t lane=0u;
    const uint32_t vector_end=active_width&~3u;
    const float32x4_t scale4=vdupq_n_f32(scale);
    for(;lane<vector_end;lane+=4u){
        float32x4_t x=vld1q_f32(sum_sq+lane);
        float32x4_t estimate=vrsqrteq_f32(x);
        float32x4_t square=vmulq_f32(estimate,estimate);
        float32x4_t step=vrsqrtsq_f32(x,square);
        estimate=vmulq_f32(estimate,step);
        x=vld1q_f32(sum_sq+lane);
        square=vmulq_f32(estimate,estimate);
        step=vrsqrtsq_f32(x,square);
        estimate=vmulq_f32(estimate,step);
        float32x4_t reciprocal=vrecpeq_f32(estimate);
        step=vrecpsq_f32(estimate,reciprocal);
        reciprocal=vmulq_f32(reciprocal,step);
        step=vrecpsq_f32(estimate,reciprocal);
        reciprocal=vmulq_f32(reciprocal,step);
        reciprocal=vmulq_f32(reciprocal,scale4);
        vst1q_f32(output+lane,reciprocal);
    }
    for(;lane<active_width;lane++)output[lane]=sqrtf(sum_sq[lane])*scale;
#else
    for(uint32_t row=0;row<depth;row++){
        const float *src=history+(size_t)row*UBIG_STAGE_B_RT_MAX_BANDS;
        for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++){
            const float delta=current[lane]-src[lane];
            sum_sq[lane]=fmaf(delta,delta,sum_sq[lane]);
        }
    }
    for(uint32_t lane=0;lane<active_width;lane++)output[lane]=sqrtf(sum_sq[lane])*scale;
#endif
    for(uint32_t lane=active_width;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++)output[lane]=0.0f;
}


void ubig_stage_b_rt_window_blend_process(UbigStageBRtWindowBlendState *state,
                                          uint32_t active_width,
                                          const float input[UBIG_STAGE_B_RT_MAX_BANDS],
                                          float output[UBIG_STAGE_B_RT_MAX_BANDS])
{
    if(!state||!input||!output)return;
    if(active_width>UBIG_STAGE_B_RT_MAX_BANDS)active_width=UBIG_STAGE_B_RT_MAX_BANDS;
    float *window_sum=ubig_stage_b_rt_window_sum_update(&state->input_window,input);
    if(!window_sum)return;
    float deviation[UBIG_STAGE_B_RT_MAX_BANDS];
    ubig_stage_b_rt_rms_deviation(state->rms_scale,deviation,window_sum,
                                  state->input_window.history,active_width,
                                  state->input_window.depth);
    float *control=ubig_stage_b_rt_window_sum_update(&state->rms_window,deviation);
    if(!control)return;
    const float lower=f32_bits(0x3b7c0fc1u);
    const float upper=f32_bits(0x3c3d0bd1u);
    const float output_floor=f32_bits(0xbf313b14u);
    for(uint32_t lane=0;lane<active_width;lane++){
        float c=control[lane];
        if(c<lower)c=lower;
        if(c>upper)c=upper;
        float mix=c*state->blend_scale;
        mix=mix+state->blend_bias;
        mix=mix*f32_bits(0x40020000u);
        const float keep=output[lane]*mix;
        float value=fmaf(input[lane],1.0f-mix,keep);
        if(value>input[lane])value=input[lane];
        if(value<output_floor)value=output_floor;
        output[lane]=value;
    }
    for(uint32_t lane=active_width;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++)output[lane]=0.0f;
}


float ubig_stage_b_rt_tail_estimate(float previous,
                                    const float *input,
                                    uint32_t count,
                                    const float *weights)
{
    if(!input||!weights||count<2u||count>UBIG_STAGE_B_RT_MAX_BANDS)return previous;
    const uint32_t half=count>>1;
    const uint32_t start=half-1u;
    const uint32_t end=count-1u;
    float mean=0.0f;
    for(uint32_t lane=start;lane<end;lane++)
        mean=fmaf(input[lane],f32_bits(0x3dccd000u),mean);
    float weighted=0.0f;
    uint32_t wi=0u;
    for(uint32_t lane=start;lane<end;lane++,wi++){
        float term=weights[wi]*input[lane];
        term=fmaf(-weights[wi],mean,term);
        weighted=weighted+term;
    }
    return fmaf(previous,f32_bits(0x3f7d7000u),weighted*f32_bits(0x3c240000u));
}


float ubig_stage_b_rt_tail_control(UbigStageBRtTailState *state,
                                   const float *input,
                                   uint32_t count,
                                   const float *weights)
{
    if(!state||!input||!weights||count<2u||count>UBIG_STAGE_B_RT_MAX_BANDS)return 0.0f;
    const float estimate=ubig_stage_b_rt_tail_estimate(state->estimate,input,count,weights);
    const float previous_tail=state->tail_state;
    state->estimate=estimate;

    const uint32_t start=((count*2u)/3u)-1u;
    const uint32_t end=count-1u;
    float tail=0.0f;
    for(uint32_t lane=start;lane<end;lane++)
        tail=fmaf(input[lane],f32_bits(0x3e124800u),tail);
    const float tail_state=fmaf(previous_tail,f32_bits(0x3f7d7000u),
                                tail*f32_bits(0x3c240000u));
    state->tail_state=tail_state;

    float estimate_activity;
    if(estimate<f32_bits(0xbc9d89d9u))estimate_activity=1.0f;
    else if(estimate<=f32_bits(0x3c9d89d9u)){
        estimate_activity=fmaf(-estimate,f32_bits(0x3dd00000u),f32_bits(0x3b000000u));
        estimate_activity=estimate_activity*128.0f;
        estimate_activity=estimate_activity+estimate_activity;
    }else estimate_activity=0.0f;

    float tail_activity;
    if(tail_state<f32_bits(0xbf275d6cu))tail_activity=1.0f;
    else if(tail_state<=f32_bits(0xbf09d393u)){
        tail_activity=fmaf(-tail_state,f32_bits(0x3d0aaaabu),f32_bits(0xbc954fdfu));
        tail_activity=tail_activity*128.0f;
        tail_activity=tail_activity+tail_activity;
    }else tail_activity=0.0f;

    const float activity=(tail_activity>estimate_activity)?tail_activity:estimate_activity;
    return 1.0f-activity;
}

void ubig_stage_b_rt_chain_smooth(float *state,
                                  uint32_t count,
                                  uint32_t activity,
                                  const float boundary_coeff[5])
{
    if(!state||!boundary_coeff||count<9u||count>UBIG_STAGE_B_RT_MAX_BANDS)return;
    const float rate=(count==19u)?f32_bits(0x3cd79436u):f32_bits(0x3ccccccdu);
    const float step=rate*(float)activity;
    const float norm=1.0f/(step+0.5f);
    float old0=state[0],old1=state[1],old2=state[2],old3=state[3],old4=state[4];
    float x=state[0]+state[1];
    x=x+state[2]; x=x+state[3]; x=state[4]+x;
    float t=x*f32_bits(0x3dccd000u); t=fmaf(old0,step,t);
    float value=t*norm; float residual=(value-old0)+x; state[0]=value;
    x=residual+state[5]; t=x*f32_bits(0x3daab000u); t=fmaf(old1,step,t);
    value=t*norm; residual=(value-old1)+x; state[1]=value;
    x=residual+state[6]; t=x*f32_bits(0x3d925000u); t=fmaf(old2,step,t);
    value=t*norm; residual=(value-old2)+x; state[2]=value;
    x=residual+state[7]; t=x*f32_bits(0x3d800000u); t=fmaf(old3,step,t);
    value=t*norm; residual=(value-old3)+x; state[3]=value;
    x=residual+state[8]; t=x*f32_bits(0x3d638000u); t=fmaf(old4,step,t);
    value=t*norm; residual=(value-old4)+x; state[4]=value;
    if(count>9u){
        for(uint32_t i=0;i<count-9u;i++){
            const float delta=(state[i+9u]-state[i])+residual;
            const float current=state[i+5u];
            float z=boundary_coeff[4]*delta; z=fmaf(current,step,z);
            value=z*norm; residual=(value-current)+delta; state[i+5u]=value;
        }
    }
    uint32_t lane=count-4u;
    unsigned wi=3u;
    for(;lane<count;lane++,wi--){
        const float delta=residual-state[lane-5u];
        const float current=state[lane];
        float z=boundary_coeff[wi]*delta; z=fmaf(current,step,z);
        value=z*norm; residual=(value-current)+delta; state[lane]=value;
    }
}

void ubig_stage_b_rt_band_gate_process(float control,
                                       const UbigStageBRtBandGateConfig *config,
                                       UbigStageBRtBandGateRowState *row_state,
                                       uint32_t row_count,
                                       uint32_t active_width,
                                       const float *plane_a,
                                       const float *plane_b,
                                       const float *plane_c,
                                       const float *row_control,
                                       float *output,
                                       const float boundary_coeff[5])
{
    if(!config||!row_state||!plane_a||!plane_b||!plane_c||!row_control||!output||
       !boundary_coeff||!config->reference||!config->slope||
       active_width<9u||active_width>UBIG_STAGE_B_RT_MAX_BANDS)return;
    const float cap=fmaf(-control,f32_bits(0x3d9d9000u),f32_bits(0x3e1d89d9u));
    for(uint32_t row=0;row<row_count;row++){
        const float rv=row_control[row];
        float shaped=rv*rv;
        shaped=shaped*shaped;
        shaped=fmaf(shaped,rv,rv);
        shaped=shaped*0.5f;
        const float negative=(shaped>0.0f)?0.0f:shaped;
        const float positive=(rv<0.0f)?0.0f:rv;
        const float row_limit=fmaf(negative+positive,f32_bits(0x3cfc0000u),f32_bits(0x3dfc0fc1u));
        uint32_t activity=active_width;
        UbigStageBRtBandGateRowState *state=&row_state[row];
        for(uint32_t lane=0;lane<active_width;lane++){
            const size_t index=(size_t)row*UBIG_STAGE_B_RT_MAX_BANDS+lane;
            const float a=plane_a[index],b=plane_b[index],c=plane_c[index];
            float value;
            if(c<=a){
                const float p=(b-config->reference[lane])*config->slope[lane];
                const float z=fmaf(a-config->reference[lane],config->slope[lane],f32_bits(0x3d3d0bd1u));
                float candidate=(z<=p)?z:p;
                if(row_limit<candidate)candidate=row_limit;
                candidate=candidate*config->inject;
                value=fmaf(config->keep,state->value[lane],candidate);
                if(value<0.0f)value=0.0f;
                state->value[lane]=value;
                const uint32_t count=state->counter[lane];
                state->counter[lane]=(count>99u)?100u:count+1u;
            }else{
                const uint32_t count=state->counter[lane];
                int32_t age_term;
                uint32_t next_count;
                if(count<=1u){next_count=1u;age_term=-1;}
                else{next_count=count-1u;age_term=1-(int32_t)count;}
                age_term+=101;
                state->counter[lane]=next_count;
                const float decrement=(float)age_term*config->decay_step;
                const float old=state->value[lane];
                value=(decrement<old)?old-decrement:0.0f;
                state->value[lane]=value;
                activity--;
            }
            if(!((b+f32_bits(0x3d9d89d9u))>c) &&
               !(f32_bits(0xbec4ec4fu)>a) && config->correction_step<value){
                value=value-config->correction_step;
                state->value[lane]=value;
            }
            output[index]=(value<=cap)?value:cap;
        }
        ubig_stage_b_rt_chain_smooth(output+(size_t)row*UBIG_STAGE_B_RT_MAX_BANDS,
                                     active_width,activity,boundary_coeff);
    }
}

static float stage_b_rt_pow2_clamped_index(int value)
{
    if(value>60)value=60;
    if(value<-60)value=-60;
    const uint32_t bits=(uint32_t)(127-value)<<23;
    return f32_bits(bits);
}

void ubig_stage_b_rt_crossfade_process(UbigStageBRtCrossfadeState *state,
                                       const float *metric_a,
                                       const float *metric_b,
                                       int32_t control_a,
                                       int32_t control_b,
                                       uint32_t count,
                                       const float *source,
                                       float *destination)
{
    if(!state||!metric_a||!metric_b||!source||!destination)return;
    float mean_a=0.0f,mean_b=0.0f;
    for(uint32_t lane=0;lane<count;lane++){
        mean_a=fmaf(metric_a[lane],f32_bits(0x3d000000u),mean_a);
        mean_b=fmaf(metric_b[lane],f32_bits(0x3d000000u),mean_b);
    }
    int delta=(control_a<34)?34-control_a:control_a-34;
    const float power=stage_b_rt_pow2_clamped_index(delta);
    int decision;
    if(control_a<34)decision=mean_a < power*f32_bits(0x3f172d6cu);
    else decision=power*mean_a < f32_bits(0x3f172d6cu);
    if(decision){
        const int pivot=control_b+19;
        const float scaled_b=mean_b*f32_bits(0x3f2d214fu);
        delta=(control_a<pivot)?pivot-control_a:control_a-pivot;
        const float second_power=stage_b_rt_pow2_clamped_index(delta);
        if(control_a<pivot)decision=mean_a < second_power*scaled_b;
        else decision=second_power*mean_a < scaled_b;
    }
    float mix=(decision?state->polarity:-state->polarity)+state->mix;
    if(mix<0.0f)mix=0.0f;
    if(mix>1.0f)mix=1.0f;
    const float dm1=mix-1.0f;
    uint32_t lane=0u;
    const uint32_t vector_end=(count>=16u)?(count&~15u):0u;
    for(;lane<vector_end;lane++){
        const float keep=destination[lane]*mix;
        const float add=source[lane]*dm1;
        destination[lane]=keep-add;
    }
    for(;lane<count;lane++){
        const float keep=destination[lane]*mix;
        destination[lane]=fmaf(-source[lane],dm1,keep);
    }
    state->mix=mix;
}

void ubig_stage_b_rt_stereo_blend_process(float input0,
                                          float input1,
                                          UbigStageBRtStereoBlendState *state,
                                          uint32_t active_width,
                                          const float *trigger_row,
                                          const float *coefficient_row,
                                          const float *comparison_row,
                                          const float *input_row,
                                          float *destination)
{
    if(!state||!state->history||!trigger_row||!coefficient_row||!comparison_row||
       !input_row||!destination||active_width>UBIG_STAGE_B_RT_MAX_BANDS)return;

    const float one_minus_input_mix=1.0f-state->input_mix;
    const float half0=input0*0.5f;
    const float difference=fmaf(-input1,0.5f,half0);
    float gate;
    if(difference>0.0f)gate=(difference<0.5f)?difference+difference:1.0f;
    else gate=0.0f;
    state->gate_state=fmaf(state->gate_state,f32_bits(0x3f733333u),
                           gate*f32_bits(0x3d4ccccdu));
    state->input_state0=fmaf(one_minus_input_mix,input0,state->input_state0*state->input_mix);
    state->input_state1=fmaf(one_minus_input_mix,input1,state->input_state1*state->input_mix);

    float limit=(state->input_state0>=0.0f)?1.0f:state->input_state0+1.0f;
    if(state->gate_state<limit)limit=state->gate_state;
    const float one_minus_adaptive_mix=1.0f-state->adaptive_mix;
    uint32_t event_count=0u;
    for(uint32_t lane=0;lane<active_width;lane++){
        float smooth=state->smoothed[lane];
        if(trigger_row[lane]<comparison_row[lane]){
            float coefficient=coefficient_row[lane]*0.25f;
            if(coefficient<f32_bits(0xbe44ec4fu))coefficient=f32_bits(0xbe44ec4fu);
            smooth=fmaf(state->input_mix,smooth,coefficient*one_minus_input_mix);
        }
        float delta=(input_row[lane]-smooth)-state->history[lane];
        if(delta>0.0f)delta=0.0f;
        float gain=(smooth+f32_bits(0x3e313b14u))*0.8125f;
        gain=gain*64.0f;
        if(limit<gain)gain=limit;
        if(gain<0.0f)gain=0.0f;
        float term=delta*gain;
        term=term*one_minus_adaptive_mix;
        state->adaptive[lane]=fmaf(state->adaptive_mix,state->adaptive[lane],term);
        state->smoothed[lane]=smooth;
        if(comparison_row[lane]<trigger_row[lane] && f32_bits(0xbf13b13bu)<trigger_row[lane])
            event_count++;
    }

    const float deadband=state->counter_scale*f32_bits(0x39000000u);
    const uint32_t event_limit=active_width>>2;
    for(uint32_t lane=0;lane<active_width;lane++){
        const int32_t counter=state->counter[lane];
        const float normalized=(float)counter*f32_bits(0x38000000u);
        const float centred=normalized*state->counter_scale;
        const float scratch=state->adaptive[lane];
        const float difference2=fmaf(centred,4.0f,-scratch);
        float adjusted=scratch;
        int32_t next=counter;
        if(difference2 < -deadband || event_count<=event_limit){
            adjusted=fmaf(centred,4.0f,deadband);
            next=(counter<0)?counter+1:0;
        }else if(deadband<difference2){
            adjusted=fmaf(centred,4.0f,-deadband);
            next=(counter<=-32768)?-32768:counter-1;
        }
        if(adjusted>0.0f)adjusted=0.0f;
        state->counter[lane]=next;
        destination[lane]=(adjusted*state->output_scale)*4.0f;
    }
}

void ubig_stage_b_rt_multiband_process(float enable_value,
                                       float curve_offset,
                                       UbigStageBRtMultibandState *state,
                                       uint32_t mode,
                                       const float *optional_control,
                                       UbigStageBRtBandRows *rows,
                                       UbigStageBRtBandRows *work,
                                       int32_t *telemetry,
                                       const UbigStageBRtMultibandTuning *tuning)
{
    if(!state||!rows||!work||!tuning||!rows->rows||!work->rows||
       !tuning->curve_fall||!tuning->curve_rise||!tuning->tail_weights||
       !tuning->chain_coeff||!tuning->gate_reference||!tuning->gate_slope||
       rows->row_count!=2u||work->row_count!=2u||
       rows->band_count!=UBIG_STAGE_B_RT_MAX_BANDS||
       work->band_count!=UBIG_STAGE_B_RT_MAX_BANDS)return;

    ubig_stage_b_rt_zero_band_tail(rows);
    ubig_stage_b_rt_zero_band_tail(work);
    state->active_mode=mode;
    state->enable_value=enable_value;

    ubig_stage_b_rt_curve_smooth(curve_offset,rows->rows,&state->curve_rows[0][0],2u,
                                 tuning->curve_fall+state->curve_mode,
                                 tuning->curve_rise+state->curve_mode);

    float scratch[144]={0};
    uint32_t status[2]={0,0};
    ubig_stage_b_rt_exp_rows(scratch+104,status,&state->curve_rows[0][0],
                             UBIG_STAGE_B_RT_MAX_BANDS,2u);
    float row_control[2]={0.0f,0.0f};
    ubig_stage_b_rt_correlation_process(state->correlation,2u,scratch+104,row_control);

    if(optional_control){
        float control=optional_control[0]-0.5f;
        control=control+control;
        row_control[0]=control;
        for(uint32_t row=0;row<2u;row++)
            ubig_stage_b_rt_mix_smooth(&state->optional_mix,control,
                                       state->curve_rows[row],state->post_rows[row],
                                       UBIG_STAGE_B_RT_MAX_BANDS);
    }else{
        for(uint32_t row=0;row<2u;row++)
            ubig_stage_b_rt_window_blend_process(&state->window[row],
                                                 UBIG_STAGE_B_RT_MAX_BANDS,
                                                 state->curve_rows[row],
                                                 state->post_rows[row]);
    }

    const float control_scale=f32_bits(0x3d9d89d9u);
    const float control_bias=f32_bits(0x3dec4ec5u);
    const float curve_floor=f32_bits(0xbf1d89d9u);
    const float keep_alpha=state->blend_alpha;
    const float inject_alpha=1.0f-keep_alpha;
    for(uint32_t row=0;row<2u;row++){
        const float bias=fmaf(-row_control[row],control_scale,control_bias);
        for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++){
            float value=state->post_rows[row][lane]+bias;
            scratch[64u+row*UBIG_STAGE_B_RT_MAX_BANDS+lane]=value;
            const float curve=state->curve_rows[row][lane];
            if(value<curve){
                const float clamped=(curve>curve_floor)?curve:curve_floor;
                const float add=clamped*inject_alpha;
                state->blend_rows[row][lane]=fmaf(state->blend_rows[row][lane],keep_alpha,add);
            }
        }
    }

    const float gate=ubig_stage_b_rt_tail_control(&state->tail,state->post_rows[0],
                                                   UBIG_STAGE_B_RT_MAX_BANDS,
                                                   tuning->tail_weights);
    UbigStageBRtBandGateConfig gate_config={
        state->gate_decay_step,state->gate_correction_step,tuning->gate_reference,
        state->gate_keep,state->gate_inject,tuning->gate_slope
    };
    ubig_stage_b_rt_band_gate_process(gate,&gate_config,state->gate_rows,2u,
                                      UBIG_STAGE_B_RT_MAX_BANDS,
                                      &state->curve_rows[0][0],&state->blend_rows[0][0],
                                      scratch+64,row_control,scratch,tuning->chain_coeff);

    const float stereo_keep=state->stereo_alpha;
    const float stereo_inject=1.0f-stereo_keep;
    uint32_t lane=0u;
    const uint32_t vector_end=UBIG_STAGE_B_RT_MAX_BANDS&~7u;
    for(;lane<vector_end;lane++){
        float mixed=scratch[lane]*0.25f;
        mixed=fmaf(state->blend_rows[0][lane],0.25f,mixed);
        if(mixed<f32_bits(0xbe1d89d9u))mixed=f32_bits(0xbe1d89d9u);
        const float keep=state->stereo_row[lane]*stereo_keep;
        state->stereo_row[lane]=fmaf(mixed,stereo_inject,keep);
    }
    for(;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++){
        float mixed=state->blend_rows[0][lane]*0.25f;
        mixed=fmaf(scratch[lane],0.25f,mixed);
        if(mixed<f32_bits(0xbe1d89d9u))mixed=f32_bits(0xbe1d89d9u);
        const float add=mixed*stereo_inject;
        state->stereo_row[lane]=fmaf(state->stereo_row[lane],stereo_keep,add);
    }

    ubig_stage_b_rt_stereo_blend_process(row_control[0],row_control[1],&state->stereo,
                                          UBIG_STAGE_B_RT_MAX_BANDS,
                                          state->curve_rows[0],state->curve_rows[1],
                                          scratch+64,state->stereo_row,scratch+84);
    ubig_stage_b_rt_crossfade_process(&state->crossfade,scratch+104,scratch+124,
                                      (int32_t)status[0],(int32_t)status[1],
                                      UBIG_STAGE_B_RT_MAX_BANDS,scratch+84,scratch+20);

    if(state->active_mode!=0u||state->enable_value!=0.0f){
        for(uint32_t row=0;row<2u;row++){
            for(uint32_t i=0;i<UBIG_STAGE_B_RT_MAX_BANDS;i++){
                const float v=scratch[row*UBIG_STAGE_B_RT_MAX_BANDS+i];
                work->rows[row][i]=work->rows[row][i]+v;
                rows->rows[row][i]=rows->rows[row][i]+v;
            }
        }
        if(telemetry){
            for(uint32_t i=0;i<UBIG_STAGE_B_RT_MAX_BANDS;i++){
                long q=lrintf(scratch[i]*2080.0f);
                if(q<-32768L)q=-32768L;
                if(q>32767L)q=32767L;
                telemetry[i]+=(int32_t)q;
            }
        }
    }
}

static int stage_b_rt_spectral_shift(float value)
{
    uint32_t bits;
    memcpy(&bits,&value,sizeof bits);
    const int32_t exponent=((bits<<1)==0u)?-127:(int32_t)((bits>>23)&0xffu)-126;
    int32_t shift=-exponent;
    if(shift<0)shift=0;
    if(shift>60)shift=60;
    return shift;
}

static float stage_b_rt_pow2_integer(int32_t exponent)
{
    return f32_bits((uint32_t)(exponent+127)<<23);
}

void ubig_stage_b_rt_spectral_accumulate(UbigStageBRtSpectralAccumulator *s,
                                         const float *row0,
                                         const float *row1,
                                         UbigStageBRtSpectralExport *out)
{
    if(!s||!row0||!row1||!out||s->period==0u)return;
    if(s->counter==0u){
        memset(s->energy,0,sizeof s->energy);
        memset(s->shift,0x3e,sizeof s->shift);
        s->global_shift=127;
    }
    for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_SPECTRAL_BINS;lane++){
        float real=0.0f,imag=0.0f;
        real=fmaf(row0[2u*lane],0.5f,real);
        imag=fmaf(row0[2u*lane+1u],0.5f,imag);
        real=fmaf(row1[2u*lane],0.5f,real);
        imag=fmaf(row1[2u*lane+1u],0.5f,imag);

        int32_t shift=stage_b_rt_spectral_shift(real);
        const int32_t imag_shift=stage_b_rt_spectral_shift(imag);
        if(imag_shift<shift)shift=imag_shift;
        const float normalization=stage_b_rt_pow2_integer(shift-1);
        const float scaled_real=normalization*real;
        const float scaled_imag=normalization*imag;
        const int32_t next_shift=shift*2;
        int32_t delta=s->shift[lane]-next_shift;
        if(delta>60)delta=60;
        if(delta<-60)delta=-60;

        float energy=fmaf(scaled_real,scaled_real,scaled_imag*scaled_imag);
        energy*=f32_bits(0x3d800000u);
        if(delta<0){
            s->energy[lane]=fmaf(stage_b_rt_pow2_integer(delta),energy,s->energy[lane]);
        }else{
            s->shift[lane]=next_shift;
            s->energy[lane]=fmaf(stage_b_rt_pow2_integer(-delta),s->energy[lane],energy);
        }
    }

    s->counter++;
    if(s->counter!=s->period)return;

    for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_SPECTRAL_BINS;lane++)
        if(s->shift[lane]<s->global_shift)s->global_shift=s->shift[lane];

    float aggregate=0.0f;
    for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_SPECTRAL_BINS;lane++){
        const int32_t delta=s->shift[lane]-s->global_shift;
        const float exponent_scale=stage_b_rt_pow2_integer(-(delta>>1));
        float value=sqrtf(s->energy[lane]);
        value*=s->output_scale;
        value*=exponent_scale;
        out->bins[lane]=value;
        aggregate=fmaf(value,f32_bits(0x3c000000u),aggregate);
    }
    out->aggregate=aggregate;
    out->count=UBIG_STAGE_B_RT_SPECTRAL_BINS;
    out->exponent=(s->global_shift>>1)-s->exponent_offset-1;
    s->counter=0u;
}

void ubig_stage_b_rt_variation_history_process(UbigStageBRtVariationHistory *state,
                                                const UbigStageBRtVariationConfig *config,
                                                const float *input,
                                                uint32_t input_count)
{
    if(!state||!config||!input||!config->boundaries||!config->weights||
       config->segment_count>UBIG_STAGE_B_RT_VARIATION_MAX_SEGMENTS)return;

    float energy=0.0f;
    for(uint32_t i=0;i<input_count;i++){
        const float square=input[i]*input[i];
        energy=fmaf(square,f32_bits(0x3c000000u),energy);
    }
    const float root=sqrtf(energy*0.5f);
    const int32_t shift=stage_b_rt_spectral_shift(root);
    const float normalized=stage_b_rt_pow2_integer(shift)*root;
    float *dst=state->history[state->index];

    for(uint32_t segment=0;segment<config->segment_count;segment++){
        float sum=0.0f;
        uint32_t position=config->boundaries[segment];
        const uint32_t end=config->boundaries[segment+1u]-1u;
        while(position<end){
            float delta=(input[position+1u]-input[position])*config->weights[segment];
            if(delta<0.0f)delta=-delta;
            sum+=delta;
            position++;
        }
        if(normalized==0.0f){
            dst[segment]=0.0f;
        }else{
            const float scaled=stage_b_rt_pow2_integer(shift-4)*sum;
            dst[segment]=(float)((double)scaled/(double)normalized);
        }
    }

    state->index++;
    if(state->index>=UBIG_STAGE_B_RT_VARIATION_HISTORY_DEPTH)state->index=0u;
}

static int32_t stage_b_rt_clamp60(int32_t value)
{
    if(value>60)value=60;
    if(value<-60)value=-60;
    return value;
}

void ubig_stage_b_rt_spectral_change_process(UbigStageBRtSpectralChangeHistory *s,
                                             const UbigStageBRtSpectralExport *in)
{
    if(!s||!in||in->count>UBIG_STAGE_B_RT_SPECTRAL_BINS)return;
    const uint32_t index=s->index;
    s->history[index]=0.0f;
    float average;
    float sum=0.0f;
    if(s->previous_exponent>=in->exponent){
        const int32_t delta=s->previous_exponent-in->exponent;
        const float previous=s->previous_aggregate*stage_b_rt_pow2_integer(-delta);
        average=fmaf(in->aggregate,0.5f,previous*0.5f);
        if(average>0.0f){
            const float previous_scale=stage_b_rt_pow2_integer(-stage_b_rt_clamp60(delta+7));
            for(uint32_t lane=0;lane<in->count;lane++){
                const float current=in->bins[lane]*f32_bits(0x3c000000u);
                float difference=fmaf(s->previous_bins[lane],previous_scale,-current);
                if(difference<0.0f)difference=-difference;
                sum+=difference;
            }
            sum*=0.5f;
            s->history[index]=(sum>=average)?1.0f:(float)((double)sum/(double)average);
        }
    }else{
        const int32_t delta=in->exponent-s->previous_exponent;
        const float current=in->aggregate*stage_b_rt_pow2_integer(-stage_b_rt_clamp60(delta+1));
        average=fmaf(s->previous_aggregate,0.5f,current);
        if(average>0.0f){
            const float current_scale=stage_b_rt_pow2_integer(-stage_b_rt_clamp60(delta+7));
            for(uint32_t lane=0;lane<in->count;lane++){
                const float current_bin=in->bins[lane]*current_scale;
                float difference=fmaf(s->previous_bins[lane],f32_bits(0x3c000000u),-current_bin);
                if(difference<0.0f)difference=-difference;
                sum+=difference;
            }
            sum*=0.5f;
            s->history[index]=(sum>=average)?1.0f:(float)((double)sum/(double)average);
        }
    }
    s->index=index+1u;
    if(s->index>=UBIG_STAGE_B_RT_CHANGE_HISTORY_DEPTH)s->index=0u;
    memcpy(s->previous_bins,in->bins,in->count*sizeof(float));
    s->previous_exponent=in->exponent;
    s->previous_aggregate=in->aggregate;
}

float ubig_stage_b_rt_ratio_map_mode(float ratio,int32_t mode)
{
    float mode_term=(float)mode*f32_bits(0x38000000u);
    mode_term*=f32_bits(0x44000000u);
    if(mode==0&&ratio==1.0f)return 0.0f;
    const int32_t shift=stage_b_rt_spectral_shift(ratio);
    float polynomial=1.0f;
    if(shift<31){
        const float normalized=stage_b_rt_pow2_integer(shift)*ratio;
        const uint32_t fixed=(uint32_t)shift*0x02aaaaacu;
        const float square=normalized*normalized;
        float a=square*f32_bits(0x3f03d886u);
        const float cube=square*normalized;
        a=fmaf(-cube,f32_bits(0x3e1ce39cu),a);
        const float b=fmaf(-normalized,f32_bits(0x3f412715u),f32_bits(0x3ec901c2u));
        const float correction=(a+b)*f32_bits(0x3e2aaaabu);
        polynomial=fmaf((float)(int32_t)fixed,f32_bits(0x30000000u),correction);
    }
    const float mapped=fmaf(-polynomial,0.75f,mode_term);
    return mapped*f32_bits(0x3f317218u);
}

float ubig_stage_b_rt_ratio_map(float ratio)
{
    return ubig_stage_b_rt_ratio_map_mode(ratio,0);
}

void ubig_stage_b_rt_segment_ratio_process(UbigStageBRtSegmentRatioHistory *s,
                                           const UbigStageBRtSegmentRatioConfig *config,
                                           const UbigStageBRtSpectralExport *in)
{
    if(!s||!config||!config->boundaries||!in)return;
    float *dst=s->history[s->index];
    for(uint32_t segment=0;segment<UBIG_STAGE_B_RT_SEGMENT_RATIO_COUNT;segment++){
        dst[segment]=0.0f;
        const uint32_t begin=config->boundaries[segment];
        const uint32_t end=config->boundaries[segment+1u];
        float maximum=in->bins[begin];
        float minimum=maximum;
        for(uint32_t lane=begin+1u;lane<end;lane++){
            const float value=in->bins[lane];
            if(maximum<value)maximum=value;
            if(value<minimum)minimum=value;
        }
        if(minimum+f32_bits(0x3456bf95u)>=maximum)continue;

        const float upper=fmaf(maximum,f32_bits(0x3f4ccccdu),minimum*f32_bits(0x3e4ccccdu));
        const float lower=fmaf(maximum,f32_bits(0x3e4ccccdu),minimum*f32_bits(0x3f4ccccdu));
        float high_sum=0.0f,low_sum=0.0f;
        uint32_t high_count=0u,low_count=0u;
        for(uint32_t lane=begin;lane<end;lane++){
            const float value=in->bins[lane];
            if(value>=upper){
                high_sum=fmaf(value,f32_bits(0x3d800000u),high_sum);
                high_count++;
            }else if(value<=lower){
                low_sum=fmaf(value,f32_bits(0x3d800000u),low_sum);
                low_count++;
            }
        }
        if(high_count){
            const float count_scaled=(float)high_count*f32_bits(0x38000000u);
            const float inverse=(float)((double)f32_bits(0x38000000u)/(double)count_scaled);
            high_sum*=inverse;
            high_sum*=16.0f;
            high_sum*=stage_b_rt_pow2_integer(-in->exponent);
        }
        if(low_count){
            const float count_scaled=(float)low_count*f32_bits(0x38000000u);
            const float inverse=(float)((double)f32_bits(0x38000000u)/(double)count_scaled);
            low_sum*=inverse;
            low_sum*=16.0f;
            low_sum*=stage_b_rt_pow2_integer(-in->exponent);
        }
        const float ratio=(float)((double)(low_sum+f32_bits(0x3727c5acu))/
                                  (double)(high_sum+f32_bits(0x3727c5acu)));
        dst[segment]=-ubig_stage_b_rt_ratio_map(ratio)*2.0f;
    }
    s->index++;
    if(s->index>=UBIG_STAGE_B_RT_SEGMENT_RATIO_DEPTH)s->index=0u;
}

void ubig_stage_b_rt_peak_residual_process(UbigStageBRtPeakResidualHistory *s,
                                           const UbigStageBRtSpectralExport *in,
                                           float scratch[UBIG_STAGE_B_RT_SPECTRAL_BINS])
{
    if(!s||!in||!scratch||in->count==0u||in->count>UBIG_STAGE_B_RT_SPECTRAL_BINS)return;
    const uint32_t count=in->count;
    memcpy(scratch,in->bins,count*sizeof(float));

    uint32_t peak=0u;
    float maximum=in->bins[0];
    for(uint32_t lane=1u;lane<count;lane++){
        if(maximum<in->bins[lane]){maximum=in->bins[lane];peak=lane;}
    }
    uint32_t begin=peak>5u?peak-5u:0u;
    uint32_t end=peak<count-5u?peak+5u:count;
    float first=0.0f;
    for(uint32_t lane=begin;lane<end;lane++){
        first=fmaf(in->bins[lane],f32_bits(0x3d800000u),first);
        scratch[lane]=0.0f;
    }

    peak=0u;
    maximum=scratch[0];
    for(uint32_t lane=1u;lane<count;lane++){
        if(maximum<scratch[lane]){maximum=scratch[lane];peak=lane;}
    }
    begin=peak>5u?peak-5u:0u;
    end=peak<count-5u?peak+5u:count;
    float second=0.0f;
    for(uint32_t lane=begin;lane<end;lane++)
        second=fmaf(scratch[lane],f32_bits(0x3d800000u),second);

    float residual1=in->aggregate-first*0.125f;
    if(residual1<0.0f)residual1=0.0f;
    float residual2=residual1-second*0.125f;
    if(residual2<0.0f)residual2=0.0f;
    const float scale=stage_b_rt_pow2_integer(-in->exponent);
    float *dst=s->history[s->index];
    dst[0]=residual1*scale;
    dst[1]=residual2*scale;
    dst[2]=(residual1>0.0f?second:0.0f)*scale;
    s->index++;
    if(s->index>=UBIG_STAGE_B_RT_PEAK_HISTORY_DEPTH)s->index=0u;
}

void ubig_stage_b_rt_feature_change_process(UbigStageBRtFeatureChangeHistory *s,
                                            const float input[UBIG_STAGE_B_RT_FEATURE_COUNT],
                                            float normalized[UBIG_STAGE_B_RT_FEATURE_COUNT])
{
    if(!s||!input||!normalized)return;
    int32_t shift=stage_b_rt_spectral_shift(input[0]);
    int32_t previous_shift=stage_b_rt_spectral_shift(s->previous[0]);
    for(uint32_t lane=1;lane<UBIG_STAGE_B_RT_FEATURE_COUNT;lane++){
        int32_t value=stage_b_rt_spectral_shift(input[lane]);
        if(value<shift)shift=value;
        value=stage_b_rt_spectral_shift(s->previous[lane]);
        if(value<previous_shift)previous_shift=value;
    }
    if(previous_shift<shift)shift=previous_shift;
    const float scale=stage_b_rt_pow2_integer(shift);
    for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_FEATURE_COUNT;lane++){
        normalized[lane]=input[lane]*scale;
        s->previous[lane]*=scale;
    }

    float energy=0.0f;
    for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_FEATURE_COUNT;lane++){
        const float previous=s->previous[lane]*0.25f;
        const float current=normalized[lane]*0.25f;
        float pair=previous*previous;
        pair=fmaf(current,current,pair);
        energy=energy+pair;
    }

    float metric=0.0f;
    if(energy!=0.0f){
        const float root=sqrtf(energy);
        float sum=0.0f;
        for(uint32_t lane=0;lane<UBIG_STAGE_B_RT_FEATURE_COUNT;lane++){
            const float current=normalized[lane]*0.5f;
            float difference=fmaf(-s->previous[lane],0.5f,current);
            difference*=0.125f;
            if(difference<0.0f)difference=-difference;
            sum+=difference;
        }
        sum*=0.5f;
        metric=(float)((double)sum/(double)root);
    }
    s->history[s->index]=metric;
    s->index++;
    if(s->index>=UBIG_STAGE_B_RT_FEATURE_CHANGE_DEPTH)s->index=0u;
    memcpy(s->previous,input,sizeof s->previous);
}

float ubig_stage_b_rt_scaled_sum(const float *input,uint32_t count,int32_t exponent)
{
    if(!input||count==0u)return 0.0f;
    const float scale=stage_b_rt_pow2_integer(-exponent);
    float sum=input[0]*scale;
    for(uint32_t i=1;i<count;i++)sum=fmaf(input[i],scale,sum);
    return sum;
}

typedef struct { float lane[4]; } StageBRtVec4;

static StageBRtVec4 stage_b_rt_v4_load(const float *src)
{
    StageBRtVec4 out;
    memcpy(out.lane,src,sizeof out.lane);
    return out;
}

static void stage_b_rt_v4_store(float *dst,StageBRtVec4 value)
{
    memcpy(dst,value.lane,sizeof value.lane);
}

static StageBRtVec4 stage_b_rt_v4_add(StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out;
    for(uint32_t i=0u;i<4u;i++)out.lane[i]=a.lane[i]+b.lane[i];
    return out;
}

static StageBRtVec4 stage_b_rt_v4_sub(StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out;
    for(uint32_t i=0u;i<4u;i++)out.lane[i]=a.lane[i]-b.lane[i];
    return out;
}

static StageBRtVec4 stage_b_rt_v4_mul(StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out;
    for(uint32_t i=0u;i<4u;i++)out.lane[i]=a.lane[i]*b.lane[i];
    return out;
}

static StageBRtVec4 stage_b_rt_v4_fma(StageBRtVec4 acc,StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out;
    for(uint32_t i=0u;i<4u;i++)out.lane[i]=fmaf(a.lane[i],b.lane[i],acc.lane[i]);
    return out;
}

static StageBRtVec4 stage_b_rt_v4_fms(StageBRtVec4 acc,StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out;
    for(uint32_t i=0u;i<4u;i++)out.lane[i]=fmaf(-a.lane[i],b.lane[i],acc.lane[i]);
    return out;
}

static StageBRtVec4 stage_b_rt_v4_uzp1(StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out={{a.lane[0],a.lane[2],b.lane[0],b.lane[2]}};
    return out;
}

static StageBRtVec4 stage_b_rt_v4_uzp2(StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out={{a.lane[1],a.lane[3],b.lane[1],b.lane[3]}};
    return out;
}

static StageBRtVec4 stage_b_rt_v4_zip1(StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out={{a.lane[0],b.lane[0],a.lane[1],b.lane[1]}};
    return out;
}

static StageBRtVec4 stage_b_rt_v4_zip2(StageBRtVec4 a,StageBRtVec4 b)
{
    StageBRtVec4 out={{a.lane[2],b.lane[2],a.lane[3],b.lane[3]}};
    return out;
}

static StageBRtVec4 stage_b_rt_v4_bits(const uint32_t bits[4])
{
    StageBRtVec4 out;
    for(uint32_t i=0u;i<4u;i++)out.lane[i]=f32_bits(bits[i]);
    return out;
}

/* Exact forward complex FFT-16 schedule selected by the deployed spectrum32
 * reference path.  The constants are ordinary roots of unity, represented by
 * their binary32 encodings so no proprietary tuning data enters UbiG. */
static void stage_b_rt_fft16(float output[32],const float input[32])
{
    static const uint32_t twiddle[24]={
        0x3f800000u,0x3f6c8366u,0x3f3504f7u,0x3ec3ef07u,
        0x80000000u,0xbec3ef07u,0xbf3504f7u,0xbf6c8366u,
        0x3f800000u,0x3f3504f7u,0x00000000u,0xbf3504f7u,
        0x80000000u,0xbf3504f7u,0xbf800000u,0xbf3504f7u,
        0x3f800000u,0x3ec3ef07u,0xbf3504f7u,0xbf6c8366u,
        0x80000000u,0xbf6c8366u,0xbf3504f7u,0x3ec3ef07u
    };
    StageBRtVec4 a0=stage_b_rt_v4_load(input),a1=stage_b_rt_v4_load(input+4);
    StageBRtVec4 a2=stage_b_rt_v4_load(input+8),a3=stage_b_rt_v4_load(input+12);
    StageBRtVec4 a4=stage_b_rt_v4_load(input+16),a5=stage_b_rt_v4_load(input+20);
    StageBRtVec4 a6=stage_b_rt_v4_load(input+24),a7=stage_b_rt_v4_load(input+28);
    StageBRtVec4 v23=stage_b_rt_v4_uzp1(a0,a1),v27=stage_b_rt_v4_uzp2(a0,a1);
    StageBRtVec4 v22=stage_b_rt_v4_uzp1(a2,a3),v26=stage_b_rt_v4_uzp2(a2,a3);
    StageBRtVec4 v21=stage_b_rt_v4_uzp1(a4,a5),v20=stage_b_rt_v4_uzp2(a4,a5);
    StageBRtVec4 v18=stage_b_rt_v4_uzp1(a6,a7),v16=stage_b_rt_v4_uzp2(a6,a7);
    StageBRtVec4 v19=stage_b_rt_v4_add(v23,v21),v25=stage_b_rt_v4_sub(v23,v21);
    v23=stage_b_rt_v4_add(v27,v20);
    StageBRtVec4 v17=stage_b_rt_v4_add(v22,v18),v24=stage_b_rt_v4_sub(v22,v18);
    v21=stage_b_rt_v4_add(v26,v16);v18=stage_b_rt_v4_sub(v26,v16);
    v16=stage_b_rt_v4_sub(v19,v17);v17=stage_b_rt_v4_add(v19,v17);
    StageBRtVec4 v22b=stage_b_rt_v4_sub(v27,v20);
    StageBRtVec4 v19z=stage_b_rt_v4_zip2(v17,v16),v20z=stage_b_rt_v4_zip1(v17,v16);
    v17=stage_b_rt_v4_add(v25,v18);v16=stage_b_rt_v4_sub(v25,v18);
    StageBRtVec4 v18z=stage_b_rt_v4_zip1(v17,v16),v16z=stage_b_rt_v4_zip2(v17,v16);
    StageBRtVec4 v17a=stage_b_rt_v4_zip1(v20z,v18z),v31=stage_b_rt_v4_zip2(v20z,v18z);
    StageBRtVec4 v30=stage_b_rt_v4_zip1(v19z,v16z),v29=stage_b_rt_v4_zip2(v19z,v16z);
    v16=stage_b_rt_v4_add(v23,v21);v18=stage_b_rt_v4_sub(v23,v21);
    v19=stage_b_rt_v4_add(v22b,v24);
    StageBRtVec4 v21z=stage_b_rt_v4_zip1(v16,v18),v20b=stage_b_rt_v4_zip2(v16,v18);
    v16=stage_b_rt_v4_sub(v22b,v24);
    StageBRtVec4 v18b=stage_b_rt_v4_zip1(v16,v19),v16b=stage_b_rt_v4_zip2(v16,v19);
    StageBRtVec4 v26z=stage_b_rt_v4_zip2(v21z,v18b);
    v25=stage_b_rt_v4_zip1(v20b,v16b);v24=stage_b_rt_v4_zip2(v20b,v16b);
    StageBRtVec4 v28=stage_b_rt_v4_zip1(v21z,v18b);
    StageBRtVec4 t23=stage_b_rt_v4_bits(twiddle),t20=stage_b_rt_v4_bits(twiddle+4);
    StageBRtVec4 t19=stage_b_rt_v4_bits(twiddle+8),t18=stage_b_rt_v4_bits(twiddle+12);
    StageBRtVec4 t16=stage_b_rt_v4_bits(twiddle+16),t22=stage_b_rt_v4_bits(twiddle+20);
    StageBRtVec4 v27m=stage_b_rt_v4_mul(v26z,t23),v21m=stage_b_rt_v4_mul(v31,t23);
    v27m=stage_b_rt_v4_fma(v27m,v31,t20);v21m=stage_b_rt_v4_fms(v21m,v26z,t20);
    StageBRtVec4 v20m=stage_b_rt_v4_mul(v30,t19),v19m=stage_b_rt_v4_mul(v25,t19);
    v20m=stage_b_rt_v4_fms(v20m,v25,t18);v19m=stage_b_rt_v4_fma(v19m,v30,t18);
    StageBRtVec4 v18m=stage_b_rt_v4_mul(v29,t16),v16m=stage_b_rt_v4_mul(v24,t16);
    v18m=stage_b_rt_v4_fms(v18m,v24,t22);v16m=stage_b_rt_v4_fma(v16m,v29,t22);
    StageBRtVec4 o26=stage_b_rt_v4_add(v17a,v20m),o25=stage_b_rt_v4_sub(v17a,v20m);
    StageBRtVec4 o24=stage_b_rt_v4_add(v28,v19m),o23=stage_b_rt_v4_sub(v28,v19m);
    StageBRtVec4 o22=stage_b_rt_v4_add(v21m,v18m),o20=stage_b_rt_v4_add(v27m,v16m);
    StageBRtVec4 o21=stage_b_rt_v4_sub(v21m,v18m),o19=stage_b_rt_v4_sub(v27m,v16m);
    StageBRtVec4 q17=stage_b_rt_v4_add(o26,o22),q18=stage_b_rt_v4_add(o24,o20);
    v16=stage_b_rt_v4_zip1(q17,q18);v17=stage_b_rt_v4_zip2(q17,q18);
    stage_b_rt_v4_store(output,v16);stage_b_rt_v4_store(output+4,v17);
    q18=stage_b_rt_v4_sub(o23,o21);q17=stage_b_rt_v4_add(o25,o19);
    v16=stage_b_rt_v4_zip1(q17,q18);v17=stage_b_rt_v4_zip2(q17,q18);
    stage_b_rt_v4_store(output+8,v16);stage_b_rt_v4_store(output+12,v17);
    q18=stage_b_rt_v4_sub(o24,o20);q17=stage_b_rt_v4_sub(o26,o22);
    v16=stage_b_rt_v4_zip1(q17,q18);v17=stage_b_rt_v4_zip2(q17,q18);
    stage_b_rt_v4_store(output+16,v16);stage_b_rt_v4_store(output+20,v17);
    q18=stage_b_rt_v4_add(o23,o21);q17=stage_b_rt_v4_sub(o25,o19);
    v16=stage_b_rt_v4_zip1(q17,q18);v17=stage_b_rt_v4_zip2(q17,q18);
    stage_b_rt_v4_store(output+24,v16);stage_b_rt_v4_store(output+28,v17);
}

static void stage_b_rt_real_post16(float data[34])
{
    static const uint32_t sine[7]={
        0x3e47c5c2u,0x3ec3ef15u,0x3f0e39dau,0x3f3504f3u,
        0x3f54db31u,0x3f6c835eu,0x3f7b14beu
    };
    const float half=0.5f;
    const float real0=data[0],imag0=data[1];
    data[0]=fmaf(imag0,1.0f,real0);
    data[1]=fmaf(-imag0,1.0f,real0);
    data[16]*=1.0f;
    data[17]=-(data[17]*1.0f);
    for(uint32_t k=1u;k<8u;k++){
        const float ar=data[2u*k],ai=data[2u*k+1u];
        const float br=data[32u-2u*k],bi=data[33u-2u*k];
        const float sinv=f32_bits(sine[k-1u]),cosv=f32_bits(sine[7u-k]);
        const float sum_i=fmaf(bi,half,ai*half);
        const float diff_r=fmaf(br,half,-(ar*half));
        const float diff_i=fmaf(-bi,half,ai*half);
        const float sum_r=fmaf(br,half,ar*half);
        const float rot_i=fmaf(-sinv,sum_i,diff_r*cosv);
        const float rot_r=fmaf(cosv,sum_i,diff_r*sinv);
        data[2u*k]=sum_r+rot_r;
        data[2u*k+1u]=diff_i+rot_i;
        data[32u-2u*k]=sum_r-rot_r;
        data[33u-2u*k]=rot_i-diff_i;
    }
}

void ubig_stage_b_rt_spectrum32(const float input[32],float output[16])
{
    if(!input||!output)return;
    int32_t shift=32;
    for(uint32_t i=0u;i<32u;i++){
        const int32_t lane_shift=stage_b_rt_spectral_shift(input[i]);
        if(lane_shift<shift)shift=lane_shift;
    }
    float normalized[32],spectrum[34]={0};
    const float scale=stage_b_rt_pow2_integer(shift-5);
    const float one_over_32=f32_bits(0x3d000000u);
    float mean=0.0f;
    for(uint32_t i=0u;i<32u;i++){
        normalized[i]=input[i]*scale;
        mean=fmaf(normalized[i],one_over_32,mean);
    }
    /* Reference reciprocal-N lookup is 1/32 for the only deployed size, and
     * its final x32 multiply cancels exactly in binary32. Preserve both rounds. */
    const float normalized_mean=(mean*one_over_32)*f32_bits(0x42000000u);
    stage_b_rt_fft16(spectrum,normalized);
    stage_b_rt_real_post16(spectrum);
    spectrum[32]=spectrum[1];
    spectrum[33]=0.0f;
    spectrum[1]=0.0f;
    if(normalized_mean==0.0f){
        memset(output,0,16u*sizeof *output);
        return;
    }
    const int32_t mean_shift=stage_b_rt_spectral_shift(normalized_mean);
    const float scaled_mean=stage_b_rt_pow2_integer(mean_shift)*normalized_mean;
    const float inverse=(float)(0.5/(double)scaled_mean);
    const float post_scale=stage_b_rt_pow2_integer(mean_shift-5);
    for(uint32_t bin=1u;bin<=16u;bin++){
        const float real=spectrum[2u*bin],imag=spectrum[2u*bin+1u];
        float magnitude=sqrtf(fmaf(imag,imag,real*real));
        magnitude*=inverse;
        output[bin-1u]=magnitude*post_scale;
    }
}


void ubig_stage_b_rt_slope32_prepare(const float row0[UBIG_STAGE_B_RT_SLOPE32_VALUES],
                                     const float row1[UBIG_STAGE_B_RT_SLOPE32_VALUES],
                                     UbigStageBRtSlope32 *out)
{
    if(!row0||!row1||!out)return;
    int32_t shift=32;
    for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_SLOPE32_VALUES;lane++){
        int32_t lane_shift=stage_b_rt_spectral_shift(row0[lane]);
        if(lane_shift<shift)shift=lane_shift;
        lane_shift=stage_b_rt_spectral_shift(row1[lane]);
        if(lane_shift<shift)shift=lane_shift;
    }

    const float input_scale=stage_b_rt_pow2_integer(shift-1);
    for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_SLOPE32_VALUES;lane++){
        out->normalized_row0[lane]=row0[lane]*input_scale;
        out->normalized_row1[lane]=row1[lane]*input_scale;
    }

    const float coefficient=f32_bits(0x3f0a9555u);
    for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_SLOPE32_VALUES;lane++){
        float value=0.0f;
        if(lane>0u)value=fmaf(out->normalized_row0[lane-1u],-coefficient,value);
        value=fmaf(out->normalized_row0[lane],0.0f,value);
        if(lane+1u<UBIG_STAGE_B_RT_SLOPE32_VALUES)
            value=fmaf(out->normalized_row0[lane+1u],coefficient,value);
        out->positive_row0[lane]=value;

        value=0.0f;
        if(lane>0u)value=fmaf(out->normalized_row1[lane-1u],-coefficient,value);
        value=fmaf(out->normalized_row1[lane],0.0f,value);
        if(lane+1u<UBIG_STAGE_B_RT_SLOPE32_VALUES)
            value=fmaf(out->normalized_row1[lane+1u],coefficient,value);
        out->positive_row1[lane]=value;
    }

    for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_SLOPE32_VALUES;lane++){
        if(out->positive_row0[lane]<0.0f)out->positive_row0[lane]=0.0f;
        if(out->positive_row1[lane]<0.0f)out->positive_row1[lane]=0.0f;
        out->combined[lane]=out->positive_row0[lane]+out->positive_row1[lane];
    }

    const float one_over_32=f32_bits(0x3d000000u);
    float row1_mean=out->normalized_row1[0]*one_over_32;
    for(uint32_t lane=1u;lane<UBIG_STAGE_B_RT_SLOPE32_VALUES;lane++)
        row1_mean=fmaf(out->normalized_row1[lane],one_over_32,row1_mean);
    float row0_mean=out->normalized_row0[0]*one_over_32;
    for(uint32_t lane=1u;lane<UBIG_STAGE_B_RT_SLOPE32_VALUES;lane++)
        row0_mean=fmaf(out->normalized_row0[lane],one_over_32,row0_mean);
    const float mean=row0_mean+row1_mean;
    if(mean>0.0f){
        const int32_t mean_shift=stage_b_rt_spectral_shift(mean);
        const float scaled_mean=stage_b_rt_pow2_integer(mean_shift)*mean;
        const float inverse=(float)(0.5/(double)scaled_mean);
        const float output_scale=stage_b_rt_pow2_integer(mean_shift-4);
        for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_SLOPE32_VALUES;lane++){
            const float normalized=out->combined[lane]*inverse;
            out->combined[lane]=normalized*output_scale;
        }
    }
}


static float stage_b_rt_reciprocal_count(uint32_t count)
{
    return 1.0f/(float)count;
}

static float stage_b_rt_scaled_mean32(const float input[32])
{
    const float scale=f32_bits(0x3d000000u);
    float sum=input[0]*scale;
    for(uint32_t lane=1u;lane<32u;lane++)sum=sum+input[lane]*scale;
    return sum;
}

static void stage_b_rt_autocorrelation32(const float input[32],float output[32])
{
    int32_t shift=32;
    for(uint32_t lane=0u;lane<32u;lane++){
        const int32_t lane_shift=stage_b_rt_spectral_shift(input[lane]);
        if(lane_shift<shift)shift=lane_shift;
    }
    const float normalize=stage_b_rt_pow2_integer(shift);
    const float one_over_32=f32_bits(0x3d000000u);
    const float thirty_two=f32_bits(0x42000000u);
    for(uint32_t lag=0u;lag<25u;lag++){
        float sum=0.0f;
        uint32_t base=0u;
        for(uint32_t lane=lag;lane<32u;lane++,base++){
            const float lhs=one_over_32*normalize*input[lane];
            const float rhs=normalize*input[base];
            sum=sum+lhs*rhs;
        }
        output[lag]=stage_b_rt_reciprocal_count(32u-lag)*sum*thirty_two;
    }

    const float root=output[0];
    const int32_t root_shift=stage_b_rt_spectral_shift(root);
    const float denominator_scale=stage_b_rt_pow2_integer(root_shift);
    const float numerator_scale=stage_b_rt_pow2_integer(root_shift-4);
    if(root==0.0f){
        for(uint32_t lane=0u;lane<25u;lane++)output[lane]=0.0f;
    }else{
        for(uint32_t lane=0u;lane<25u;lane++){
            const float numerator=numerator_scale*output[lane];
            const float denominator=denominator_scale*root;
            output[lane]=(float)((double)numerator/(double)denominator);
        }
    }
}

static int stage_b_rt_local_max2(const float *values,uint32_t lane)
{
    for(uint32_t distance=1u;distance<=2u;distance++)
        if(values[lane]<values[lane-distance]||values[lane]<=values[lane+distance])return 0;
    return 1;
}

static int stage_b_rt_local_min2(const float *values,uint32_t lane)
{
    for(uint32_t distance=1u;distance<=2u;distance++)
        if(values[lane-distance]<values[lane]||values[lane+distance]<=values[lane])return 0;
    return 1;
}

static void stage_b_rt_sort_ascending(float *values,uint32_t count)
{
    if(count<=1u)return;
    for(uint32_t pass=1u;pass<count;pass++){
        for(uint32_t lane=1u;pass+lane-1u<count;lane++){
            if(values[lane]<values[lane-1u]){
                const float swap=values[lane-1u];
                values[lane-1u]=values[lane];
                values[lane]=swap;
            }
        }
    }
}

static float stage_b_rt_transfer_log(float input,int32_t mode)
{
    const float mode_a=f32_bits(0x38000000u);
    const float mode_b=f32_bits(0x44000000u);
    const float log_scale=f32_bits(0x3f317218u);
    const float cubic_a=f32_bits(0x3f03d886u);
    const float cubic_b=f32_bits(0x3e1ce39cu);
    const float linear=f32_bits(0x3f412715u);
    const float bias=f32_bits(0x3ec901c2u);
    const float polynomial_scale=f32_bits(0x3e2aaaabu);
    const float exponent_scale=f32_bits(0x30000000u);

    float mode_term=(float)mode*mode_a;
    mode_term*=mode_b;
    float approximation=1.0f;
    if(mode==0&&input==1.0f)return 0.0f*log_scale;
    const int32_t shift=stage_b_rt_spectral_shift(input);
    if(shift<31){
        const float normalized=stage_b_rt_pow2_integer(shift)*input;
        const int32_t exponent_term=(int32_t)((uint32_t)shift*0x02aaaaacu);
        const float square=normalized*normalized;
        float high=square*cubic_a;
        const float cube=square*normalized;
        high=fmaf(-cube,cubic_b,high);
        float low=fmaf(-normalized,linear,bias);
        low=high+low;
        low*=polynomial_scale;
        approximation=fmaf((float)exponent_term,exponent_scale,low);
    }
    const float mapped=fmaf(-approximation,0.75f,mode_term);
    return mapped*log_scale;
}

static float stage_b_rt_mean_top2(const float *values,uint32_t count)
{
    if(count==0u)return 0.0f;
    int32_t shift=stage_b_rt_spectral_shift(values[0]);
    for(uint32_t lane=1u;lane<count;lane++){
        const int32_t lane_shift=stage_b_rt_spectral_shift(values[lane]);
        if(lane_shift<shift)shift=lane_shift;
    }
    const int32_t exponent=5-shift;
    const float normalize=stage_b_rt_pow2_integer(-exponent);
    float sum=values[0]*normalize;
    for(uint32_t lane=1u;lane<count;lane++)sum=sum+values[lane]*normalize;
    const float reciprocal=stage_b_rt_reciprocal_count(count);
    const float restore=stage_b_rt_pow2_integer(exponent);
    if(exponent<0)return (reciprocal*sum)*restore;
    return (reciprocal*restore)*sum;
}

void ubig_stage_b_rt_slope32_features(UbigStageBRtSlope32 *workspace,
                                      float output[UBIG_STAGE_B_RT_SLOPE_FEATURES])
{
    if(!workspace||!output)return;
    float threshold=stage_b_rt_scaled_mean32(workspace->combined);
    threshold*=f32_bits(0x3d000000u);
    threshold*=f32_bits(0x42000000u);

    stage_b_rt_autocorrelation32(workspace->combined,workspace->positive_row0);

    uint32_t peak_count=0u;
    float peak_sum=0.0f;
    const float one_over_16=f32_bits(0x3d800000u);
    for(uint32_t lane=2u;lane<=29u;lane++){
        if(stage_b_rt_local_max2(workspace->combined,lane)&&threshold<workspace->combined[lane]){
            peak_count++;
            peak_sum=fmaf(workspace->combined[lane],one_over_16,peak_sum);
        }
    }

    const float count_unit=f32_bits(0x38000000u);
    output[1]=(float)((double)((float)peak_count*count_unit)*1024.0);
    if(peak_count==0u){
        output[0]=0.0f;
    }else{
        const float average=stage_b_rt_reciprocal_count(peak_count)*peak_sum;
        output[0]=stage_b_rt_transfer_log(fmaf(average,16.0f,f32_bits(0x3c800000u)),6);
    }

    float *maxima=workspace->positive_row1;
    float *minima=workspace->normalized_row0;
    uint32_t maximum_count=0u,minimum_count=0u;
    const float maximum_floor=f32_bits(0x3bcccccdu);
    for(uint32_t lane=2u;lane<=22u;lane++){
        if(stage_b_rt_local_max2(workspace->positive_row0,lane)&&
           maximum_floor<workspace->positive_row0[lane])
            maxima[maximum_count++]=workspace->positive_row0[lane];
        if(stage_b_rt_local_min2(workspace->positive_row0,lane))
            minima[minimum_count++]=workspace->positive_row0[lane];
    }

    const uint32_t maximum_use=maximum_count<2u?maximum_count:2u;
    const uint32_t minimum_use=minimum_count<2u?minimum_count:2u;
    stage_b_rt_sort_ascending(maxima,maximum_count);
    stage_b_rt_sort_ascending(minima,minimum_count);

    float metric=0.0f;
    if(maximum_use!=0u){
        const float *selected=maxima+(maximum_count-maximum_use);
        const float scale=f32_bits(0x3d000000u);
        float sum=selected[0]*scale;
        for(uint32_t lane=1u;lane<maximum_use;lane++)sum=fmaf(selected[lane],scale,sum);
        metric=stage_b_rt_reciprocal_count(maximum_use)*sum;
        metric*=f32_bits(0x42000000u);
    }
    output[2]=metric*0.5f;

    const float valley=minimum_use?stage_b_rt_mean_top2(minima,minimum_use):0.0f;
    if(valley==0.0f){
        metric=0.0f;
    }else if(f32_bits(0x39000000u)<=valley){
        const int32_t valley_shift=stage_b_rt_spectral_shift(valley);
        const float numerator=stage_b_rt_pow2_integer(valley_shift-13)*metric;
        const float denominator=stage_b_rt_pow2_integer(valley_shift)*valley;
        metric=(float)((double)numerator/(double)denominator);
    }
    output[3]=metric;
}

float ubig_stage_b_rt_deviation32(float mean,const float input[32],uint32_t shift)
{
    if(!input)return 0.0f;
    const float scale=stage_b_rt_pow2_integer((int32_t)shift-1);
    const float scaled_mean=mean*scale;
    float energy=0.0f;
    for(uint32_t lane=0u;lane<32u;lane++){
        const float centered=fmaf(input[lane],scale,-scaled_mean);
        const float square=centered*centered;
        energy=fmaf(square,f32_bits(0x3d000000u),energy);
    }
    energy*=f32_bits(0x3d000000u);
    energy*=f32_bits(0x42000000u);
    return sqrtf(energy)*stage_b_rt_pow2_integer(1-(int32_t)shift);
}

void ubig_stage_b_rt_stat32(const float input[32],float *mean,float *deviation)
{
    if(!input||!mean||!deviation)return;
    int32_t shift=stage_b_rt_spectral_shift(input[0]);
    for(uint32_t lane=1u;lane<32u;lane++){
        const int32_t lane_shift=stage_b_rt_spectral_shift(input[lane]);
        if(lane_shift<shift)shift=lane_shift;
    }

    const float sum_scale=stage_b_rt_pow2_integer(shift-5);
    float average=input[0]*sum_scale;
    for(uint32_t lane=1u;lane<32u;lane++)
        average=fmaf(input[lane],sum_scale,average);
    average*=f32_bits(0x3d000000u);
    average*=stage_b_rt_pow2_integer(5-shift);
    *mean=average;

    const float center_scale=stage_b_rt_pow2_integer(shift-1);
    const float centered_average=average*center_scale;
    float energy=0.0f;
    for(uint32_t lane=0u;lane<32u;lane++){
        const float centered=fmaf(input[lane],center_scale,-centered_average);
        const float square=centered*centered;
        energy=fmaf(square,f32_bits(0x3d000000u),energy);
    }
    energy*=f32_bits(0x3d000000u);
    energy*=f32_bits(0x42000000u);
    *deviation=sqrtf(energy)*stage_b_rt_pow2_integer(1-shift);
}

static float stage_b_rt_cadence_mean(float accumulator,uint32_t accumulator_shift,
                                      float outgoing,uint32_t *used_shift)
{
    uint32_t shift=(uint32_t)stage_b_rt_spectral_shift(outgoing);
    if(accumulator_shift<shift)shift=accumulator_shift;
    const float accumulator_term=stage_b_rt_pow2_integer((int32_t)shift-(int32_t)accumulator_shift)*accumulator;
    const float outgoing_term=stage_b_rt_pow2_integer((int32_t)shift-5)*outgoing;
    float sum=accumulator_term+outgoing_term;
    sum*=f32_bits(0x3d000000u);
    *used_shift=shift;
    return stage_b_rt_pow2_integer(5-(int32_t)shift)*sum;
}

void ubig_stage_b_rt_cadence_summary_process(UbigStageBRtCadenceSummary *s,
                                             float output[UBIG_STAGE_B_RT_CADENCE_OUTPUTS],
                                             float scratch[32])
{
    if(!s||!output||!scratch)return;
    const uint32_t previous=(s->cursor.index==0u)?31u:s->cursor.index-1u;

    for(uint32_t column=0u;column<UBIG_STAGE_B_RT_CADENCE_COLUMNS;column++){
        for(uint32_t row=0u;row<32u;row++)scratch[row]=s->matrix[row][column];
        uint32_t shift;
        output[column]=stage_b_rt_cadence_mean(s->column_accumulator[column],
                                                s->column_shift[column],
                                                s->matrix[previous][column],&shift);
        output[UBIG_STAGE_B_RT_CADENCE_COLUMNS+column]=
            ubig_stage_b_rt_deviation32(output[column],scratch,shift);
    }

    for(uint32_t column=0u;column<UBIG_STAGE_B_RT_CADENCE_DELTAS;column++){
        for(uint32_t row=0u;row<32u;row++){
            const float high=s->matrix[row][column+1u]*0.5f;
            scratch[row]=fmaf(-s->matrix[row][column],0.5f,high);
        }
        const float previous_high=s->matrix[previous][column+1u]*0.5f;
        const float outgoing=fmaf(-s->matrix[previous][column],0.5f,previous_high);
        uint32_t shift;
        const uint32_t mean_index=2u*UBIG_STAGE_B_RT_CADENCE_COLUMNS+column;
        output[mean_index]=stage_b_rt_cadence_mean(s->delta_accumulator[column],
                                                   s->delta_shift[column],outgoing,&shift);
        output[mean_index+UBIG_STAGE_B_RT_CADENCE_DELTAS]=
            ubig_stage_b_rt_deviation32(output[mean_index],scratch,shift);
    }

    uint32_t next=s->cursor.index+s->cursor.step;
    if(next>=32u)next-=32u;
    s->cursor.index=next;
}

void ubig_stage_b_rt_stat32_step(UbigStageBRtStatCursor *cursor,
                                 const float input[32],
                                 float scratch[32],
                                 float output[2])
{
    if(!cursor||!input||!scratch||!output)return;
    memcpy(scratch,input,32u*sizeof(float));
    ubig_stage_b_rt_stat32(scratch,&output[0],&output[1]);
    uint32_t next=cursor->index+cursor->step;
    if(next>=32u)next-=32u;
    cursor->index=next;
}

void ubig_stage_b_rt_stat32_columns(UbigStageBRtStatCursor *cursor,
                                    const float matrix[32][UBIG_STAGE_B_RT_STAT_COLUMNS],
                                    uint32_t count,
                                    float scratch[32],
                                    float mean[UBIG_STAGE_B_RT_STAT_COLUMNS],
                                    float deviation[UBIG_STAGE_B_RT_STAT_COLUMNS])
{
    if(!cursor||!matrix||!scratch||!mean||!deviation||count>UBIG_STAGE_B_RT_STAT_COLUMNS)return;
    for(uint32_t column=0u;column<count;column++){
        for(uint32_t row=0u;row<32u;row++)scratch[row]=matrix[row][column];
        ubig_stage_b_rt_stat32(scratch,&mean[column],&deviation[column]);
    }
    uint32_t next=cursor->index+cursor->step;
    if(next>=32u)next-=32u;
    cursor->index=next;
}

void ubig_stage_b_rt_stat32_ring_columns(UbigStageBRtStatCursor *cursor,
                                         const float matrix[32][UBIG_STAGE_B_RT_STAT_COLUMNS],
                                         uint32_t prefix_count,
                                         float scratch[64],
                                         float mean[UBIG_STAGE_B_RT_STAT_COLUMNS],
                                         float deviation[UBIG_STAGE_B_RT_STAT_COLUMNS])
{
    if(!cursor||!matrix||!scratch||!mean||!deviation||prefix_count>32u)return;
    for(uint32_t column=0u;column<UBIG_STAGE_B_RT_STAT_COLUMNS;column++){
        uint32_t out=0u;
        for(uint32_t row=cursor->index;row<32u;row++)scratch[out++]=matrix[row][column];
        for(uint32_t row=0u;row<prefix_count;row++)scratch[out++]=matrix[row][column];
        ubig_stage_b_rt_stat32(scratch,&mean[column],&deviation[column]);
    }
    uint32_t next=cursor->index+cursor->step;
    if(next>=32u)next-=32u;
    cursor->index=next;
}

static float stage_b_rt_reduce32_exact(const float values[UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH],
                                       int32_t shift)
{
    const float scale=stage_b_rt_pow2_integer(shift-5);
    float sum=0.0f;
    for(uint32_t base=0;base<UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH;base+=16u){
        for(uint32_t lane=0;lane<15u;lane++)
            sum=fmaf(scale,values[base+lane],sum);
        const float last=scale*values[base+15u];
        sum=last+sum;
    }
    return sum;
}

void ubig_stage_b_rt_feature_history_process(UbigStageBRtFeatureHistory *s,
                                             const UbigStageBRtFeatureHistoryConfig *config,
                                             const UbigStageBRtSpectralExport *in)
{
    if(!s||!config||!config->boundaries||!in||in->count==0u||
       in->count>UBIG_STAGE_B_RT_SPECTRAL_BINS||
       config->scaled_sum_count==0u||config->scaled_sum_count>=in->count)return;

    const uint32_t index=s->index;
    float *record=s->records[index];
    const float aggregate=in->aggregate;
    if(aggregate==0.0f){
        memset(record,0,sizeof s->records[index]);
    }else{
        const int32_t shift=stage_b_rt_spectral_shift(aggregate);
        const float normalized=(float)(0.5/(double)(stage_b_rt_pow2_integer(shift)*aggregate));
        const float fraction_scale=stage_b_rt_pow2_integer(shift-3);
        for(uint32_t segment=0;segment<UBIG_STAGE_B_RT_FEATURE_SEGMENTS;segment++){
            float sum=0.0f;
            for(uint32_t lane=config->boundaries[segment];lane<config->boundaries[segment+1u];lane++)
                sum=fmaf(in->bins[lane],f32_bits(0x3e000000u),sum);
            record[12u+segment]=stage_b_rt_pow2_integer(-in->exponent)*sum;
            const float normalized_sum=sum*normalized;
            record[4u+segment]=fraction_scale*normalized_sum;
        }

        const float exponent_scale=stage_b_rt_pow2_integer(-in->exponent);
        const float aligned_aggregate=exponent_scale*aggregate;
        record[0]=ubig_stage_b_rt_ratio_map_mode(aligned_aggregate+f32_bits(0x3c000000u),7);
        record[1]=aligned_aggregate;

        const float scaled_sum=ubig_stage_b_rt_scaled_sum(in->bins+1u,config->scaled_sum_count,3);
        const float numerator=scaled_sum*f32_bits(0x3d800000u);
        record[2]=(float)((double)numerator/(double)aggregate);
        record[3]=(scaled_sum==0.0f)?0.0f:
                  ubig_stage_b_rt_ratio_map_mode(exponent_scale*scaled_sum+f32_bits(0x3e000000u),3);
    }

    if((((index+2u)&31u))==s->phase){
        const uint32_t next=(index+1u<UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH)?index+1u:0u;
        float values[UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH];
        int32_t shared_shift=32;

        for(uint32_t column=4u;column<12u;column++){
            s->records[next][column]=0.0f;
            for(uint32_t row=0;row<UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH;row++){
                values[row]=s->records[row][column];
                const int32_t lane_shift=stage_b_rt_spectral_shift(values[row]);
                if(lane_shift<shared_shift)shared_shift=lane_shift;
            }
            const uint32_t out=column-4u;
            s->segment_sum[out]=stage_b_rt_reduce32_exact(values,shared_shift);
            s->segment_shift[out]=(uint32_t)(shared_shift&0xff);
        }

        for(uint32_t segment=1u;segment<UBIG_STAGE_B_RT_FEATURE_SEGMENTS;segment++){
            s->records[next][11u+segment]=0.0f;
            int32_t shift=30;
            for(uint32_t row=0;row<UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH;row++){
                values[row]=s->records[row][12u+segment]-s->records[row][11u+segment];
                if(row==next)values[row]=0.0f;
                const int32_t lane_shift=stage_b_rt_spectral_shift(values[row]);
                if(lane_shift<shift)shift=lane_shift;
            }
            const uint32_t out=segment-1u;
            s->delta_sum[out]=stage_b_rt_reduce32_exact(values,shift);
            s->delta_shift[out]=(uint32_t)(shift&0xff);
        }
    }

    s->index=index+1u;
    if(s->index>=UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH)s->index=0u;
}

float ubig_stage_b_rt_feature_history_mean(const float records[UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH][UBIG_STAGE_B_RT_FEATURE_RECORD_VALUES])
{
    if(!records)return 0.0f;
    int32_t shift=stage_b_rt_spectral_shift(records[0][1]);
    for(uint32_t row=1u;row<UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH;row++){
        const int32_t row_shift=stage_b_rt_spectral_shift(records[row][1]);
        if(row_shift<shift)shift=row_shift;
    }
    const float scale=stage_b_rt_pow2_integer(shift-5);
    float mean=records[0][1]*scale;
    for(uint32_t row=1u;row<UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH;row++)
        mean=fmaf(records[row][1],scale,mean);
    mean*=f32_bits(0x3d000000u);
    mean*=stage_b_rt_pow2_integer(5-shift);
    if(mean==0.0f)mean+=f32_bits(0x2f800000u);
    return mean;
}


void ubig_stage_b_rt_feature_cadence_process(UbigStageBRtFeatureHistory *state,
                                             uint32_t cadence_step,
                                             float output[UBIG_STAGE_B_RT_FEATURE_CADENCE_OUTPUTS])
{
    if(!state||!output||cadence_step>=UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH||
       state->index!=state->phase)return;

    float scratch[32];
    float weighted[32];
    float weights[32];
    const float history_mean=ubig_stage_b_rt_feature_history_mean(state->records);
    const int32_t global_shift=stage_b_rt_spectral_shift(history_mean);
    const float aligned_mean=stage_b_rt_pow2_integer(global_shift)*history_mean;
    const float inverse=(float)(0.5/(double)aligned_mean);
    const uint32_t phase=state->phase;
    const uint32_t index=state->index;
    const uint32_t previous=phase?phase-1u:31u;

    /* Columns 4..11 use the reducers maintained by feature-history. */
    for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_FEATURE_SEGMENTS;lane++){
        const uint32_t column=4u+lane;
        for(uint32_t row=0u;row<32u;row++)scratch[row]=state->records[row][column];
        uint32_t shift;
        output[4u+lane]=stage_b_rt_cadence_mean(state->segment_sum[lane],
                                                state->segment_shift[lane],
                                                state->records[previous][column],&shift);
        output[24u+lane]=ubig_stage_b_rt_deviation32(output[4u+lane],scratch,shift);
    }

    /* Direct statistics for the first two record columns. Column 1 is the
     * aggregate-energy lane and is expressed relative to the 32-row mean. */
    for(uint32_t row=0u;row<32u;row++)scratch[row]=state->records[row][0];
    ubig_stage_b_rt_stat32(scratch,&output[0],&output[20]);
    for(uint32_t row=0u;row<32u;row++)scratch[row]=state->records[row][1];
    ubig_stage_b_rt_stat32(scratch,&output[1],&output[21]);
    const float aggregate_scale=stage_b_rt_pow2_integer(global_shift-4);
    output[1]=aggregate_scale*(output[1]*inverse);
    output[21]=aggregate_scale*(output[21]*inverse);

    /* The lower spectral bank weights the dimensionless segment fractions by
     * exponent-aligned aggregate energy, then circularizes at phase/index. */
    int32_t weight_shift=32;
    for(uint32_t row=0u;row<32u;row++){
        const int32_t lane_shift=stage_b_rt_spectral_shift(state->records[row][1]);
        if(lane_shift<weight_shift)weight_shift=lane_shift;
    }
    const float weight_scale=stage_b_rt_pow2_integer(weight_shift);
    for(uint32_t row=0u;row<32u;row++)weights[row]=state->records[row][1]*weight_scale;

    const float column_scale=stage_b_rt_pow2_integer(global_shift-8);
    for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_FEATURE_SEGMENTS;lane++){
        uint32_t out=0u;
        const uint32_t weighted_column=4u+lane;
        for(uint32_t row=phase;row<32u;row++)
            weighted[out++]=state->records[row][weighted_column]*weights[row];
        for(uint32_t row=0u;row<index;row++)
            weighted[out++]=state->records[row][weighted_column]*weights[row];

        const uint32_t column=12u+lane;
        for(uint32_t row=0u;row<32u;row++)scratch[row]=state->records[row][column];
        ubig_stage_b_rt_stat32(scratch,&output[12u+lane],&output[32u+lane]);
        output[12u+lane]=column_scale*(output[12u+lane]*inverse);
        output[32u+lane]=column_scale*(output[32u+lane]*inverse);
        ubig_stage_b_rt_spectrum32(weighted,&output[58u+16u*lane]);
    }

    /* Seven adjacent differences share their own upper-history reducers. */
    const float delta_mean_scale=stage_b_rt_pow2_integer(global_shift-3);
    const float delta_deviation_scale=stage_b_rt_pow2_integer(global_shift-8);
    for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_FEATURE_SEGMENTS-1u;lane++){
        const uint32_t column=12u+lane;
        for(uint32_t row=0u;row<32u;row++)
            scratch[row]=state->records[row][column+1u]-state->records[row][column];
        const float outgoing=state->records[previous][column+1u]-
                             state->records[previous][column];
        uint32_t shift;
        output[40u+lane]=stage_b_rt_cadence_mean(state->delta_sum[lane],
                                                 state->delta_shift[lane],outgoing,&shift);
        output[47u+lane]=ubig_stage_b_rt_deviation32(output[40u+lane],scratch,shift);
        output[40u+lane]=delta_mean_scale*(output[40u+lane]*inverse);
        output[47u+lane]=delta_deviation_scale*(output[47u+lane]*inverse);
    }

    /* Columns 2 and 10 feed the final two-row slope descriptor through the
     * same energy-weighted circular window used by the spectral bank. */
    float slope_row0[32],slope_row1[32];
    uint32_t out=0u;
    for(uint32_t row=phase;row<32u;row++){
        slope_row0[out]=state->records[row][2]*weights[row];
        slope_row1[out]=state->records[row][10]*weights[row];
        out++;
    }
    for(uint32_t row=0u;row<index;row++){
        slope_row0[out]=state->records[row][2]*weights[row];
        slope_row1[out]=state->records[row][10]*weights[row];
        out++;
    }
    UbigStageBRtSlope32 slope;
    ubig_stage_b_rt_slope32_prepare(slope_row0,slope_row1,&slope);
    ubig_stage_b_rt_slope32_features(&slope,&output[54]);

    for(uint32_t row=0u;row<32u;row++)scratch[row]=state->records[row][2];
    ubig_stage_b_rt_stat32(scratch,&output[2],&output[22]);
    for(uint32_t row=0u;row<32u;row++)scratch[row]=state->records[row][3];
    ubig_stage_b_rt_stat32(scratch,&output[3],&output[23]);

    uint32_t next=phase+cadence_step;
    if(next>=UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH)next-=UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH;
    state->phase=next;
}

void ubig_stage_b_rt_rank_metrics(float gain,
                                  const float input[32],
                                  float scratch[32],
                                  float *peak_metric,
                                  float *ratio_metric)
{
    if(!input||!scratch||!peak_metric||!ratio_metric)return;
    if(input!=scratch)memcpy(scratch,input,32u*sizeof(float));
    for(uint32_t pass=1u;pass<32u;pass++){
        for(uint32_t lane=1u;pass+lane-1u<32u;lane++){
            if(scratch[lane]<scratch[lane-1u]){
                const float swap=scratch[lane-1u];
                scratch[lane-1u]=scratch[lane];
                scratch[lane]=swap;
            }
        }
    }

    const int32_t shift=stage_b_rt_spectral_shift(scratch[31]);
    const float scale=stage_b_rt_pow2_integer(shift-5);
    float top=fmaf(scale,scratch[30],0.0f);
    const float highest=scale*scratch[31];
    top=highest+top;
    top*=0.5f;
    top*=32.0f;

    float shoulder=0.0f;
    for(uint32_t base=0u;base<=15u;base+=15u){
        shoulder=fmaf(scale,scratch[base],shoulder);
        shoulder=fmaf(scale,scratch[base+1u],shoulder);
        shoulder=fmaf(scale,scratch[base+2u],shoulder);
        for(uint32_t lane=base+3u;lane<=base+14u;lane++){
            const float product=scale*scratch[lane];
            shoulder=product+shoulder;
        }
    }
    shoulder*=f32_bits(0x3d088889u);
    shoulder*=32.0f;

    float peak=0.0f;
    if(gain!=0.0f){
        const float scaled=top*gain;
        peak=stage_b_rt_pow2_integer(1-shift)*scaled;
    }
    *peak_metric=peak;

    if(shoulder==0.0f){*ratio_metric=0.0f;return;}
    float activity=stage_b_rt_pow2_integer(-shift)*shoulder;
    activity*=gain;
    if(activity<f32_bits(0x38000000u)){*ratio_metric=peak;return;}

    const int32_t top_shift=stage_b_rt_spectral_shift(top);
    const int32_t shoulder_shift=stage_b_rt_spectral_shift(shoulder);
    const float top_norm=stage_b_rt_pow2_integer(top_shift-1)*top;
    const float shoulder_norm=stage_b_rt_pow2_integer(shoulder_shift)*shoulder;
    const float ratio=(float)((double)top_norm/(double)shoulder_norm);
    *ratio_metric=stage_b_rt_pow2_integer(shoulder_shift-top_shift-14)*ratio;
}

void ubig_stage_b_rt_rank_history_process(UbigStageBRtRankHistory *state,
                                          float control,
                                          float output[UBIG_STAGE_B_RT_RANK_OUTPUTS],
                                          float scratch[64])
{
    if(!state||!output||!scratch)return;
    if(control<=0.0f){
        memset(output,0,UBIG_STAGE_B_RT_RANK_OUTPUTS*sizeof(float));
    }else{
        const int32_t shift=stage_b_rt_spectral_shift(control);
        const float denominator=stage_b_rt_pow2_integer(shift)*control;
        const float gain=(float)(0.5/(double)denominator);
        const float row_scale=stage_b_rt_pow2_integer(shift-5);
        for(uint32_t column=0u;column<2u;column++){
            for(uint32_t row=0u;row<UBIG_STAGE_B_RT_RANK_HISTORY_ROWS;row++)
                scratch[row]=state->matrix[row][column]*row_scale;
            ubig_stage_b_rt_stat32(scratch,&output[column],&output[3u+column]);
            const float mean=output[column]*gain;
            output[column]=mean+mean;
            const float deviation=gain*output[3u+column];
            output[3u+column]=deviation+deviation;
            ubig_stage_b_rt_rank_metrics(gain,scratch,scratch+32u,
                                         &output[6u+column],&output[8u+column]);
        }
        for(uint32_t row=0u;row<UBIG_STAGE_B_RT_RANK_HISTORY_ROWS;row++)
            scratch[row]=state->matrix[row][2];
        ubig_stage_b_rt_stat32(scratch,&output[2],&output[5]);
        const float mean=output[2]*gain;
        output[2]=stage_b_rt_pow2_integer(shift-7)*mean;
        const float deviation=output[5]*gain;
        output[5]=stage_b_rt_pow2_integer(shift-7)*deviation;
    }
    uint32_t next=state->cursor.index+state->cursor.step;
    if(next>=32u)next-=32u;
    state->cursor.index=next;
}

void ubig_stage_b_rt_projection_history_process(UbigStageBRtProjectionHistory *s,
                                                const UbigStageBRtProjectionConfig *config,
                                                const UbigStageBRtSpectralExport *in)
{
    if(!s||!config||!config->projection_lut||!in||in->count==0u||
       in->count>UBIG_STAGE_B_RT_SPECTRAL_BINS)return;

    float measurements[UBIG_STAGE_B_RT_PROJECTION_MEASUREMENTS];
    for(uint32_t band=0;band<UBIG_STAGE_B_RT_PROJECTION_MEASUREMENTS;band++){
        const UbigStageBRtProjectionBand *b=&config->bands[band];
        if(!b->weights||b->count==0u||b->start>=in->count||b->count>in->count-b->start)return;
        float sum=in->bins[b->start]*b->weights[0];
        sum*=0.25f;
        for(uint32_t lane=1;lane<b->count;lane++){
            const float product=in->bins[b->start+lane]*b->weights[lane];
            sum=fmaf(product,0.25f,sum);
        }
        const float mapped=stage_b_rt_pow2_integer(-in->exponent)*sum+f32_bits(0x3627c5acu);
        measurements[band]=ubig_stage_b_rt_ratio_map_mode(mapped,2);
    }

    const uint32_t index=s->index;
    float *record=s->records[index];
    for(uint32_t output=1;output<=UBIG_STAGE_B_RT_PROJECTION_VALUES;output++){
        float sum=0.0f;
        for(uint32_t band=0;band<UBIG_STAGE_B_RT_PROJECTION_MEASUREMENTS;band++){
            const uint32_t lookup=((2u*band+1u)*output)%UBIG_STAGE_B_RT_PROJECTION_LUT;
            sum=fmaf(config->projection_lut[lookup],measurements[band],sum);
        }
        record[output-1u]=sum;
    }

    if((((index+2u)&31u))==s->phase){
        const uint32_t next=(index+1u<UBIG_STAGE_B_RT_PROJECTION_HISTORY_DEPTH)?index+1u:0u;
        float values[UBIG_STAGE_B_RT_PROJECTION_HISTORY_DEPTH];
        int32_t shared_shift=32;
        for(uint32_t column=0;column<UBIG_STAGE_B_RT_PROJECTION_VALUES;column++){
            s->records[next][column]=0.0f;
            for(uint32_t row=0;row<UBIG_STAGE_B_RT_PROJECTION_HISTORY_DEPTH;row++){
                values[row]=s->records[row][column];
                const int32_t lane_shift=stage_b_rt_spectral_shift(values[row]);
                if(lane_shift<shared_shift)shared_shift=lane_shift;
            }
            s->sum[column]=stage_b_rt_reduce32_exact(values,shared_shift);
            s->shift[column]=(uint32_t)shared_shift;
        }

        for(uint32_t column=1;column<UBIG_STAGE_B_RT_PROJECTION_VALUES;column++){
            s->records[next][column-1u]=0.0f;
            int32_t shift=32;
            for(uint32_t row=0;row<UBIG_STAGE_B_RT_PROJECTION_HISTORY_DEPTH;row++){
                const float current=s->records[row][column]*0.5f;
                values[row]=fmaf(-s->records[row][column-1u],0.5f,current);
                const int32_t lane_shift=stage_b_rt_spectral_shift(values[row]);
                if(lane_shift<shift)shift=lane_shift;
            }
            values[next]=0.0f;
            const uint32_t out=column-1u;
            s->delta_sum[out]=stage_b_rt_reduce32_exact(values,shift);
            s->delta_shift[out]=(uint32_t)shift;
        }
    }

    s->index=index+1u;
    if(s->index>=UBIG_STAGE_B_RT_PROJECTION_HISTORY_DEPTH)s->index=0u;
}

static float stage_b_rt_control_score(const float *features,
                                      const UbigStageBRtControlDescriptor *descriptor)
{
    float score=0.0f;
    for(uint32_t i=0u;i<descriptor->term_count;i++){
        const UbigStageBRtControlTerm *term=&descriptor->terms[i];
        const uint32_t exponent_field=(uint32_t)term->exponent+127u;
        const float exponent_scale=f32_bits(exponent_field<<23);
        const float lower=exponent_scale*f32_bits(0xb7000000u);
        const float upper=exponent_scale*f32_bits(0x37000000u);
        float value=(features[term->feature_index]-term->center)*term->scale;
        if(value<lower)value=lower;
        if(upper<value)value=upper;
        const float output_scale=f32_bits((138u-(uint32_t)term->exponent)<<23);
        score=fmaf(term->weight,value*output_scale,score);
    }
    return score;
}

float ubig_stage_b_rt_control_transfer(float score,float gain,float bias)
{
    float value=score*gain;
    value=value+value;
    value=fmaf(bias,f32_bits(0x3a800000u),value);
    if(0.125f<value)value=1.0f;
    else if(value<-0.125f)value=-1.0f;
    else value*=8.0f;

    const float scaled=value*f32_bits(0x3f38aa3bu);
    const float quantized_input=(scaled*f32_bits(0x3a800000u))*f32_bits(0x47000000u);
    int32_t quantized=(int32_t)lrintf(quantized_input);
    if(32767<quantized)quantized=32767;

    int32_t exponent;
    if(quantized<=-21)exponent=-21;
    else exponent=quantized+1;
    if(21<quantized)exponent=21;

    const float q1024=(float)(quantized*1024);
    float residual=fmaf(-q1024,f32_bits(0x38000000u),scaled);
    residual*=f32_bits(0x42000000u);
    const float y=residual*f32_bits(0x3f317218u);
    const float y2=y*y;
    float poly=fmaf(y2,0.5f,y);
    float power=y2*y;
    poly=fmaf(power,f32_bits(0x3e2aaaabu),poly);
    power*=y;
    poly=fmaf(power,f32_bits(0x3d2aaaabu),poly);
    power*=y;
    poly=fmaf(power,f32_bits(0x3c088889u),poly);
    power*=y;
    poly=fmaf(power,f32_bits(0x3ab60b61u),poly);
    const float exp_half=fmaf(poly,0.5f,0.5f);

    if(exponent>=0){
        const float scale=stage_b_rt_pow2_integer(-1-exponent);
        const float denominator=fmaf(exp_half,0.5f,scale);
        return (float)((double)scale/(double)denominator);
    }
    const float scale=stage_b_rt_pow2_integer(exponent-1);
    const float denominator=fmaf(scale,exp_half,0.5f);
    return (float)(0.5/(double)denominator);
}

void ubig_stage_b_rt_control_score_process(const float *features,
                                           const UbigStageBRtControlDescriptor *descriptor,
                                           float output[2])
{
    if(!features||!descriptor||!output||
       (descriptor->term_count!=0u&&!descriptor->terms))return;
    const float score=stage_b_rt_control_score(features,descriptor);
    output[1]=score;
    output[0]=ubig_stage_b_rt_control_transfer(score,descriptor->transfer_gain,
                                               descriptor->transfer_bias);
}

void ubig_stage_b_rt_control_select_process(const float *features,
                                            const UbigStageBRtControlGroup groups[UBIG_STAGE_B_RT_CONTROL_GROUPS],
                                            uint32_t result_words[UBIG_STAGE_B_RT_CONTROL_RESULT_WORDS])
{
    if(!features||!groups||!result_words)return;
    if(features[0]<f32_bits(0xbacccccdu)){
        memset(result_words+1u,0,(UBIG_STAGE_B_RT_CONTROL_RESULT_WORDS-1u)*sizeof(uint32_t));
        result_words[0]=4u;
        return;
    }

    uint32_t winner=0u;
    float best=-1.0f;
    for(uint32_t i=0u;i<UBIG_STAGE_B_RT_CONTROL_GROUPS;i++){
        const UbigStageBRtControlGroup *group=&groups[i];
        const UbigStageBRtControlDescriptor *descriptor=group->descriptor;
        if(!descriptor||group->output_index>=UBIG_STAGE_B_RT_CONTROL_SLOTS||
           (descriptor->term_count!=0u&&!descriptor->terms))return;
        const float score=stage_b_rt_control_score(features,descriptor);
        const uint32_t base=2u*group->output_index+1u;
        float transfer=ubig_stage_b_rt_control_transfer(score,descriptor->transfer_gain,
                                                        descriptor->transfer_bias);
        memcpy(result_words+base,&transfer,sizeof transfer);
        memcpy(result_words+base+1u,&score,sizeof score);
        if(best<score){best=score;winner=group->output_index;}
    }
    result_words[0]=winner;
}

void ubig_stage_b_rt_control_cadence_process(UbigStageBRtControlCadence *state,
                                             const UbigStageBRtControlCadenceConfig *config,
                                             float features[UBIG_STAGE_B_RT_UNIVERSAL_FEATURES])
{
    if(!state||!config||!config->secondary||!features)return;
    state->updated=0u;
    state->counter++;
    if(state->counter==state->period){
        state->counter=0u;
        state->cycle++;
    }

    if(state->cycle!=state->target&&state->armed==1u){
        state->armed=0u;
        for(uint32_t i=0u;i<UBIG_STAGE_B_RT_UNIVERSAL_FEATURES;i++)features[i]*=0.5f;
        ubig_stage_b_rt_control_select_process(features,config->groups,state->primary_result);

        const uint32_t winner=state->primary_result[0];
        if(winner==1u||winner==2u){
            float extended[UBIG_STAGE_B_RT_EXTENDED_FEATURES]={0};
            memcpy(extended,features,UBIG_STAGE_B_RT_UNIVERSAL_FEATURES*sizeof(float));
            const uint32_t source_words[4]={3u,5u,13u,11u};
            for(uint32_t i=0u;i<4u;i++){
                float value;
                memcpy(&value,&state->primary_result[source_words[i]],sizeof value);
                extended[292u+i]=value*0.5f;
            }
            ubig_stage_b_rt_control_score_process(extended,config->secondary,
                                                  state->secondary_result);
        }else{
            state->secondary_result[0]=0.5f;
            state->secondary_result[1]=0.0f;
        }
        state->updated=1u;
    }

    if(state->cycle==state->target){
        state->armed=1u;
        state->cycle=state->reset;
    }
}

static float stage_b_rt_control_asymmetric_smooth(float previous,float current,float keep)
{
    if(previous<current)return current;
    return fmaf(current,1.0f-keep,previous*keep);
}

void ubig_stage_b_rt_control_aggregate_process(UbigStageBRtControlAggregateState *state,
                                               const UbigStageBRtControlAggregateItem *items,
                                               uint32_t item_count,
                                               float output[UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS])
{
    if(!state||!output||(item_count!=0u&&!items))return;
    if(state->enabled==0u){
        output[0]=1.0f;
        output[1]=1.0f;
        output[2]=0.0f;
        output[3]=0.0f;
        output[4]=f32_bits(0x3ed154a8u);
        return;
    }

    const float keep=state->smoothing_keep;
    const float one_minus_keep=1.0f-keep;
    for(uint32_t i=0u;i<item_count;i++){
        const UbigStageBRtControlAggregateItem *item=&items[i];
        state->hysteresis.response_a=stage_b_rt_control_asymmetric_smooth(
            state->hysteresis.response_a,item->slot1_transfer,keep);
        state->slot2_state=stage_b_rt_control_asymmetric_smooth(
            state->slot2_state,item->slot2_transfer,keep);
        state->hysteresis.response_b=stage_b_rt_control_asymmetric_smooth(
            state->hysteresis.response_b,item->slot5_transfer,keep);
        state->hysteresis.response_c=stage_b_rt_control_asymmetric_smooth(
            state->hysteresis.response_c,item->slot6_transfer,keep);
        if(item->winner-5u>1u)
            state->hysteresis.input=fmaf(item->secondary_transfer,one_minus_keep,
                                        state->hysteresis.input*keep);
    }

    float activity=fmaf(-state->hysteresis.response_a,state->hysteresis.response_a,1.0f);
    activity*=state->slot2_state;
    const float activity_alpha=(state->activity_state>=activity)?
                               state->activity_alpha_low:state->activity_alpha_high;
    state->activity_state=fmaf(state->activity_state,activity_alpha,
                               (1.0f-activity_alpha)*activity);
    output[1]=state->activity_state;

    output[0]=ubig_stage_b_rt_hysteresis_process(&state->hysteresis);
    output[2]=state->hysteresis.response_a;
    output[3]=state->slot2_state;

    float peak=state->hysteresis.response_a;
    if(peak<state->hysteresis.response_c)peak=state->hysteresis.response_c;
    if(peak<state->hysteresis.response_b)peak=state->hysteresis.response_b;
    float drive=fmaf(-peak,peak,1.0f);
    drive*=state->slot2_state;
    float final=(1.0f-state->final_blend)*drive;
    final=fmaf(state->final_blend,state->final_state,final);
    state->final_state=final;

    if(final<=f32_bits(0x3e666666u)){
        output[4]=1.0f-final*f32_bits(0x3ee38e37u);
    }else if(final<f32_bits(0x3f066666u)){
        const float shaped=fmaf(final,f32_bits(0x3f2aaaabu),-0.125f);
        output[4]=1.0f-shaped*4.0f;
    }else if(final<f32_bits(0x3f3fbe77u)){
        const float shaped=fmaf(final,f32_bits(0x3ee38e37u),f32_bits(0x3f2aaaabu));
        output[4]=1.0f-shaped;
    }else{
        output[4]=0.0f;
    }
}

uint32_t ubig_stage_b_rt_scheduler_step(UbigStageBRtSchedulerClock *clock)
{
    if(!clock)return 0u;
    uint32_t actions=0u;
    clock->upper_count++;
    if(clock->upper_count==clock->upper_period){
        actions|=UBIG_STAGE_B_RT_SCHED_UPPER;
        clock->upper_count=0u;
        clock->lower_count++;
    }
    if(clock->lower_count==clock->lower_period||clock->lower_toggle==1u){
        if(clock->lower_toggle==0u){
            actions|=UBIG_STAGE_B_RT_SCHED_LOWER_A;
            clock->lower_toggle=1u;
        }else{
            actions|=UBIG_STAGE_B_RT_SCHED_LOWER_B;
            clock->lower_toggle=0u;
        }
        clock->lower_count=clock->lower_reset;
    }
    return actions;
}

static float stage_b_rt_deep_reciprocal(uint32_t count)
{
    if(count==7u)return f32_bits(0x3e124924u);
    return 1.0f/(float)count;
}

void ubig_stage_b_rt_pair_bounds_process(float control,float subtract,
                                         float base_offset,float modulation_scale,
                                         UbigStageBRtPairBoundsState *state,
                                         const float *lower_source,
                                         const float *upper_source,
                                         float *lower_output,float *upper_output,
                                         const float *modulation)
{
    if(!state||!state->config||!lower_output||!upper_output||
       state->active_width>UBIG_STAGE_B_RT_MAX_BANDS)return;
    const float threshold=f32_bits(0xbbfc0fc1u);
    const float pivot=f32_bits(0xbcbd0bd1u);
    float baseline=state->baseline;
    if(threshold<=control){
        float mix=(baseline-pivot)*64.0f;
        if(mix<0.0f)mix=0.0f;
        if(1.0f<mix)mix=1.0f;
        float drive=mix*f32_bits(0x3d8dc55cu);
        drive=fmaf(-(1.0f-mix),f32_bits(0x3cb6be9fu),drive);
        drive*=state->config->blend_drive;
        baseline=fmaf(state->config->blend_keep,baseline,drive);
    }else{
        const float delta=control-threshold;
        if(baseline<=pivot)baseline+=state->config->below_pivot_slope*delta;
        else baseline+=state->config->above_pivot_slope*delta;
    }
    state->baseline=baseline;
    float upper=baseline*0.5f;
    if(0.0f<subtract)upper-=subtract*0.5f;
    const float lower=upper-f32_bits(0x3d3d0bd1u);
    if(!lower_source||!upper_source){
        for(uint32_t lane=0u;lane<state->active_width;lane++){
            upper_output[lane]=upper;
            lower_output[lane]=lower;
        }
        return;
    }
    if(!modulation)return;
    const float base=base_offset*0.5f-subtract*0.5f;
    const float scale=modulation_scale*0.5f;
    for(uint32_t lane=0u;lane<state->active_width;lane++){
        const float center=fmaf(modulation[lane],scale,base);
        const float lane_upper=fmaf(upper_source[lane],0.5f,center);
        if(upper<=lane_upper){
            upper_output[lane]=upper;
            lower_output[lane]=lower;
        }else{
            upper_output[lane]=lane_upper;
            float lane_lower=fmaf(lower_source[lane],0.5f,center);
            const float cap=lane_upper-f32_bits(0x3d3d0bd1u);
            if(cap<lane_lower)lane_lower=cap;
            lower_output[lane]=lane_lower;
        }
    }
}

void ubig_stage_b_rt_residual_balance_process(float alpha,const int32_t *status,
                                              const float *input,uint32_t count,
                                              float *primary,float *secondary)
{
    if(!status||!input||!primary||!secondary||count==0u||
       count>UBIG_STAGE_B_RT_MAX_BANDS)return;
    float delta[UBIG_STAGE_B_RT_MAX_BANDS];
    float out_primary[UBIG_STAGE_B_RT_MAX_BANDS];
    float out_secondary[UBIG_STAGE_B_RT_MAX_BANDS];
    float minimum=1.0f;
    for(uint32_t lane=0u;lane<count;lane++){
        delta[lane]=input[lane]*0.5f-primary[lane];
        if(status[lane]==0&&delta[lane]<minimum)minimum=delta[lane];
    }
    float sum=0.0f;
    float maximum=0.0f;
    uint32_t active=0u;
    for(uint32_t lane=0u;lane<count;lane++){
        if(status[lane]!=0)continue;
        const float difference=delta[lane]-minimum;
        if(f32_bits(0x39c9a634u)<difference){
            sum=fmaf(difference,f32_bits(0x3d000000u),sum);
            if(maximum<difference)maximum=difference;
        }
        active++;
    }
    if(active==0u)return;
    float aggregate=stage_b_rt_deep_reciprocal(active)*sum;
    aggregate*=32.0f;
    aggregate*=f32_bits(0x3edf5123u);
    aggregate=fmaf(maximum,f32_bits(0x3f10576eu),aggregate);
    aggregate+=minimum;
    const float keep=1.0f-alpha;
    for(uint32_t lane=0u;lane<count;lane++){
        const float old=primary[lane];
        float target=old;
        if(status[lane]==0)target=(delta[lane]-aggregate)+old;
        const float low=(old<target)?old:target;
        float secondary_target=secondary[lane]+(low-old);
        const float cap=low-f32_bits(0x3d3d0bd1u);
        if(cap<secondary_target)secondary_target=cap;
        out_primary[lane]=fmaf(old,keep,low*alpha);
        out_secondary[lane]=fmaf(secondary[lane],keep,secondary_target*alpha);
    }
    memcpy(primary,out_primary,(size_t)count*sizeof(float));
    memcpy(secondary,out_secondary,(size_t)count*sizeof(float));
}

void ubig_stage_b_rt_residual_mean_process(float gain,float bias,
                                           UbigStageBRtResidualMeanState *state,
                                           const float *primary_envelope,
                                           const float *lower_bound,
                                           const float *upper_bound,
                                           const int32_t *status,
                                           float *base_output,float *residual_output)
{
    if(!state||!state->config||!primary_envelope||!lower_bound||!upper_bound||
       !status||!base_output||!residual_output||
       state->active_width>UBIG_STAGE_B_RT_MAX_BANDS)return;
    const float old_scalar=state->scalar;
    for(uint32_t lane=0u;lane<state->active_width;lane++){
        const float half=fmaf(primary_envelope[lane],0.5f,old_scalar);
        const float target=fmaf(bias,0.5f,half);
        base_output[lane]=target-old_scalar;
        float residual=0.0f;
        if(upper_bound[lane]<target){
            const float span=upper_bound[lane]-lower_bound[lane];
            const float test=(span-lower_bound[lane])+target;
            if(test<0.0f){
                const float denominator=span*4.0f;
                const float inverse=(float)(1.0/(double)denominator);
                const float difference=target-upper_bound[lane];
                residual=inverse*(difference*difference);
            }else residual=lower_bound[lane]-target;
        }
        residual_output[lane]=residual;
    }
    float sum=0.0f;
    uint32_t active=0u;
    for(uint32_t lane=0u;lane<state->active_width;lane++){
        if(status[lane]!=0)continue;
        sum=fmaf(residual_output[lane],f32_bits(0x3d000000u),sum);
        active++;
    }
    float next=0.0f;
    if(active!=0u){
        float mean=stage_b_rt_deep_reciprocal(active)*sum;
        mean*=32.0f;
        mean*=gain;
        if(mean<old_scalar){
            const float inject=state->config->down_inject*mean;
            next=fmaf(state->config->down_keep,old_scalar,inject);
        }else{
            const float inject=state->config->up_inject*mean;
            next=fmaf(state->config->up_keep,old_scalar,inject);
        }
    }
    state->scalar=next;
    for(uint32_t lane=0u;lane<state->active_width;lane++)residual_output[lane]+=old_scalar;
}


void ubig_stage_b_rt_telemetry_smooth(UbigStageBRtTelemetrySmoothState *state,
                                      const int32_t *code,
                                      const float *input,
                                      uint32_t count)
{
    if(!state||!state->coeff||!code||!input||count>UBIG_STAGE_B_RT_MAX_BANDS)return;
    for(uint32_t i=0u;i<count;i++){
        int32_t x=code[i];
        if(x<-192)x=-192;
        if(x>576)x=576;
        state->code[i]=x;
        const float old=state->value[i];
        const float current=input[i];
        float alpha,minimum;
        if(current>=old){alpha=state->coeff[2];minimum=state->coeff[0]+current;}
        else{alpha=state->coeff[3];minimum=state->coeff[1]+old;}
        float next=fmaf(current,alpha,(1.0f-alpha)*old);
        if(next<minimum)next=minimum;
        state->value[i]=next;
        int32_t scaled=(int32_t)floorf(next*2080.0f);
        if(scaled<-192)scaled=-192;
        if(scaled>576)scaled=576;
        state->scaled[i]=scaled;
    }
}

void ubig_stage_b_rt_late_pipeline_process(
    float row_offset,
    UbigStageBRtLatePipelineState *state,
    const UbigStageBRtLatePipelineConfig *config,
    UbigStageBRtComplexGroups *groups,
    UbigStageBRtBandRows *analysis_rows,
    UbigStageBRtBandRows *output_rows,
    float linked_accumulator[UBIG_STAGE_B_RT_SP11_BANDS],
    int32_t *base_meter,
    int32_t *output_meter,
    float late_rows[UBIG_STAGE_B_RT_LATE_ROWS][UBIG_STAGE_B_RT_LATE_ROW_FLOATS])
{
    if(!state||!config||!groups||!analysis_rows||!output_rows||!linked_accumulator||!late_rows||
       !config->band_ends||!config->deep_controls||!config->late_config||
       !groups->groups||groups->group_count!=UBIG_STAGE_B_RT_SP11_ROWS||
       groups->vectors_per_group!=UBIG_STAGE_B_RT_TARGET_PLANES||
       analysis_rows->row_count!=UBIG_STAGE_B_RT_SP11_ROWS||
       output_rows->row_count!=UBIG_STAGE_B_RT_SP11_ROWS)return;

    const int32_t map[UBIG_STAGE_B_RT_SP11_ROWS]={0,1};
    int32_t *telemetry_rows[UBIG_STAGE_B_RT_SP11_ROWS];
    for(uint32_t row=0u;row<UBIG_STAGE_B_RT_SP11_ROWS;row++)
        telemetry_rows[row]=(int32_t*)output_rows->rows[row];
    UbigStageBRtTelemetryRows telemetry={telemetry_rows};
    ubig_stage_b_rt_band_log_process(0.0f,config->analysis_offset,groups,0,
                                     config->band_ends,map,analysis_rows,&telemetry);
    ubig_stage_b_rt_linked_row_accumulate(analysis_rows,linked_accumulator);

    ubig_stage_b_rt_deep_controller_process(state->late.output,&state->deep,
                                             config->deep_lower_source,config->deep_upper_source,
                                             config->deep_status,config->deep_controls,
                                             analysis_rows,output_rows,base_meter,output_meter);

    UbigStageBRtTargetObject objects[UBIG_STAGE_B_RT_SP11_ROWS];
    for(uint32_t row=0u;row<UBIG_STAGE_B_RT_SP11_ROWS;row++)
        for(uint32_t plane=0u;plane<UBIG_STAGE_B_RT_TARGET_PLANES;plane++)
            objects[row].plane[plane]=groups->groups[row][plane];
    UbigStageBRtTargetSet targets={UBIG_STAGE_B_RT_SP11_ROWS,UBIG_STAGE_B_RT_SP11_BINS,objects};
    const uint32_t object_to_row[UBIG_STAGE_B_RT_SP11_ROWS]={0u,1u};
    const float ceiling_base=f32_bits(0xbe95e7aau);
    ubig_stage_b_rt_output_shape(row_offset,ceiling_base-row_offset,analysis_rows,output_rows,
                                 object_to_row,config->band_ends,&targets);

    float *late_analysis[UBIG_STAGE_B_RT_LATE_ROWS][UBIG_STAGE_B_RT_LATE_BLOCKS];
    for(uint32_t row=0u;row<UBIG_STAGE_B_RT_LATE_ROWS;row++)
        for(uint32_t block=0u;block<UBIG_STAGE_B_RT_LATE_BLOCKS;block++)
            late_analysis[row][block]=groups->groups[row][block];
    ubig_stage_b_rt_late_controller_process(&state->late,config->late_config,late_analysis,late_rows);
}

void ubig_stage_b_rt_linked_row_accumulate(const UbigStageBRtBandRows *rows,float *accumulator)
{
    if(!rows||!rows->rows||!accumulator)return;
    const float threshold=f32_bits(0x3e1d89d9u);
    const float c0=f32_bits(0x3f229946u);
    const float c1=f32_bits(0x3e722614u);
    const float c2=f32_bits(0x3cfbf0a8u);
    const float c3=f32_bits(0x3abdb181u);
    for(uint32_t band=0u;band<rows->band_count;band++){
        float linked=-1.0f;
        for(uint32_t row=0u;row<rows->row_count;row++){
            const float value=rows->rows[row][band];
            const float maximum=value>linked?value:linked;
            const float delta=linked-value;
            float distance=-delta;
            if(!(distance>delta))distance=delta;
            if(distance<threshold){
                float p=fmaf(-distance,c0,c1);
                p=fmaf(p,distance,-c2);
                p=fmaf(p,distance,c3);
                linked=fmaf(p,16.0f,maximum);
                if(linked<-1.0f)linked=-1.0f;
                if(linked>1.0f)linked=1.0f;
            }else linked=maximum;
        }
        accumulator[band]+=linked;
    }
}

void ubig_stage_b_rt_deep_controller_reset(UbigStageBRtDeepControllerState *state,
                                           uint32_t row_count)
{
    if(!state||!state->config||state->config->active_width>UBIG_STAGE_B_RT_MAX_BANDS)return;
    const uint32_t count=state->config->active_width;
    state->row_count_cache=row_count;

    state->dual.config=&state->config->dual_envelope;
    state->dual.active_width=count;
    for(uint32_t lane=0u;lane<count;lane++){
        state->dual.primary[lane]=-1.0f;
        state->dual.secondary[lane]=-1.0f;
    }

    state->envelope.config=&state->config->envelope;
    state->envelope.active_width=count;
    for(uint32_t lane=0u;lane<UBIG_STAGE_B_RT_MAX_BANDS;lane++){
        state->envelope.status[lane]=0u;
        state->envelope.envelope[lane]=-1.0f;
        state->envelope.lane_activity[lane]=0.0f;
        state->output[lane]=0.0f;
    }
    state->envelope.scalar_envelope=-1.0f;
    state->envelope.activity_state=0.0f;

    state->pair_bounds.config=&state->config->pair_bounds;
    state->pair_bounds.active_width=count;
    state->pair_bounds.baseline=f32_bits(0x3d8dc55cu);

    state->residual_mean.config=&state->config->residual_mean;
    state->residual_mean.active_width=count;
    state->residual_mean.scalar=0.0f;

    for(uint32_t lane=0u;lane<count;lane++)state->intermediate[lane]=f32_bits(0x3b7c0fc1u);
}

void ubig_stage_b_rt_deep_controller_process(float control,
                                             UbigStageBRtDeepControllerState *state,
                                             const float *lower_source,
                                             const float *upper_source,
                                             const int32_t *status,
                                             const UbigStageBRtDeepControllerControls *controls,
                                             UbigStageBRtBandRows *analysis_rows,
                                             UbigStageBRtBandRows *output_rows,
                                             int32_t *base_meter,
                                             int32_t *output_meter)
{
    if(!state||!state->config||!controls||!analysis_rows||!output_rows||
       !analysis_rows->rows||!output_rows->rows||
       state->config->active_width>UBIG_STAGE_B_RT_MAX_BANDS||
       output_rows->row_count<analysis_rows->row_count)return;
    const uint32_t count=state->config->active_width;
    if(state->mode==1u&&!status)return;
    if(state->row_count_cache!=analysis_rows->row_count||
       state->dual.config!=&state->config->dual_envelope||
       state->envelope.config!=&state->config->envelope||
       state->pair_bounds.config!=&state->config->pair_bounds||
       state->residual_mean.config!=&state->config->residual_mean)
        ubig_stage_b_rt_deep_controller_reset(state,analysis_rows->row_count);

    int32_t zero_status[UBIG_STAGE_B_RT_MAX_BANDS]={0};
    uint32_t dual_status[UBIG_STAGE_B_RT_MAX_BANDS];
    float lower[UBIG_STAGE_B_RT_MAX_BANDS];
    float upper[UBIG_STAGE_B_RT_MAX_BANDS];
    float base[UBIG_STAGE_B_RT_MAX_BANDS];
    const int32_t *active_status=(state->mode==1u)?status:zero_status;

    ubig_stage_b_rt_dual_envelope_process(controls->dual_offset,&state->dual,
                                          analysis_rows,dual_status);
    ubig_stage_b_rt_envelope_activity_process(&state->envelope,analysis_rows,
                                              state->output);
    ubig_stage_b_rt_pair_bounds_process(control,controls->subtract,
                                        controls->base_offset,controls->modulation_scale,
                                        &state->pair_bounds,
                                        state->mode==1u?lower_source:NULL,
                                        state->mode==1u?upper_source:NULL,
                                        lower,upper,state->envelope.lane_activity);
    ubig_stage_b_rt_residual_balance_process(state->mode==1u?controls->gain:1.0f,
                                             active_status,state->dual.secondary,count,
                                             upper,lower);
    ubig_stage_b_rt_residual_mean_process(controls->gain,controls->bias,
                                          &state->residual_mean,state->dual.primary,
                                          upper,lower,active_status,base,state->output);

    for(uint32_t lane=0u;lane<count;lane++){
        float current=state->output[lane];
        const float prior=state->intermediate[lane];
        if((current<prior&&dual_status[lane]==0u)||
           (prior<=current&&dual_status[lane]!=0u))
            current=fmaf(prior,state->config->post_new,current*state->config->post_old);
        state->intermediate[lane]=current;
    }
    ubig_stage_b_rt_neighbor_smooth(count,active_status,state->intermediate,state->output);

    for(uint32_t row=0u;row<state->row_count_cache;row++){
        float *analysis=analysis_rows->rows[row];
        float *output=output_rows->rows[row];
        if(!analysis||!output)return;
        for(uint32_t lane=0u;lane<count;lane++){
            analysis[lane]+=state->output[lane];
            output[lane]+=state->output[lane];
        }
    }
    for(uint32_t lane=0u;lane<count;lane++){
        if(output_meter)output_meter[lane]=(int32_t)floorf(state->output[lane]*2080.0f);
        if(base_meter)base_meter[lane]=(int32_t)floorf(base[lane]*4160.0f);
    }
}

static float stage_b_rt_dual_curve_primary(const UbigStageBRtDualEnvelopeConfig *config,
                                           float target,float old)
{
    const float half_old=old*0.5f;
    const float delta=target*0.5f-half_old;
    float value;
    if(delta<0.0f){
        float term=config->primary_negative_slope*delta;
        if(term<config->primary_lower_limit)term=config->primary_lower_limit;
        value=term+half_old;
    }else if(config->primary_quadratic_limit<delta){
        value=config->primary_linear_offset+delta+half_old;
    }else{
        float x=delta*4.0f;
        x=x*x;
        value=fmaf(x,config->primary_quadratic_scale,half_old);
    }
    return value+value;
}

static float stage_b_rt_dual_curve_secondary(const UbigStageBRtDualEnvelopeConfig *config,
                                             float target,float old)
{
    const float half_old=old*0.5f;
    const float delta=target*0.5f-half_old;
    float value;
    if(delta<0.0f){
        float term=config->secondary_negative_slope*delta;
        if(term<config->secondary_lower_limit)term=config->secondary_lower_limit;
        value=term+half_old;
    }else if(config->secondary_cubic_limit<delta){
        value=config->secondary_linear_offset+delta+half_old;
    }else{
        const float twice=delta+delta;
        const float square=twice*twice;
        const float cube=square*twice;
        const float scaled=cube*config->secondary_cubic_scale;
        value=fmaf(scaled,4.0f,half_old);
    }
    return value+value;
}

void ubig_stage_b_rt_dual_envelope_process(float offset,
                                           UbigStageBRtDualEnvelopeState *state,
                                           const UbigStageBRtBandRows *rows,
                                           uint32_t status[UBIG_STAGE_B_RT_MAX_BANDS])
{
    if(!state||!state->config||!rows||!rows->rows||!status||
       state->active_width>UBIG_STAGE_B_RT_MAX_BANDS)return;
    for(uint32_t lane=0u;lane<state->active_width;lane++){
        float maximum=-1.0f;
        for(uint32_t row=0u;row<rows->row_count;row++){
            const float value=rows->rows[row][lane];
            if(maximum<value)maximum=value;
        }
        float target=maximum+offset;
        if(target<-1.0f)target=-1.0f;
        if(1.0f<target)target=1.0f;
        const float old=state->primary[lane];
        status[lane]=(uint32_t)(old<target);
        state->primary[lane]=stage_b_rt_dual_curve_primary(state->config,target,old);
        state->secondary[lane]=stage_b_rt_dual_curve_secondary(state->config,target,
                                                                state->secondary[lane]);
    }
}

void ubig_stage_b_rt_neighbor_smooth(uint32_t count,const int32_t *status,
                                     const float *input,float *output)
{
    if(!status||!input||!output||count>UBIG_STAGE_B_RT_MAX_BANDS)return;
    const float third=f32_bits(0x3eaa7efau);
    const float center=f32_bits(0x3eab020cu);
    const float two_thirds=f32_bits(0x3f2ac083u);
    for(uint32_t lane=0u;lane<count;lane++){
        const int left_blocked=(lane==0u)||status[lane-1u]!=0;
        const int center_blocked=status[lane]!=0;
        const int right_blocked=(lane+1u==count)||status[lane+1u]!=0;
        const float left=(lane==0u)?0.0f:input[lane-1u];
        const float current=input[lane];
        const float right=(lane+1u==count)?0.0f:input[lane+1u];
        float value;
        if(center_blocked)value=current;
        else if(!left_blocked&&!right_blocked){
            float z=left*third;
            z=fmaf(current,center,z);
            value=fmaf(right,third,z);
        }else if(!left_blocked&&right_blocked){
            const float z=left*third;
            value=fmaf(current,two_thirds,z);
        }else if(left_blocked&&!right_blocked){
            const float z=current*two_thirds;
            value=fmaf(right,third,z);
        }else value=current;
        if(current<value)value=current;
        output[lane]=value+value;
    }
}

static float stage_b_rt_envelope_curve(const UbigStageBRtEnvelopeConfig *config,
                                      float target,float old)
{
    const float half_old=old*0.5f;
    const float delta=target*0.5f-half_old;
    float value;
    if(delta<0.0f){
        float term=config->negative_slope*delta;
        if(term<config->lower_limit)term=config->lower_limit;
        value=term+half_old;
    }else if(config->quadratic_limit<delta){
        value=config->linear_offset+delta+half_old;
    }else{
        float x=delta*4.0f;
        x=x*x;
        value=fmaf(x,config->quadratic_scale,half_old);
    }
    return value+value;
}

int ubig_stage_b_rt_envelope_track(UbigStageBRtEnvelopeState *state,
                                   const UbigStageBRtBandRows *rows,
                                   const float *lane_offset)
{
    if(!state||!state->config||!rows||!rows->rows||!lane_offset||
       state->active_width>UBIG_STAGE_B_RT_MAX_BANDS)return 0;
    const float smooth_limit=f32_bits(0x3e1d89d9u);
    const float poly_a=f32_bits(0x3f229946u);
    const float poly_b=f32_bits(0x3e722614u);
    const float poly_c=f32_bits(0x3cfbf0a8u);
    const float poly_d=f32_bits(0x3abdb181u);
    float smooth=-1.0f;
    for(uint32_t lane=0u;lane<state->active_width;lane++){
        float maximum=-1.0f;
        for(uint32_t row=0u;row<rows->row_count;row++){
            const float value=rows->rows[row][lane];
            if(maximum<value)maximum=value;
        }
        const float target=maximum+lane_offset[lane];
        const float envelope=stage_b_rt_envelope_curve(state->config,target,state->envelope[lane]);
        state->envelope[lane]=envelope;
        state->status[lane]=(uint32_t)(envelope<target);

        const float high=(smooth<target)?target:smooth;
        const float difference=target-smooth;
        const float magnitude=(-difference<difference)?difference:-difference;
        if(magnitude<smooth_limit){
            float z=fmaf(-magnitude,poly_a,poly_b);
            z=fmaf(z,magnitude,-poly_c);
            z=fmaf(z,magnitude,poly_d);
            z=fmaf(z,16.0f,high);
            if(z<-1.0f)z=-1.0f;
            if(1.0f<z)z=1.0f;
            smooth=z;
        }else smooth=high;
    }
    const float scalar=stage_b_rt_envelope_curve(state->config,smooth,state->scalar_envelope);
    state->scalar_envelope=scalar;
    return scalar<smooth;
}

void ubig_stage_b_rt_envelope_activity_process(UbigStageBRtEnvelopeState *state,
                                               const UbigStageBRtBandRows *rows,
                                               const float *lane_offset)
{
    if(!state||!state->config||!state->config->lane_weight||!rows||!rows->rows||
       !lane_offset||state->active_width>UBIG_STAGE_B_RT_MAX_BANDS)return;
    const int rising=ubig_stage_b_rt_envelope_track(state,rows,lane_offset);
    float maximum=-1.0f;
    float accumulator=-1.0f;
    const float high_bias=f32_bits(0x3e1d89d9u);
    const float low_bias=f32_bits(0x3d1d89d9u);
    for(uint32_t lane=0u;lane<state->active_width;lane++){
        const float half=state->envelope[lane]*0.5f;
        const float high=half-high_bias;
        if(maximum<high)maximum=high;
        const float low=half-low_bias;
        if(low<=maximum){
            const float weighted=state->config->lane_weight[lane]*(maximum-low);
            accumulator=fmaf(weighted,0.25f,accumulator);
        }
    }
    float activity=(accumulator+1.0f)*8.0f;
    if(activity<0.0f)activity=0.0f;
    const float cap=f32_bits(0x3d44ec4fu);
    if(cap<activity)activity=cap;
    activity*=f32_bits(0x3f266600u);
    float target=fmaf(-activity,f32_bits(0x42000000u),1.0f);
    if(!rising||target<state->activity_state)
        target=fmaf(state->config->smooth_keep,state->activity_state,
                    state->config->smooth_inject*target);
    for(uint32_t lane=0u;lane<state->active_width;lane++){
        float value=target;
        if(state->status[lane]==0u||target<state->lane_activity[lane])
            value=fmaf(state->config->smooth_keep,state->lane_activity[lane],
                       state->config->smooth_inject*target);
        state->lane_activity[lane]=value;
    }
    state->activity_state=target;
}


typedef struct { float r,i; } StageBRtComplex64;

static void stage_b_rt_fft64_radix8(StageBRtComplex64 y[8],const StageBRtComplex64 x[8])
{
    const float h=f32_bits(0x3f3504f3u);
    const float r0=x[0].r,i0=x[0].i,r1=x[1].r,i1=x[1].i;
    const float r2=x[2].r,i2=x[2].i,r3=x[3].r,i3=x[3].i;
    const float r4=x[4].r,i4=x[4].i,r5=x[5].r,i5=x[5].i;
    const float r6=x[6].r,i6=x[6].i,r7=x[7].r,i7=x[7].i;
    const float a04r=r0+r4,d04r=r0-r4,a04i=i0+i4,d04i=i0-i4;
    const float a26r=r2+r6,d26r=r2-r6,a26i=i2+i6,d26i=i2-i6;
    const float e2r=a04r-a26r,e0r=a04r+a26r;
    const float e3r=d04r-d26i,e1r=d04r+d26i;
    const float e0i=a04i+a26i,e2i=a04i-a26i;
    const float e3i=d04i+d26r,e1i=d04i-d26r;

    const float a15r=r1+r5,d15r=r1-r5,a15i=i1+i5,d15i=i1-i5;
    const float a37r=r3+r7,d37r=r3-r7,a37i=i3+i7,d37i=i3-i7;
    const float o1r=d15r+d37i;
    const float h_o1r=o1r*h;
    const float o1i=d15i-d37r;
    const float h_o1i=o1i*h;
    const float o3r=d15r-d37i;
    const float nh_o3r=o3r*(-h);
    const float o3i=d15i+d37r;
    const float o2i=a15i-a37i;
    const float nh_o3i=o3i*(-h);
    const float o0r=a15r+a37r;
    const float t3r=nh_o3r-nh_o3i;
    const float o0i=a15i+a37i;
    const float neg_o2r=a37r-a15r;
    const float t1r=h_o1r+h_o1i;
    const float t3i=nh_o3i+nh_o3r;
    const float t1i=h_o1i-h_o1r;

    y[0]=(StageBRtComplex64){e0r+o0r,e0i+o0i};
    y[4]=(StageBRtComplex64){e0r-o0r,e0i-o0i};
    y[1]=(StageBRtComplex64){e1r+t1r,e1i+t1i};
    y[5]=(StageBRtComplex64){e1r-t1r,e1i-t1i};
    y[2]=(StageBRtComplex64){e2r+o2i,e2i+neg_o2r};
    y[6]=(StageBRtComplex64){e2r-o2i,e2i-neg_o2r};
    y[3]=(StageBRtComplex64){e3r+t3r,e3i+t3i};
    y[7]=(StageBRtComplex64){e3r-t3r,e3i-t3i};
}

static const float stage_b_rt_fft64_tw_c[8][8]={
 {0x1.0000000000000p+0f,0x1.0000000000000p+0f,0x1.0000000000000p+0f,0x1.0000000000000p+0f,0x1.0000000000000p+0f,0x1.0000000000000p+0f,0x1.0000000000000p+0f,0x1.0000000000000p+0f},
 {0x1.0000000000000p+0f,0x1.fd88da0000000p-1f,0x1.f6297c0000000p-1f,0x1.e9f4160000000p-1f,0x1.d906bc0000000p-1f,0x1.c38b300000000p-1f,0x1.a9b6620000000p-1f,0x1.8bc8060000000p-1f},
 {0x1.0000000000000p+0f,0x1.f6297c0000000p-1f,0x1.d906bc0000000p-1f,0x1.a9b6620000000p-1f,0x1.6a09e60000000p-1f,0x1.1c73b40000000p-1f,0x1.87de2a0000000p-2f,0x1.8f8b840000000p-3f},
 {0x1.0000000000000p+0f,0x1.e9f4160000000p-1f,0x1.a9b6620000000p-1f,0x1.44cf320000000p-1f,0x1.87de2a0000000p-2f,0x1.917a6c0000000p-4f,-0x1.8f8b840000000p-3f,-0x1.e2b5d40000000p-2f},
 {0x1.0000000000000p+0f,0x1.d906bc0000000p-1f,0x1.6a09e60000000p-1f,0x1.87de2a0000000p-2f,0x0.0p+0f,-0x1.87de2a0000000p-2f,-0x1.6a09e60000000p-1f,-0x1.d906bc0000000p-1f},
 {0x1.0000000000000p+0f,0x1.c38b300000000p-1f,0x1.1c73b40000000p-1f,0x1.917a6c0000000p-4f,-0x1.87de2a0000000p-2f,-0x1.8bc8060000000p-1f,-0x1.f6297c0000000p-1f,-0x1.e9f4160000000p-1f},
 {0x1.0000000000000p+0f,0x1.a9b6620000000p-1f,0x1.87de2a0000000p-2f,-0x1.8f8b840000000p-3f,-0x1.6a09e60000000p-1f,-0x1.f6297c0000000p-1f,-0x1.d906bc0000000p-1f,-0x1.1c73b40000000p-1f},
 {0x1.0000000000000p+0f,0x1.8bc8060000000p-1f,0x1.8f8b840000000p-3f,-0x1.e2b5d40000000p-2f,-0x1.d906bc0000000p-1f,-0x1.e9f4160000000p-1f,-0x1.1c73b40000000p-1f,0x1.917a6c0000000p-4f},
};
static const float stage_b_rt_fft64_tw_s[8][8]={
 {-0x0.0p+0f,-0x0.0p+0f,-0x0.0p+0f,-0x0.0p+0f,-0x0.0p+0f,-0x0.0p+0f,-0x0.0p+0f,-0x0.0p+0f},
 {-0x0.0p+0f,-0x1.917a6c0000000p-4f,-0x1.8f8b840000000p-3f,-0x1.2940620000000p-2f,-0x1.87de2a0000000p-2f,-0x1.e2b5d40000000p-2f,-0x1.1c73b40000000p-1f,-0x1.44cf320000000p-1f},
 {-0x0.0p+0f,-0x1.8f8b840000000p-3f,-0x1.87de2a0000000p-2f,-0x1.1c73b40000000p-1f,-0x1.6a09e60000000p-1f,-0x1.a9b6620000000p-1f,-0x1.d906bc0000000p-1f,-0x1.f6297c0000000p-1f},
 {-0x0.0p+0f,-0x1.2940620000000p-2f,-0x1.1c73b40000000p-1f,-0x1.8bc8060000000p-1f,-0x1.d906bc0000000p-1f,-0x1.fd88da0000000p-1f,-0x1.f6297c0000000p-1f,-0x1.c38b300000000p-1f},
 {-0x0.0p+0f,-0x1.87de2a0000000p-2f,-0x1.6a09e60000000p-1f,-0x1.d906bc0000000p-1f,-0x1.0000000000000p+0f,-0x1.d906bc0000000p-1f,-0x1.6a09e60000000p-1f,-0x1.87de2a0000000p-2f},
 {-0x0.0p+0f,-0x1.e2b5d40000000p-2f,-0x1.a9b6620000000p-1f,-0x1.fd88da0000000p-1f,-0x1.d906bc0000000p-1f,-0x1.44cf320000000p-1f,-0x1.8f8b840000000p-3f,0x1.2940620000000p-2f},
 {-0x0.0p+0f,-0x1.1c73b40000000p-1f,-0x1.d906bc0000000p-1f,-0x1.f6297c0000000p-1f,-0x1.6a09e60000000p-1f,-0x1.8f8b840000000p-3f,0x1.87de2a0000000p-2f,0x1.a9b6620000000p-1f},
 {-0x0.0p+0f,-0x1.44cf320000000p-1f,-0x1.f6297c0000000p-1f,-0x1.c38b300000000p-1f,-0x1.87de2a0000000p-2f,0x1.2940620000000p-2f,0x1.a9b6620000000p-1f,0x1.fd88da0000000p-1f},
};


void ubig_stage_b_rt_fft64(float output[UBIG_STAGE_B_RT_FFT64_FLOATS],
                           const float input[UBIG_STAGE_B_RT_FFT64_FLOATS])
{
    if(!output||!input)return;
    StageBRtComplex64 stage[8][8];
    for(uint32_t n1=0u;n1<8u;n1++){
        StageBRtComplex64 x[8],y[8];
        for(uint32_t n2=0u;n2<8u;n2++){
            const uint32_t index=n1+8u*n2;
            x[n2].r=input[2u*index];
            x[n2].i=input[2u*index+1u];
        }
        stage_b_rt_fft64_radix8(y,x);
        for(uint32_t k2=0u;k2<8u;k2++)stage[n1][k2]=y[k2];
    }
    for(uint32_t k2=0u;k2<8u;k2++){
        StageBRtComplex64 x[8],y[8];
        for(uint32_t n1=0u;n1<8u;n1++){
            const StageBRtComplex64 a=stage[n1][k2];
            const float c=stage_b_rt_fft64_tw_c[k2][n1];
            const float si=stage_b_rt_fft64_tw_s[k2][n1];
            const float real_base=a.r*c;
            const float imag_base=a.i*c;
            x[n1].r=fmaf(-a.i,si,real_base);
            x[n1].i=fmaf(a.r,si,imag_base);
        }
        stage_b_rt_fft64_radix8(y,x);
        for(uint32_t k1=0u;k1<8u;k1++){
            const uint32_t k=k2+8u*k1;
            output[2u*k]=y[k1].r;
            output[2u*k+1u]=y[k1].i;
        }
    }
}


#if defined(__aarch64__)
static float32x4_t stage_b_rt_reverse4_f32(float32x4_t value)
{
    const float32x4_t pairs=vrev64q_f32(value);
    return vcombine_f32(vget_high_f32(pairs),vget_low_f32(pairs));
}
#endif

void ubig_stage_b_rt_transform64_process(
    float *state_rows[UBIG_STAGE_B_RT_TRANSFORM64_ROWS],
    const float filter[UBIG_STAGE_B_RT_TRANSFORM64_FILTER_FLOATS],
    const float phase[UBIG_STAGE_B_RT_TRANSFORM64_PHASE_FLOATS],
    const float *source[UBIG_STAGE_B_RT_TRANSFORM64_BLOCKS][UBIG_STAGE_B_RT_TRANSFORM64_ROWS],
    float *output[UBIG_STAGE_B_RT_TRANSFORM64_BLOCKS][UBIG_STAGE_B_RT_TRANSFORM64_ROWS])
{
    if(!state_rows||!filter||!phase||!source||!output)return;
    for(uint32_t block=0u;block<UBIG_STAGE_B_RT_TRANSFORM64_BLOCKS;block++){
        for(uint32_t row=0u;row<UBIG_STAGE_B_RT_TRANSFORM64_ROWS;row++){
            if(!state_rows[row]||!source[block][row]||!output[block][row])continue;
            float fft_input[UBIG_STAGE_B_RT_FFT64_FLOATS];
            float fft_output[UBIG_STAGE_B_RT_FFT64_FLOATS];
            const float *src=source[block][row];
            for(uint32_t chunk=0u;chunk<8u;chunk++){
                for(uint32_t lane=0u;lane<4u;lane++){
                    const uint32_t low=4u*chunk+lane;
                    const uint32_t high=63u-low;
                    fft_input[2u*low]=src[16u*chunk+4u*lane];
                    fft_input[2u*low+1u]=-src[16u*chunk+4u*lane+1u];
                    fft_input[2u*high]=src[16u*chunk+4u*lane+2u];
                    fft_input[2u*high+1u]=src[16u*chunk+4u*lane+3u];
                }
            }
            ubig_stage_b_rt_fft64(fft_output,fft_input);
            float *history=state_rows[row];
            float *dst=output[block][row];
            for(uint32_t section=0u;section<16u;section++){
                float *h=history+36u*section;
                const float *coef=filter+600u-40u*section;
#if defined(__aarch64__)
                const float32x4x2_t bins=vld2q_f32(fft_output+8u*section);
                const float32x4_t phase_r=vld1q_f32(phase+8u*section);
                const float32x4_t phase_i=vld1q_f32(phase+8u*section+4u);
                float32x4_t real=vmulq_f32(bins.val[0],phase_r);
                real=vfmsq_f32(real,bins.val[1],phase_i);
                float32x4_t imag=vmulq_f32(bins.val[0],phase_i);
                imag=vfmaq_f32(imag,bins.val[1],phase_r);
                real=stage_b_rt_reverse4_f32(real);
                imag=stage_b_rt_reverse4_f32(imag);

                float32x4_t a=vld1q_f32(h);
                a=vfmaq_f32(a,real,vld1q_f32(coef+36u));
                const float32x4_t reversed=stage_b_rt_reverse4_f32(a);
                vst1q_f32(dst+4u*section,vaddq_f32(reversed,reversed));

                a=vld1q_f32(h+4u);
                a=vfmaq_f32(a,imag,vld1q_f32(coef+32u));
                vst1q_f32(h,a);
                float32x4_t b=vld1q_f32(h+12u);
                b=vfmsq_f32(b,imag,vld1q_f32(coef+24u));
                a=vld1q_f32(h+8u);
                a=vfmsq_f32(a,real,vld1q_f32(coef+28u));
                vst1q_f32(h+4u,a);vst1q_f32(h+8u,b);

                a=vld1q_f32(h+16u);
                a=vfmaq_f32(a,real,vld1q_f32(coef+20u));
                b=vld1q_f32(h+20u);
                b=vfmaq_f32(b,imag,vld1q_f32(coef+16u));
                vst1q_f32(h+12u,a);vst1q_f32(h+16u,b);

                a=vld1q_f32(h+24u);
                a=vfmsq_f32(a,real,vld1q_f32(coef+12u));
                b=vld1q_f32(h+28u);
                b=vfmsq_f32(b,imag,vld1q_f32(coef+8u));
                vst1q_f32(h+20u,a);vst1q_f32(h+24u,b);

                b=vmulq_f32(imag,vld1q_f32(coef));
                a=vld1q_f32(h+32u);
                a=vfmaq_f32(a,real,vld1q_f32(coef+4u));
                vst1q_f32(h+28u,a);vst1q_f32(h+32u,b);
#else
                float real[4],imag[4];
                for(uint32_t lane=0u;lane<4u;lane++){
                    const float xr=fft_output[8u*section+2u*lane];
                    const float xi=fft_output[8u*section+2u*lane+1u];
                    const float pr=phase[8u*section+lane];
                    const float pi=phase[8u*section+4u+lane];
                    const float rr=xr*pr;
                    const float ii=xr*pi;
                    real[3u-lane]=fmaf(-xi,pi,rr);
                    imag[3u-lane]=fmaf(xi,pr,ii);
                }
                for(uint32_t lane=0u;lane<4u;lane++){
                    const float first=fmaf(real[lane],coef[36u+lane],h[lane]);
                    dst[4u*section+(3u-lane)]=first+first;
                    h[lane]=fmaf(imag[lane],coef[32u+lane],h[4u+lane]);
                    const float s8=fmaf(-real[lane],coef[28u+lane],h[8u+lane]);
                    const float s12=fmaf(-imag[lane],coef[24u+lane],h[12u+lane]);
                    h[4u+lane]=s8;h[8u+lane]=s12;
                    const float s16=fmaf(real[lane],coef[20u+lane],h[16u+lane]);
                    const float s20=fmaf(imag[lane],coef[16u+lane],h[20u+lane]);
                    h[12u+lane]=s16;h[16u+lane]=s20;
                    const float s24=fmaf(-real[lane],coef[12u+lane],h[24u+lane]);
                    const float s28=fmaf(-imag[lane],coef[8u+lane],h[28u+lane]);
                    h[20u+lane]=s24;h[24u+lane]=s28;
                    h[28u+lane]=fmaf(real[lane],coef[4u+lane],h[32u+lane]);
                    h[32u+lane]=imag[lane]*coef[lane];
                }
#endif
            }
        }
    }
}


static void stage_b_rt_late_aggregate(float *values)
{
    float a0=0.0f,a1=0.0f,a2=0.0f,a3=0.0f,a4=0.0f,a5=0.0f;
    a0=values[0]+a0;a0+=values[4];a0+=values[8];a0+=values[12];a0+=values[14];a0+=values[18];
    a1=values[1]+a1;a1+=values[5];a1+=values[9];a1+=values[13];a1+=values[15];a1+=values[19];
    a0=values[6]+a0;a0+=values[2];
    a1=a1-values[7];a1=a1-values[3];
    a2=values[24]+a2;a2+=values[10];a2+=values[16];a2+=values[20];
    a3=a3+values[25];a3+=values[11];a3+=values[17];a3+=values[21];
    a4=values[26]+a4;a4+=values[28];a4+=values[30];a4+=values[22];
    a5=a5+values[27];a5+=values[29];a5+=values[31];a5+=values[23];
    values[26]=a0;values[27]=a1;values[28]=a2;values[29]=a3;values[30]=a4;values[31]=a5;
}

static float stage_b_rt_late_history_block(UbigStageBRtLateControllerState *state,
                                           const UbigStageBRtLateControllerConfig *config,
                                           float *row0,float *row1)
{
    float peak=ubig_stage_b_rt_max_abs4(row0,UBIG_STAGE_B_RT_LATE_BLOCK_FLOATS);
    const float row1_peak=ubig_stage_b_rt_max_abs4(row1,UBIG_STAGE_B_RT_LATE_BLOCK_FLOATS);
    if(peak<row1_peak)peak=row1_peak;
    const float held=(peak>state->previous_peak)?peak:state->previous_peak;
    state->previous_peak=peak;
    state->delayed_envelope=state->envelope;

    const float product=held*state->gain;
    float candidate=1.0f;
    if(config->limit<product){
        const float inverse=(float)(1.0/(double)product);
        candidate=inverse*config->limit;
    }
    float minimum=candidate;
    if(state->minimum_ring<minimum)minimum=state->minimum_ring;
    state->minimum_ring=candidate;
    state->ring_index=0u;

    const float old_gain=state->gain;
    const float old_envelope=state->envelope;
    const float target=candidate*old_gain;
    float control=candidate;
    if(target<old_envelope){
        state->envelope=target;
        state->smoothed=candidate;
    }else{
        const float alpha=config->response_curve[4];
        float smooth=fmaf(-minimum,alpha,minimum);
        smooth=fmaf(state->smoothed,alpha,smooth);
        state->smoothed=smooth;
        control=smooth;
        const float smoothed_target=smooth*old_gain;
        float envelope=old_envelope;
        if(envelope<smoothed_target)envelope=smoothed_target;
        if(target<envelope)envelope=target;
        state->envelope=envelope;
    }

    float next_gain;
    if(control<f32_bits(0x3f576600u)){
        float term=control*old_gain;
        term*=f32_bits(0x3f98209cu);
        term*=config->response_curve[1];
        next_gain=fmaf(config->response_curve[0],old_gain,term);
    }else{
        next_gain=fmaf(config->response_curve[2],old_gain,config->response_curve[3]);
    }
    state->gain=next_gain;

    UbigStageBRtSymmetricHistoryMix mix={
        config->history_kernel,UBIG_STAGE_B_RT_LATE_BLOCK_FLOATS,
        config->history_scale,state->delayed_envelope,
        state->envelope,config->history_scale,state->history
    };
    ubig_stage_b_rt_symmetric_history_mix(&mix,row0,row0,0u);
    ubig_stage_b_rt_symmetric_history_mix(&mix,row1,row1,1u);
    state->history_scale=config->history_scale;
    return state->envelope;
}

void ubig_stage_b_rt_late_controller_process(
    UbigStageBRtLateControllerState *state,
    const UbigStageBRtLateControllerConfig *config,
    float *analysis[UBIG_STAGE_B_RT_LATE_ROWS][UBIG_STAGE_B_RT_LATE_BLOCKS],
    float rows[UBIG_STAGE_B_RT_LATE_ROWS][UBIG_STAGE_B_RT_LATE_ROW_FLOATS])
{
    if(!state||!config||!analysis||!rows||!config->transform_filter||!config->transform_phase||
       !config->response_curve||!config->history_kernel)return;
    for(uint32_t block=0u;block<UBIG_STAGE_B_RT_LATE_BLOCKS;block++)
        for(uint32_t row=0u;row<UBIG_STAGE_B_RT_LATE_ROWS;row++)
            if(analysis[row][block])stage_b_rt_late_aggregate(analysis[row][block]);

    float *transform_state[UBIG_STAGE_B_RT_LATE_ROWS]={state->transform_history[0],state->transform_history[1]};
    const float *source[UBIG_STAGE_B_RT_LATE_BLOCKS][UBIG_STAGE_B_RT_LATE_ROWS];
    float *output[UBIG_STAGE_B_RT_LATE_BLOCKS][UBIG_STAGE_B_RT_LATE_ROWS];
    for(uint32_t block=0u;block<UBIG_STAGE_B_RT_LATE_BLOCKS;block++){
        for(uint32_t row=0u;row<UBIG_STAGE_B_RT_LATE_ROWS;row++){
            source[block][row]=analysis[row][block]?analysis[row][block]+26u:0;
            output[block][row]=rows[row]+UBIG_STAGE_B_RT_LATE_BLOCK_FLOATS*block;
        }
    }
    ubig_stage_b_rt_transform64_process(transform_state,config->transform_filter,
                                        config->transform_phase,source,output);

    float minimum_envelope=1.0f;
    for(uint32_t block=0u;block<UBIG_STAGE_B_RT_LATE_BLOCKS;block++){
        float *row0=rows[0]+UBIG_STAGE_B_RT_LATE_BLOCK_FLOATS*block;
        float *row1=rows[1]+UBIG_STAGE_B_RT_LATE_BLOCK_FLOATS*block;
        const float envelope=stage_b_rt_late_history_block(state,config,row0,row1);
        if(envelope<minimum_envelope)minimum_envelope=envelope;
    }
    int exponent=0;
    const float mantissa=(float)frexp((double)minimum_envelope,&exponent);
    float mapped=fmaf(mantissa,4.0f,-f32_bits(0x402aaaabu));
    const float square=mantissa*mantissa;
    mapped=fmaf(-square,f32_bits(0x3faaaaabu),mapped);
    mapped+=(float)exponent;
    state->output=mapped*f32_bits(0x3d3db1f9u);
}

float ubig_stage_b_rt_max_abs4(const float *input,uint32_t count)
{
    if(!input||count<4u||(count&3u)!=0u)return 0.0f;
#if defined(__aarch64__)
    float32x4_t minimum=vld1q_f32(input);
    float32x4_t maximum=minimum;
    for(uint32_t i=4u;i<count;i+=4u){
        const float32x4_t v=vld1q_f32(input+i);
        maximum=vmaxq_f32(maximum,v);
        minimum=vminq_f32(minimum,v);
    }
    float32x2_t min_pair=vpmin_f32(vget_high_f32(minimum),vget_low_f32(minimum));
    min_pair=vpmin_f32(min_pair,min_pair);
    float32x2_t max_pair=vpmax_f32(vget_high_f32(maximum),vget_low_f32(maximum));
    max_pair=vpmax_f32(max_pair,max_pair);
    float32x2_t result=vdup_n_f32(-vget_lane_f32(min_pair,0));
    result=vset_lane_f32(vget_lane_f32(max_pair,0),result,1);
    result=vpmax_f32(result,result);
    return vget_lane_f32(result,0);
#else
    float minimum=input[0],maximum=input[0];
    for(uint32_t i=1u;i<count;i++){
        if(input[i]<minimum)minimum=input[i];
        if(maximum<input[i])maximum=input[i];
    }
    const float negative_min=-minimum;
    return negative_min<=maximum?maximum:negative_min;
#endif
}

void ubig_stage_b_rt_symmetric_history_mix(UbigStageBRtSymmetricHistoryMix *state,
                                             float *output,
                                             const float *input,
                                             uint32_t history_index)
{
    if(!state||!output||!input||!state->kernel||!state->history||state->count==0u)return;
    const float reflected_gain=state->reflected_scale_a*state->reflected_scale_b;
    const float forward_gain=state->forward_scale_a*state->forward_scale_b;
    float *history=state->history+(size_t)state->count*history_index;
#if defined(__aarch64__)
    const float32x4_t reflected_v=vdupq_n_f32(reflected_gain);
    const float32x4_t forward_v=vdupq_n_f32(forward_gain);
    const float32x4_t scale_v=vdupq_n_f32(256.0f);
    for(uint32_t i=0u;i<state->count;i+=4u){
        const float32x4_t old=vld1q_f32(history+i);
        const float32x4_t tail=vld1q_f32(state->kernel+state->count-i-4u);
        const float32x4_t pair_rev=vrev64q_f32(tail);
        const float32x4_t reflected=vcombine_f32(vget_high_f32(pair_rev),vget_low_f32(pair_rev));
        const float32x4_t forward=vld1q_f32(state->kernel+i);
        const float32x4_t forward_term=vmulq_f32(old,forward_v);
        float32x4_t mixed=vmulq_f32(old,reflected_v);
        mixed=vmulq_f32(mixed,reflected);
        mixed=vfmaq_f32(mixed,forward_term,forward);
        mixed=vmulq_f32(mixed,scale_v);
        vst1q_f32(history+i,vld1q_f32(input+i));
        vst1q_f32(output+i,mixed);
    }
#else
    for(uint32_t i=0u;i<state->count;i++){
        const float old=history[i];
        const float forward_term=old*forward_gain;
        float mixed=(old*reflected_gain)*state->kernel[state->count-1u-i];
        mixed=fmaf(forward_term,state->kernel[i],mixed);
        output[i]=mixed*256.0f;
        history[i]=input[i];
    }
#endif
}

int ubig_stage_b_rt_sparse_complex_mix(float *output,
                                      const float *const *const *rows,
                                      const UbigStageBRtSparseMix *mix,
                                      uint32_t channel,
                                      uint32_t complex_bins)
{
    if(!output||!rows||!mix||(mix->count!=0u&&(!mix->indices||!mix->weights)))return -1;
    if(mix->count==0u){
        memset(output,0,(size_t)complex_bins*2u*sizeof(float));
        return 1;
    }
    uint32_t j=0u;
    for(;j+1u<mix->count;j+=2u){
        const float *a=rows[mix->indices[j]][channel];
        const float *b=rows[mix->indices[j+1u]][channel];
        const float wa=mix->weights[j];
        const float wb=mix->weights[j+1u];
        if(j==0u){
            for(uint32_t k=0u;k<complex_bins;k++){
                const uint32_t i=2u*k;
                const float real0=a[i]*wa;
                const float imag0=a[i+1u]*wa;
                output[i]=fmaf(b[i],wb,real0);
                output[i+1u]=fmaf(b[i+1u],wb,imag0);
            }
        }else{
            for(uint32_t k=0u;k<complex_bins;k++){
                const uint32_t i=2u*k;
                const float real0=fmaf(a[i],wa,output[i]);
                const float imag0=fmaf(a[i+1u],wa,output[i+1u]);
                output[i]=fmaf(b[i],wb,real0);
                output[i+1u]=fmaf(b[i+1u],wb,imag0);
            }
        }
    }
    if(j<mix->count){
        const float *a=rows[mix->indices[j]][channel];
        const float wa=mix->weights[j];
        if(j==0u){
            for(uint32_t i=0u;i<2u*complex_bins;i++)output[i]=a[i]*wa;
        }else{
            for(uint32_t i=0u;i<2u*complex_bins;i++)output[i]=fmaf(a[i],wa,output[i]);
        }
    }
    return 0;
}

uint32_t ubig_stage_b_rt_sparse_remap(UbigStageBRtComplexMatrix *matrix,
                                      const UbigStageBRtSparseRemapPlan *plan,
                                      void *workspace)
{
    if(!matrix||!plan||!matrix->rows||!plan->mixes||!workspace)return 0u;
    const uint32_t prefix=plan->source_rows<plan->target_rows?plan->source_rows:plan->target_rows;
    uint32_t zero_mask=0u;
    const float *const *const *source_rows=(const float *const *const *)matrix->rows;

    for(uint32_t row=prefix;row<plan->target_rows;row++){
        for(uint32_t channel=0u;channel<matrix->channel_count;channel++){
            if(ubig_stage_b_rt_sparse_complex_mix(matrix->rows[row][channel],source_rows,
                                                   &plan->mixes[row],channel,
                                                   matrix->complex_bins)!=0)
                zero_mask|=UINT32_C(1)<<(row&31u);
        }
    }

    const size_t stride_floats=(size_t)((matrix->complex_bins+3u)>>2)*8u;
    float *scratch=(float*)(((uintptr_t)workspace+31u)&~(uintptr_t)31u);
    for(uint32_t channel=0u;channel<matrix->channel_count;channel++){
        for(uint32_t row=0u;row<prefix;row++){
            if(ubig_stage_b_rt_sparse_complex_mix(scratch+(size_t)row*stride_floats,
                                                   source_rows,&plan->mixes[row],channel,
                                                   matrix->complex_bins)!=0)
                zero_mask|=UINT32_C(1)<<(row&31u);
        }
        for(uint32_t row=0u;row<prefix;row++)
            memcpy(matrix->rows[row][channel],scratch+(size_t)row*stride_floats,
                   (size_t)matrix->complex_bins*2u*sizeof(float));
    }
    matrix->row_count=plan->target_rows;
    return zero_mask;
}

void ubig_stage_b_rt_control_export_process(UbigStageBRtControlAggregateState *state,
                                            const UbigStageBRtControlAggregateItem *items,
                                            uint32_t item_count,
                                            int32_t output[UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS])
{
    if(!state||!output||(item_count!=0u&&!items))return;
    float scalar[UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS];
    ubig_stage_b_rt_control_aggregate_process(state,items,item_count,scalar);
    for(uint32_t i=0u;i<UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS;i++)
        output[i]=ubig_stage_b_rt_q31_encode(scalar[i]);
}

void ubig_stage_b_rt_pair_transform(float *row_a,float *row_b,
                                    uint32_t complex_bins,float scale)
{
    if(!row_a||!row_b)return;
    for(uint32_t k=0u;k<complex_bins;k++){
        const uint32_t i=2u*k;
        const float ar=row_a[i];
        const float ai=row_a[i+1u];
        const float br_scaled=row_b[i]*scale;
        const float bi_scaled=row_b[i+1u]*scale;
        row_a[i]=fmaf(ar,scale,br_scaled);
        row_a[i+1u]=fmaf(ai,scale,bi_scaled);
        row_b[i]=fmaf(ar,scale,-br_scaled);
        row_b[i+1u]=fmaf(ai,scale,-bi_scaled);
    }
}

int32_t ubig_stage_b_rt_q31_encode(float value)
{
    if(value>=1.0f)return INT32_C(0x7fffffff);
    if(value<=-1.0f)return INT32_MIN;
    return (int32_t)lrintf(value*f32_bits(0x4f000000u));
}

void ubig_stage_b_rt_universal_analysis_process(UbigStageBRtUniversalAnalysis *state,
                                                const UbigStageBRtUniversalConfig *config,
                                                const UbigStageBRtSpectralExport *input,
                                                UbigStageBRtUniversalOutput *output)
{
    if(!state||!config||!input||!output||!config->feature_history||
       !config->variation||!config->segment_ratio||!config->projection)return;

    const uint32_t actions=ubig_stage_b_rt_scheduler_step(&state->clock);
    float scratch32[32];
    float scratch64[64];

    if(actions&UBIG_STAGE_B_RT_SCHED_UPPER){
        ubig_stage_b_rt_feature_history_process(&state->feature_history,
                                                config->feature_history,input);
        ubig_stage_b_rt_segment_ratio_process(&state->segment_ratio_history,
                                              config->segment_ratio,input);
        ubig_stage_b_rt_variation_history_process(&state->variation_history,
                                                  config->variation,input->bins,input->count);
        ubig_stage_b_rt_spectral_change_process(&state->spectral_change_history,input);
        ubig_stage_b_rt_projection_history_process(&state->projection_history,
                                                   config->projection,input);
        const uint32_t projection_index=state->projection_history.index;
        const uint32_t written=projection_index?projection_index-1u:31u;
        float normalized[UBIG_STAGE_B_RT_FEATURE_COUNT];
        ubig_stage_b_rt_feature_change_process(&state->feature_change_history,
                                               state->projection_history.records[written],normalized);
        float peak_scratch[UBIG_STAGE_B_RT_SPECTRAL_BINS];
        ubig_stage_b_rt_peak_residual_process(&state->peak_residual_history,input,peak_scratch);
    }

    if(actions&UBIG_STAGE_B_RT_SCHED_LOWER_A){
        ubig_stage_b_rt_feature_cadence_process(&state->feature_history,
                                                config->feature_cadence_step,
                                                output->feature_cadence);
        ubig_stage_b_rt_stat32_ring_columns(&state->segment_ratio_cursor,
                                            state->segment_ratio_history.history,
                                            state->segment_ratio_history.index,
                                            scratch64,
                                            output->segment_ratio_mean,
                                            output->segment_ratio_deviation);
    }

    if(actions&UBIG_STAGE_B_RT_SCHED_LOWER_B){
        const float control=ubig_stage_b_rt_feature_history_mean(state->feature_history.records);
        ubig_stage_b_rt_stat32_columns(&state->variation_cursor,
                                       state->variation_history.history,
                                       config->variation->segment_count,
                                       scratch32,
                                       output->variation_mean,
                                       output->variation_deviation);
        ubig_stage_b_rt_stat32_step(&state->spectral_change_cursor,
                                    state->spectral_change_history.history,
                                    scratch32,output->spectral_change);

        UbigStageBRtCadenceSummary projection_summary;
        memcpy(projection_summary.matrix,state->projection_history.records,
               sizeof projection_summary.matrix);
        projection_summary.cursor.step=config->projection_cadence_step;
        projection_summary.cursor.index=state->projection_history.phase;
        memcpy(projection_summary.column_accumulator,state->projection_history.sum,
               sizeof projection_summary.column_accumulator);
        memcpy(projection_summary.delta_accumulator,state->projection_history.delta_sum,
               sizeof projection_summary.delta_accumulator);
        memcpy(projection_summary.column_shift,state->projection_history.shift,
               sizeof projection_summary.column_shift);
        memcpy(projection_summary.delta_shift,state->projection_history.delta_shift,
               sizeof projection_summary.delta_shift);
        ubig_stage_b_rt_cadence_summary_process(&projection_summary,
                                                output->projection_cadence,scratch32);
        state->projection_history.phase=projection_summary.cursor.index;

        ubig_stage_b_rt_stat32_step(&state->feature_change_cursor,
                                    state->feature_change_history.history,
                                    scratch32,output->feature_change);

        UbigStageBRtRankHistory rank;
        memcpy(rank.matrix,state->peak_residual_history.history,sizeof rank.matrix);
        rank.cursor=state->peak_residual_cursor;
        ubig_stage_b_rt_rank_history_process(&rank,control,output->peak_rank,scratch64);
        state->peak_residual_cursor=rank.cursor;
    }
}

void ubig_stage_b_rt_universal_pack_features(const UbigStageBRtUniversalOutput *output,
                                             float features[UBIG_STAGE_B_RT_UNIVERSAL_FEATURES])
{
    if(!output||!features)return;
    uint32_t offset=0u;
#define UBIG_PACK(field,count) do { memcpy(features+offset,output->field,(count)*sizeof(float)); offset+=(count); } while(0)
    UBIG_PACK(feature_cadence,UBIG_STAGE_B_RT_FEATURE_CADENCE_OUTPUTS);
    UBIG_PACK(variation_mean,UBIG_STAGE_B_RT_STAT_COLUMNS);
    UBIG_PACK(variation_deviation,UBIG_STAGE_B_RT_STAT_COLUMNS);
    UBIG_PACK(segment_ratio_mean,UBIG_STAGE_B_RT_STAT_COLUMNS);
    UBIG_PACK(segment_ratio_deviation,UBIG_STAGE_B_RT_STAT_COLUMNS);
    UBIG_PACK(peak_rank,UBIG_STAGE_B_RT_RANK_OUTPUTS);
    UBIG_PACK(projection_cadence,UBIG_STAGE_B_RT_CADENCE_OUTPUTS);
    UBIG_PACK(feature_change,2u);
    UBIG_PACK(spectral_change,2u);
#undef UBIG_PACK
}

void ubig_stage_b_rt_universal_unpack_features(UbigStageBRtUniversalOutput *output,
                                               const float features[UBIG_STAGE_B_RT_UNIVERSAL_FEATURES])
{
    if(!output||!features)return;
    uint32_t offset=0u;
#define UBIG_UNPACK(field,count) do { memcpy(output->field,features+offset,(count)*sizeof(float)); offset+=(count); } while(0)
    UBIG_UNPACK(feature_cadence,UBIG_STAGE_B_RT_FEATURE_CADENCE_OUTPUTS);
    UBIG_UNPACK(variation_mean,UBIG_STAGE_B_RT_STAT_COLUMNS);
    UBIG_UNPACK(variation_deviation,UBIG_STAGE_B_RT_STAT_COLUMNS);
    UBIG_UNPACK(segment_ratio_mean,UBIG_STAGE_B_RT_STAT_COLUMNS);
    UBIG_UNPACK(segment_ratio_deviation,UBIG_STAGE_B_RT_STAT_COLUMNS);
    UBIG_UNPACK(peak_rank,UBIG_STAGE_B_RT_RANK_OUTPUTS);
    UBIG_UNPACK(projection_cadence,UBIG_STAGE_B_RT_CADENCE_OUTPUTS);
    UBIG_UNPACK(feature_change,2u);
    UBIG_UNPACK(spectral_change,2u);
#undef UBIG_UNPACK
}

void ubig_stage_b_rt_analysis_controller_process(UbigStageBRtAnalysisController *state,
                                                 const UbigStageBRtAnalysisControllerConfig *config,
                                                 const float *row0,
                                                 const float *row1)
{
    if(!state||!config||!config->analysis||!config->control||!row0||!row1)return;
    ubig_stage_b_rt_spectral_accumulate(&state->spectral,row0,row1,&state->spectral_export);
    ubig_stage_b_rt_universal_analysis_process(&state->analysis,config->analysis,
                                               &state->spectral_export,&state->analysis_output);
    float features[UBIG_STAGE_B_RT_UNIVERSAL_FEATURES];
    ubig_stage_b_rt_universal_pack_features(&state->analysis_output,features);
    ubig_stage_b_rt_control_cadence_process(&state->control,config->control,features);
    ubig_stage_b_rt_universal_unpack_features(&state->analysis_output,features);
}
