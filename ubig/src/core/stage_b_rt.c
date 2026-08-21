#include "stage_b_rt.h"
#include "stage_a_math.h"
#include <math.h>
#include <string.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif

static float f32_bits(uint32_t u){float f;memcpy(&f,&u,4);return f;}

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
