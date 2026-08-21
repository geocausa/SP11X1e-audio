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
