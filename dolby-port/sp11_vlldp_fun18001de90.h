/*
 * Exact C helper for the validated DolbyAPOvlldp150!FUN_18001de90 export
 * boundary. Source constants come from sp11_vlldp_v8_runtime_contract.h.
 */
#ifndef SP11_VLLDP_FUN18001DE90_H
#define SP11_VLLDP_FUN18001DE90_H

#include <stdint.h>

#include "sp11_vlldp_v8_runtime_contract.h"

static inline int32_t sp11_vlldp_floor_raw_2080(float value)
{
    float scaled = value * 2080.0f;
    int32_t truncated = (int32_t)scaled;
    return (scaled < (float)truncated) ? truncated - 1 : truncated;
}

static inline float sp11_vlldp_smooth_180021b60(float previous, float target)
{
    if (target < previous) {
        float weighted = (1.0f - sp11_vlldp_fun180021b60_param3_down_alpha) * previous
                       + target * sp11_vlldp_fun180021b60_param3_down_alpha;
        float limited = previous + sp11_vlldp_fun180021b60_param1_downward_step;
        return weighted <= limited ? limited : weighted;
    }

    float weighted = (1.0f - sp11_vlldp_fun180021b60_param2_up_alpha) * previous
                   + target * sp11_vlldp_fun180021b60_param2_up_alpha;
    float limited = target + sp11_vlldp_fun180021b60_param0_validation_value;
    return weighted <= limited ? limited : weighted;
}

static inline void sp11_vlldp_fun18001de90_export(
    const float previous_bbc[SP11_VLLDP_CONTRACT_BANDS],
    const float source_ch0[SP11_VLLDP_CONTRACT_BANDS],
    const float source_ch1[SP11_VLLDP_CONTRACT_BANDS],
    float out_bbc[SP11_VLLDP_CONTRACT_BANDS],
    int32_t out_c0c[SP11_VLLDP_CONTRACT_BANDS])
{
    for (int band = 0; band < SP11_VLLDP_CONTRACT_BANDS; band++) {
        float target = source_ch0[band];
        if (target < source_ch1[band])
            target = source_ch1[band];
        out_bbc[band] = sp11_vlldp_smooth_180021b60(previous_bbc[band], target);
        out_c0c[band] = sp11_vlldp_floor_raw_2080(out_bbc[band]);
    }
}

#endif
