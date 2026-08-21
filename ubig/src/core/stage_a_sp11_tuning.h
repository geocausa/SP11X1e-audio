#ifndef UBIG_STAGE_A_SP11_TUNING_H
#define UBIG_STAGE_A_SP11_TUNING_H
#include <stdint.h>
#include "stage_a_core.h"

typedef enum {
    UBIG_STAGE_A_PROFILE_FAMILY_COMMON = 0,
    UBIG_STAGE_A_PROFILE_FAMILY_MOVIE_MUSIC = 1
} UbigStageAProfileFamily;

typedef struct {
    uint32_t group_count;
    const int32_t *groups; /* flattened group_count x 6 DEVICE_TUNING payload */
    int32_t channel_deviation;
    uint32_t slow_gain_enable;
    int32_t slow_gain_mix;
} UbigStageAProfileFamilyState;

/* DEVICE_TUNING: populate the common SP11 48 kHz Stage-A audio contract.
 * Direct VLLDP differential testing established that the distinct Movie/Music
 * family state below is bit-transparent at this Stage-A audio boundary. */
void ubig_stage_a_sp11_dynamic_config(UbigStageACoreConfig *cfg);
const UbigStageAProfileFamilyState *
ubig_stage_a_sp11_profile_family_state(UbigStageAProfileFamily family);
#endif
