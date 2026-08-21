#include "stage_a_compressor.h"
#include "stage_a_compressor_primitives.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t u32at(const unsigned char*p,size_t o){uint32_t v;memcpy(&v,p+o,4);return v;}
static uint64_t u64at(const unsigned char*p,size_t o){uint64_t v;memcpy(&v,p+o,8);return v;}
static float f32at(const unsigned char*p,size_t o){float v;memcpy(&v,p+o,4);return v;}

int ubig_stage_a_compressor_process_warm(void *state,
                                         const float *side_a,
                                         const float *side_b,
                                         const int32_t *mask,
                                         const float runtime[5],
                                         const struct ubig_float_rows *input,
                                         const struct ubig_float_rows *output,
                                         uint32_t native_count,
                                         float drive_state,
                                         float common_drive,
                                         float controller_drive,
                                         const float *channel_mix,
                                         int32_t *band_gain_info,
                                         int32_t *matrix_info,
                                         uint32_t *matrix_rows_out)
{
    unsigned char *p=state;
    const uint32_t mode=u32at(p,0x00);
    const uint32_t input_count=input->count;
    uint32_t effective=1;
    if(mode==1 && input_count<=8) effective=input_count;
    const uint32_t mode_cache=(mode==1)?1u:0u;
    if(u32at(p,0x128)!=mode_cache || u32at(p,0x04)!=input_count ||
       f32at(p,0x18)!=drive_state || u32at(p,0x08)!=effective)
        return -1;

    const unsigned char *cfg=(const unsigned char*)(uintptr_t)u64at(p,0x10);
    const uint32_t bands=u32at(cfg,0x04);
    static const int32_t zero_mask[20]={0};
    const int32_t *active_mask=(mode==1)?mask:zero_mask;
    const float *primary=0,*secondary=0;
    int32_t rise_flags[20];
    ubig_comp_dual_plane_update((struct ubig_dual_floor_state*)(p+0x78),input,
                                &primary,&secondary,rise_flags,runtime[4]);

    const float *mix_source=(mode==1)?channel_mix:0;
    if(mix_source){
        ubig_comp_band_controller(p+0x130,(const float*)(p+0x28),input,
                                  (const float*)(p+0x648),effective,
                                  controller_drive,f32at(p,0x1c),
                                  f32at(p,0x20),f32at(p,0x24));
    }

    float half_drive[20]={0};
    for(uint32_t ch=0;ch<effective;++ch){
        float mix_level=0.0f;
        if(mix_source) mix_level=(native_count==1)?mix_source[0]:mix_source[ch];
        struct ubig_scalar_state *bounds=(struct ubig_scalar_state*)(p+0x248+16u*ch);
        struct ubig_scalar_state *curve=(struct ubig_scalar_state*)(p+0x2c8+16u*ch);
        struct ubig_directional_smoother *smooth=(struct ubig_directional_smoother*)(p+0x348+96u*ch);
        float *gain=(float*)(p+0x648+80u*ch);
        float lower[20],upper[20];
        const float *payload=(const float*)ubig_comp_scalar_payload(p+0x130);
        ubig_comp_slow_gain_bounds(bounds,
                                   mode==1?side_a:0,mode==1?side_b:0,
                                   lower,upper,payload,
                                   common_drive,drive_state,runtime[1],runtime[3],mix_level);
        ubig_comp_linked_deviation(active_mask,secondary,bands,upper,lower,
                                   mode==1?runtime[0]:1.0f);
        ubig_comp_nonlinear_correction(curve,primary,upper,lower,active_mask,
                                       half_drive,gain,runtime[0],runtime[2]);
        ubig_comp_directional_smooth(smooth,rise_flags,gain);
        ubig_comp_neighbor_limit(bands,active_mask,smooth->value,gain);
    }

    for(uint32_t row=0;row<input_count;++row){
        const uint32_t grow=(effective==1)?0u:row;
        const float *gain=(const float*)(p+0x648+80u*grow);
        float *ir=input->rows[row];
        float *orow=output->rows[row];
        for(uint32_t b=0;b<bands;++b){ir[b]+=gain[b];orow[b]+=gain[b];}
    }

    if(matrix_info){
        for(uint32_t row=0;row<effective;++row){
            const float *gain=(const float*)(p+0x648+80u*row);
            for(uint32_t b=0;b<bands;++b)
                matrix_info[row*20u+b]=(int32_t)floorf(gain[b]*2080.0f);
        }
    }
    if(matrix_rows_out)*matrix_rows_out=effective;
    if(band_gain_info){
        for(uint32_t b=0;b<bands;++b)
            band_gain_info[b]=(int32_t)floorf(half_drive[b]*4160.0f);
    }
    return 0;
}

int ubig_stage_a_compressor_process(void *state,
                                    const float *side_a,
                                    const float *side_b,
                                    const int32_t *mask,
                                    const float runtime[5],
                                    const struct ubig_float_rows *input,
                                    const struct ubig_float_rows *output,
                                    uint32_t native_count,
                                    float drive_state,
                                    float common_drive,
                                    float controller_drive,
                                    const float *channel_mix,
                                    int32_t *band_gain_info,
                                    int32_t *matrix_info,
                                    uint32_t *matrix_rows_out)
{
    unsigned char *p=state;
    const uint32_t mode=u32at(p,0x00);
    const uint32_t input_count=input->count;
    const uint32_t mode_cache=(mode==1)?1u:0u;
    uint32_t effective=1u;
    if(mode==1 && input_count<=8u)effective=input_count;
    const unsigned char *cfg=(const unsigned char*)(uintptr_t)u64at(p,0x10);
    const uint32_t bands=u32at(cfg,0x04);

    if(u32at(p,0x128)!=mode_cache){
        if(mode!=1)
            ubig_comp_band_state_init(p+0x130,cfg+0x58,bands);
        memcpy(p+0x128,&mode_cache,4);
    }

    if(u32at(p,0x04)!=input_count || f32at(p,0x18)!=drive_state || u32at(p,0x08)!=effective){
        memcpy(p+0x04,&input_count,4);
        memcpy(p+0x08,&effective,4);
        memcpy(p+0x18,&drive_state,4);
        ubig_comp_dual_floor_init((struct ubig_dual_floor_state*)(p+0x78),(const float*)(cfg+0x20),bands);
        ubig_comp_band_state_init(p+0x130,cfg+0x58,bands);
        for(uint32_t ch=0;ch<8u;++ch){
            memset(p+0x648+80u*ch,0,80);
            ubig_comp_flag_state_init((struct ubig_flag_state*)(p+0x2c8+16u*ch),(const float*)(cfg+0x48),bands);
            ubig_comp_scalar_state_init((struct ubig_scalar_state*)(p+0x248+16u*ch),(const float*)(cfg+0x08),bands);
            ubig_comp_uniform_state_init((struct ubig_directional_smoother*)(p+0x348+96u*ch),(const float*)(cfg+0x90),bands);
        }
    }

    return ubig_stage_a_compressor_process_warm(state,side_a,side_b,mask,runtime,input,output,
                                                 native_count,drive_state,common_drive,controller_drive,
                                                 channel_mix,band_gain_info,matrix_info,matrix_rows_out);
}
