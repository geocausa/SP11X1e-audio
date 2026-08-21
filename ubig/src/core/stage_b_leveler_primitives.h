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
#endif
