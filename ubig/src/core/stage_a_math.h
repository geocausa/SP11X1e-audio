#ifndef UBIG_STAGE_A_MATH_H
#define UBIG_STAGE_A_MATH_H
#include <stdint.h>
float ubig_stage_a_log2_approx(float x);
void ubig_stage_a_exp2_scaled(float *out,const float *in,uint32_t count,float scale);
#endif
