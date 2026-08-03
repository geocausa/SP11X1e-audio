/*
 * sp11_dolby_stage1.h — 4-band crossover interface.
 * See sp11_dolby_stage1.c for why this replaces bass_standalone.c's version.
 */

#ifndef SP11_DOLBY_STAGE1_H
#define SP11_DOLBY_STAGE1_H

/* bit-exact from .rdata, verified in VERIFICATION_LOG.md */
#define SP11_XOVER_INPUT_GAIN 21.5927734375f

typedef struct {
    float b0, b1, b2, a1, a2;
    float z1, z2;
} Sp11Biquad;

typedef struct {
    float      sample_rate;
    float      input_gain;
    Sp11Biquad low[2];    /* LR4 lowpass  = 2 cascaded Butterworth */
    Sp11Biquad high[2];   /* LR4 highpass = 2 cascaded Butterworth */
} Sp11Crossover;

void sp11_xover_init(Sp11Crossover *xb, float sample_rate);
void sp11_xover_process(Sp11Crossover *xb, const float *in,
                        float *out_low, float *out_high, int n);

#endif /* SP11_DOLBY_STAGE1_H */
