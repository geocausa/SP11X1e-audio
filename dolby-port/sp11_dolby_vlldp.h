/*
 * sp11_dolby_vlldp.h — VLLDP primitives decoded from DolbyAudioProcessing.dll
 * See sp11_dolby_vlldp.c for provenance and the function map.
 */

#ifndef SP11_DOLBY_VLLDP_H
#define SP11_DOLBY_VLLDP_H

/* FUN_180096c50 */
float sp11_vlldp_exp2(float x);
void  sp11_vlldp_db_to_lin(float scale, float *out, const float *in, unsigned n);

/* chain scalars, read at their addresses */
extern const float SP11_VLLDP_LOG_SCALE;      /* 21.59277344   */
extern const float SP11_VLLDP_INV_LOG_SCALE;  /* 0.04631230608 */

#endif
