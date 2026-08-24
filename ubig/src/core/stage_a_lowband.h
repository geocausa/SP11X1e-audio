#ifndef UBIG_STAGE_A_LOWBAND_H
#define UBIG_STAGE_A_LOWBAND_H
#include <stdint.h>
#include "stage_a_compressor_worker.h"

struct ubig_stage_a_lowband_config {
    uint32_t band_count;
    float rise_step;
    float fall_step;
    float threshold;
    float boundary;
    float gain[5];
};

/* Persistent state image is 0x6c bytes: raw channel levels [8], active count,
 * smoothed channel levels [8], then an exact copy of the 40-byte config. */
void ubig_stage_a_lowband_process(void *state,
                                  uint32_t active_channels,
                                  const struct ubig_stage_a_lowband_config *config,
                                  const struct ubig_float_rows *input,
                                  const struct ubig_float_rows *output,
                                  int32_t telemetry[8][20]);
#endif
