#ifndef UBIG_STAGE_A_EXPORT_H
#define UBIG_STAGE_A_EXPORT_H
#include <stdint.h>
#define UBIG_STAGE_A_BANDS 20u
void ubig_stage_a_export(const float *previous,
                         const float *sources,
                         unsigned channels,
                         float out_state[UBIG_STAGE_A_BANDS],
                         int32_t out_raw[UBIG_STAGE_A_BANDS]);
#endif
