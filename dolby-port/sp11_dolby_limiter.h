/*
 * sp11_dolby_limiter.h — Dolby envelope limiter (FUN_180097228).
 * See sp11_dolby_limiter.c for provenance.
 */

#ifndef SP11_DOLBY_LIMITER_H
#define SP11_DOLBY_LIMITER_H

#define SP11_LIM_BANDS 16

typedef struct {
    float threshold;        /* param_1, linear ceiling */
    float attack;           /* coeffs +0x04 */
    float release;          /* coeffs +0x08 */
    float slow_release;     /* coeffs +0x0c */
    const float *ramp;      /* coeffs +0x10 */
    int   ramp_taps;
    const float *weights;   /* coeffs +0x18, 16 taps */

    float fast_env;         /* state +0x09, persisted */
    float slow_env;         /* state +0x0a, persisted */
    float prev_gain;        /* state +0x0f */
} Sp11Limiter;

void sp11_lim_init(Sp11Limiter *l, float threshold,
                   float attack, float release, float slow_release,
                   const float *weights16, const float *ramp, int ramp_taps);

void sp11_lim_process(Sp11Limiter *l, float *const *chans, int nchan,
                      int nframes, int sub_block);

#endif
