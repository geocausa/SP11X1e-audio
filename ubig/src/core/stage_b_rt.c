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
