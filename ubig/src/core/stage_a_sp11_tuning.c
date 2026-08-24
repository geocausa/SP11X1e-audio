#include "stage_a_sp11_tuning.h"
#include "stage_a_sp11_tuning_tables.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t sample_rate;
    uint32_t bands;
    float scalar_cfg[6];
    float dual_cfg[10];
    float flag_cfg[4];
    float band_update_cfg[5];
    uint32_t hold_samples;
    float transition_cfg[5];
    uint32_t reserved;
    const float *severity_coeff;
    float direction_cfg[2];
} UbigSp11CompressorConfig;

_Static_assert(offsetof(UbigSp11CompressorConfig,scalar_cfg)==0x08,"scalar cfg offset");
_Static_assert(offsetof(UbigSp11CompressorConfig,dual_cfg)==0x20,"dual cfg offset");
_Static_assert(offsetof(UbigSp11CompressorConfig,flag_cfg)==0x48,"flag cfg offset");
_Static_assert(offsetof(UbigSp11CompressorConfig,band_update_cfg)==0x58,"band cfg offset");
_Static_assert(offsetof(UbigSp11CompressorConfig,hold_samples)==0x6c,"hold offset");
_Static_assert(offsetof(UbigSp11CompressorConfig,transition_cfg)==0x70,"transition offset");
_Static_assert(offsetof(UbigSp11CompressorConfig,severity_coeff)==0x88,"severity ptr offset");
_Static_assert(offsetof(UbigSp11CompressorConfig,direction_cfg)==0x90,"direction cfg offset");
_Static_assert(sizeof(UbigSp11CompressorConfig)==0x98,"compressor cfg size");

static const UbigSp11CompressorConfig compressor_cfg={
    .sample_rate=48000u,.bands=20u,
    .scalar_cfg={
#define V(i) ubig_sp11_comp_scalar_cfg[i]
        V(0),V(1),V(2),V(3),V(4),V(5)
#undef V
    },
    .dual_cfg={
#define V(i) ubig_sp11_comp_dual_cfg[i]
        V(0),V(1),V(2),V(3),V(4),V(5),V(6),V(7),V(8),V(9)
#undef V
    },
    .flag_cfg={ubig_sp11_comp_flag_cfg[0],ubig_sp11_comp_flag_cfg[1],ubig_sp11_comp_flag_cfg[2],ubig_sp11_comp_flag_cfg[3]},
    .band_update_cfg={ubig_sp11_comp_band_update_cfg[0],ubig_sp11_comp_band_update_cfg[1],ubig_sp11_comp_band_update_cfg[2],ubig_sp11_comp_band_update_cfg[3],ubig_sp11_comp_band_update_cfg[4]},
    .hold_samples=UBIG_SP11_COMP_HOLD_SAMPLES,
    .transition_cfg={ubig_sp11_comp_transition_cfg[0],ubig_sp11_comp_transition_cfg[1],ubig_sp11_comp_transition_cfg[2],ubig_sp11_comp_transition_cfg[3],ubig_sp11_comp_transition_cfg[4]},
    .reserved=UBIG_SP11_COMP_RESERVED,
    .severity_coeff=ubig_sp11_comp_severity_coeff,
    .direction_cfg={ubig_sp11_comp_direction_cfg[0],ubig_sp11_comp_direction_cfg[1]},
};

void ubig_stage_a_sp11_dynamic_config(UbigStageACoreConfig *cfg)
{
    if(!cfg)return;
    memset(cfg,0,sizeof(*cfg));
    cfg->input_scale=UBIG_SP11_STAGE_A_INPUT_SCALE;
    cfg->limiter_ceiling=UBIG_SP11_STAGE_A_LIMITER_CEILING;
    cfg->analysis_enable=1u;
    cfg->compressor_enable=1u;
    cfg->compressor_mode=1u;
    cfg->native_count=8u;
    cfg->compressor_config=&compressor_cfg;
    cfg->compressor_distribution=ubig_sp11_comp_distribution;
    cfg->base_rows[0]=ubig_sp11_stage_a_base_rows_flat;
    cfg->base_rows[1]=ubig_sp11_stage_a_base_rows_flat+20;
    cfg->side_a=ubig_sp11_stage_a_side_a;
    cfg->side_b=ubig_sp11_stage_a_side_b;
    cfg->mask=ubig_sp11_stage_a_mask;
    cfg->runtime5=ubig_sp11_stage_a_runtime;
    cfg->drive_state=UBIG_SP11_STAGE_A_DRIVE_STATE;
    cfg->controller_drive=UBIG_SP11_STAGE_A_CONTROLLER_DRIVE;
    cfg->channel_mix=ubig_sp11_stage_a_channel_mix;
}

static const UbigStageAProfileFamilyState profile_family_state[2]={
    [UBIG_STAGE_A_PROFILE_FAMILY_COMMON]={
        .group_count=UBIG_SP11_FAMILY_COMMON_GROUP_COUNT,
        .groups=ubig_sp11_family_common_groups,
        .channel_deviation=UBIG_SP11_FAMILY_COMMON_CHANNEL_DEVIATION,
        .slow_gain_enable=UBIG_SP11_FAMILY_COMMON_SLOW_GAIN_ENABLE,
        .slow_gain_mix=UBIG_SP11_FAMILY_COMMON_SLOW_GAIN_MIX,
    },
    [UBIG_STAGE_A_PROFILE_FAMILY_MOVIE_MUSIC]={
        .group_count=UBIG_SP11_FAMILY_MOVIE_MUSIC_GROUP_COUNT,
        .groups=ubig_sp11_family_movie_music_groups,
        .channel_deviation=UBIG_SP11_FAMILY_MOVIE_MUSIC_CHANNEL_DEVIATION,
        .slow_gain_enable=UBIG_SP11_FAMILY_MOVIE_MUSIC_SLOW_GAIN_ENABLE,
        .slow_gain_mix=UBIG_SP11_FAMILY_MOVIE_MUSIC_SLOW_GAIN_MIX,
    },
};

const UbigStageAProfileFamilyState *
ubig_stage_a_sp11_profile_family_state(UbigStageAProfileFamily family)
{
    return family==UBIG_STAGE_A_PROFILE_FAMILY_COMMON ||
           family==UBIG_STAGE_A_PROFILE_FAMILY_MOVIE_MUSIC
        ? &profile_family_state[family] : NULL;
}
