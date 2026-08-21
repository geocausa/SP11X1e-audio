#ifndef UBIG_STAGE_A_COMPRESSOR_PRIMITIVES_H
#define UBIG_STAGE_A_COMPRESSOR_PRIMITIVES_H

#include <stdint.h>

struct ubig_dual_floor_state {
    const float *config;
    uint32_t count;
    float primary[20];
    float secondary[20];
};

struct ubig_flag_state {
    const float *config;
    uint32_t count;
    uint32_t phase;
};

struct ubig_scalar_state {
    const float *config;
    uint32_t count;
    float value;
};

struct ubig_directional_smoother {
    const float *coeff;
    uint32_t count;
    float value[20];
};

void ubig_comp_dual_floor_init(struct ubig_dual_floor_state *s,
                               const float *config, uint32_t count);
void ubig_comp_flag_state_init(struct ubig_flag_state *s,
                               const float *config, uint32_t count);
void ubig_comp_scalar_state_init(struct ubig_scalar_state *s,
                                 const float *config, uint32_t count);
void *ubig_comp_scalar_payload(void *state_base);
void ubig_comp_uniform_state_init(struct ubig_directional_smoother *s,
                                  const float *config, uint32_t count);
void ubig_comp_directional_smooth(struct ubig_directional_smoother *s,
                                  const int32_t *direction_flags,
                                  const float *target);


void ubig_comp_slow_gain_bounds(struct ubig_scalar_state *s,
                                const float *input_a,
                                const float *input_b,
                                float *lower,
                                float *upper,
                                const float *mix,
                                float drive,
                                float bias,
                                float deviation_a,
                                float deviation_b,
                                float mix_level);

void ubig_comp_nonlinear_correction(struct ubig_scalar_state *s,
                                    const float *input,
                                    const float *floor,
                                    const float *ceiling,
                                    const int32_t *mask,
                                    float *half_drive,
                                    float *correction,
                                    float aggregate_scale,
                                    float common_drive);

void ubig_comp_linked_deviation(const int32_t *mask,
                                const float *reference,
                                uint32_t count,
                                float *upper,
                                float *lower,
                                float blend);

void ubig_comp_neighbor_limit(uint32_t count,
                              const int32_t *mask,
                              const float *input,
                              float *output);

void ubig_comp_band_state_init(void *state, const void *config, uint32_t count);

float ubig_comp_soft_max(float a, float b);

void ubig_comp_band_state_update(void *state,
                                 const float *activity,
                                 uint32_t gate_a,
                                 uint32_t gate_b,
                                 float weight,
                                 float ratio_gate,
                                 float ratio_margin,
                                 float target);

#endif
