#ifndef UBIG_STAGE_A_LIMITER_H
#define UBIG_STAGE_A_LIMITER_H

#include <stdint.h>

#define UBIG_A_LIMITER_CHANNELS 2u
#define UBIG_A_LIMITER_DELAY 64u
#define UBIG_A_LIMITER_FRAMES 256u
#define UBIG_A_LIMITER_HISTORY 16u
#define UBIG_A_LIMITER_RAMP 4u

typedef struct ubig_stage_a_limiter {
    uint32_t delay_pos;
    uint32_t history_pos;
    float envelope_primary;
    float envelope_secondary;
    float current_gain;
    float previous_gain;
    float target_gain;
    float delay[UBIG_A_LIMITER_CHANNELS][UBIG_A_LIMITER_DELAY];
    float peak_history[UBIG_A_LIMITER_HISTORY];
    float predictor_history[UBIG_A_LIMITER_HISTORY];
} ubig_stage_a_limiter;

void ubig_stage_a_limiter_init(ubig_stage_a_limiter *s);
float ubig_stage_a_limiter_process_256_feedback(ubig_stage_a_limiter *s,
                                               float ceiling,
                                               float *left,
                                               float *right);

void ubig_stage_a_limiter_process_256(ubig_stage_a_limiter *s,
                                      float ceiling,
                                      float *left,
                                      float *right);

#endif
