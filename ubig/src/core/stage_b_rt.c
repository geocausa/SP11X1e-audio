#include "stage_b_rt.h"
#include "stage_a_math.h"
#include <math.h>
#include <string.h>

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
