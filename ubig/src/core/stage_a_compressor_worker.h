#ifndef UBIG_STAGE_A_COMPRESSOR_WORKER_H
#define UBIG_STAGE_A_COMPRESSOR_WORKER_H
#include <stdint.h>

struct ubig_float_rows {
    uint32_t count;
    uint32_t reserved;
    float *const *rows;
};

float ubig_comp_transition5(const float config[5], float previous, float target);

void ubig_comp_rise_gate_worker(void *state,
                                const struct ubig_float_rows *rows,
                                const float *matrix20,
                                uint32_t matrix_rows,
                                uint32_t *matrix_gate,
                                uint32_t *aggregate_gate);
void ubig_comp_band_controller(void *state,
                               const float *activity,
                               const struct ubig_float_rows *rows,
                               const float *matrix20,
                               uint32_t matrix_rows,
                               float severity_drive,
                               float activity_weight,
                               float ratio_gate,
                               float ratio_margin);

#endif
