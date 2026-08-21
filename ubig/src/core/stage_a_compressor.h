#ifndef UBIG_STAGE_A_COMPRESSOR_H
#define UBIG_STAGE_A_COMPRESSOR_H
#include <stdint.h>
#include "stage_a_compressor_worker.h"

/* Warm-state Stage-A compressor composition. Returns 0 when the cached state
 * already matches the incoming mode/channel geometry, or -1 when lifecycle
 * reinitialization is required before processing. */
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
                                    uint32_t *matrix_rows_out);

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
                                         uint32_t *matrix_rows_out);
#endif
