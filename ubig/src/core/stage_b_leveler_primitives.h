#ifndef UBIG_STAGE_B_LEVELER_PRIMITIVES_H
#define UBIG_STAGE_B_LEVELER_PRIMITIVES_H
#include <stdint.h>

/* Exact coefficient mapper used by the SP11 Stage-B Volume-Leveler/DRC
 * adaptive controller. config[1]/config[2] are the two branch coefficients;
 * config[0] is not read by this bounded primitive. */
void ubig_stage_b_leveler_coeff_triplet(uint32_t mode,
                                        const float config[3],
                                        float blend,
                                        float history,
                                        float drive,
                                        float *out_a,
                                        float *out_b,
                                        float *out_adaptive);
/* Exact 80-slot adaptive-history accumulator used by the Leveler/DRC writer. */
typedef struct {
    float bins[51];
    float total;
    uint32_t count;
    uint32_t ring_bin[80];
    float ring_lo[80];
    float ring_hi[80];
    float ring_total[80];
    uint32_t ring_pos;
    float phase;
    uint32_t reset_max;
    float max_a;
    float max_b;
} UbigStageBLevelerHistory;

void ubig_stage_b_leveler_history_update(UbigStageBLevelerHistory *state,
                                         float step,
                                         float value_a,
                                         float value_b);
/* Exact 17-float post-controller scalar-transfer curve builder/evaluator.
 * The builder updates only the dynamic fields owned by the reference helper;
 * caller-owned threshold/static fields remain untouched. */
void ubig_stage_b_leveler_curve_build(float curve[17],
                                      float anchor,
                                      float slope_control,
                                      float delta);
float ubig_stage_b_leveler_piecewise(const float curve[17], float input);
#endif
