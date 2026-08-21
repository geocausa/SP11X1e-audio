#ifndef UBIG_STAGE_A_CORE_H
#define UBIG_STAGE_A_CORE_H
#include <stdint.h>
#include "stage_a_limiter.h"

#define UBIG_STAGE_A_CORE_CHANNELS 2u
#define UBIG_STAGE_A_CORE_BANDS 20u
#define UBIG_STAGE_A_CORE_FRAMES 256u
#define UBIG_STAGE_A_CORE_TRANSFORM 320u
#define UBIG_STAGE_A_CORE_PHASES 2u
#define UBIG_STAGE_A_COMPRESSOR_STORAGE 0x910u

typedef struct {
    float input_scale;
    float limiter_ceiling;
    uint32_t analysis_enable;
    uint32_t compressor_enable;
    uint32_t compressor_mode;
    uint32_t native_count;
    const void *compressor_config;
    const int32_t *compressor_distribution;
    const float *base_rows[UBIG_STAGE_A_CORE_CHANNELS];
    const float *side_a;
    const float *side_b;
    const int32_t *mask;
    const float *runtime5;
    float drive_state;
    float controller_drive;
    const float *channel_mix;
} UbigStageACoreConfig;

typedef struct {
    uint32_t phase_index;
    float history[64];
    float band_data[UBIG_STAGE_A_CORE_PHASES*2u*UBIG_STAGE_A_CORE_TRANSFORM];
    float overlap[2u*UBIG_STAGE_A_CORE_TRANSFORM];
    float gains[UBIG_STAGE_A_CORE_PHASES*UBIG_STAGE_A_CORE_BANDS];
} UbigStageAChannelState;

typedef struct {
    UbigStageAChannelState channel[UBIG_STAGE_A_CORE_CHANNELS];
    unsigned char compressor_storage[UBIG_STAGE_A_COMPRESSOR_STORAGE];
    void *compressor_state;
    ubig_stage_a_limiter limiter;
    float limiter_feedback;
    uint32_t startup_blend_index;
    uint32_t startup_blend_active;
    float analysis_input[UBIG_STAGE_A_CORE_CHANNELS][UBIG_STAGE_A_CORE_FRAMES];
    float export_state[UBIG_STAGE_A_CORE_BANDS];
    int32_t export_raw[UBIG_STAGE_A_CORE_BANDS];
    float analyzed[UBIG_STAGE_A_CORE_CHANNELS][UBIG_STAGE_A_CORE_BANDS];
    float main_rows[UBIG_STAGE_A_CORE_CHANNELS][UBIG_STAGE_A_CORE_BANDS];
    float companion_rows[UBIG_STAGE_A_CORE_CHANNELS][UBIG_STAGE_A_CORE_BANDS];
    int32_t band_gain_info[UBIG_STAGE_A_CORE_BANDS];
    int32_t matrix_info[8u*UBIG_STAGE_A_CORE_BANDS];
    uint32_t matrix_rows;
    float scratch[4u*UBIG_STAGE_A_CORE_TRANSFORM];
} UbigStageACoreState;

int ubig_stage_a_core_init(UbigStageACoreState *s,const UbigStageACoreConfig *cfg);
int ubig_stage_a_core_process_256(UbigStageACoreState *s,const UbigStageACoreConfig *cfg,
                                  const float *left_in,const float *right_in,
                                  float *left_out,float *right_out);
#endif
