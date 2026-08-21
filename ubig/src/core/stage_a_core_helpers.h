#ifndef UBIG_STAGE_A_CORE_HELPERS_H
#define UBIG_STAGE_A_CORE_HELPERS_H
#include <stdint.h>
#include "stage_a_compressor_worker.h"

/* Build the two 20-band row families consumed by the Stage-A regulator/
 * compressor chain. `base` is the independently tuned companion row.
 * When analysis_enable is zero, main is a copy of base. */
void ubig_stage_a_build_rows(uint32_t channels,
                             uint32_t analysis_enable,
                             const struct ubig_float_rows *analyzed,
                             const struct ubig_float_rows *base,
                             const struct ubig_float_rows *main_rows,
                             const struct ubig_float_rows *companion_rows);

/* Convert one channel's current 20-band log-domain synthesis row into the
 * active two-phase gain slot. */
void ubig_stage_a_store_phase_gains(float gains[40],
                                    uint32_t phase_index,
                                    const float log_gain20[20]);

/* Reproduce the six-block startup/configuration self-crossfade performed by
 * the reference core. Even with identical source/target samples this exact
 * multiply + FMA sequence can move a float by one ULP. */
void ubig_stage_a_startup_blend_self(float samples[256], uint32_t block_index);
#endif
