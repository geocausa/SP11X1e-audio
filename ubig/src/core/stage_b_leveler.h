#ifndef UBIG_STAGE_B_LEVELER_H
#define UBIG_STAGE_B_LEVELER_H
#include <stdint.h>
#include "stage_b_leveler_primitives.h"

typedef struct {
    float *values;
    float scalar;
    uint32_t reserved;
} UbigStageBLevelerRecord;

typedef struct {
    float history_step;
    float coeff_a;
    float coeff_b;
    uint32_t hold_limit;
    float base_decay;
    float adaptive_smooth;
} UbigStageBLevelerConfig;

typedef struct {
    float base;
    uint32_t hold_count;
    float adaptive_state;
    uint32_t reserved;
    UbigStageBLevelerRecord *primary;
    UbigStageBLevelerRecord *secondary;
    UbigStageBLevelerHistory history;
} UbigStageBLevelerState;

/* Process one indexed adaptive record and its preceding/related vector records.
 * observed_records supplies read-only instantaneous targets. */
void ubig_stage_b_leveler_update(UbigStageBLevelerState *state,
                                 const UbigStageBLevelerConfig *config,
                                 uint32_t index,
                                 uint32_t width,
                                 float control_mix,
                                 float direct_control,
                                 float secondary_scale,
                                 const UbigStageBLevelerRecord *observed_records);
#endif
