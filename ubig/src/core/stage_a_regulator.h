#ifndef UBIG_STAGE_A_REGULATOR_H
#define UBIG_STAGE_A_REGULATOR_H

#include <stdint.h>
#include "stage_a_compressor_worker.h"

/* Adaptive updater used by the grouped Stage-A regulator. The source and
 * destination tuning blocks are exactly 0xa4 bytes. */
void ubig_stage_a_regulator_adapt(void *state,
                                  float drive,
                                  float observed_max,
                                  const void *source_tuning,
                                  void *working_tuning);

void ubig_stage_a_monotone_cubic(const int32_t *knots_x,
                                  const float *knots_y,
                                  uint32_t knot_count,
                                  const int32_t *query_x,
                                  float *output,
                                  uint32_t output_count);

void ubig_stage_a_regulator_expand(void *state,
                                   const void *working_tuning,
                                   uint32_t group_count,
                                   uint32_t channel,
                                   float output20[20]);

void ubig_stage_a_regulator_process(float drive,
                                    float slow_mix,
                                    void *state,
                                    const void *source_tuning,
                                    uint32_t adaptive_enable,
                                    uint32_t slow_enable,
                                    const struct ubig_float_rows *main_rows,
                                    const struct ubig_float_rows *secondary_rows,
                                    int32_t *input_telemetry,
                                    int32_t *group_telemetry,
                                    int32_t *expanded_telemetry);

#endif
