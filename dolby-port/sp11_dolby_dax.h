/*
 * sp11_dolby_dax.h — DAX3 dynamic-profile chain for the Surface Pro 11.
 * Parameters extracted from DAX3_SPEAKER_TUNING_MSHW0486_REV0D.xml.
 */

#ifndef SP11_DOLBY_DAX_H
#define SP11_DOLBY_DAX_H

#define SP11_PEQ_BANDS   20
/* band centres average 0.48 octaves apart */
#define SP11_PEQ_Q       2.9722f
#define SP11_REG_BANDS    8

/* volume-leveler-in/out-target = -320 in 1/16 dB */
#define SP11_LEVELER_TARGET_DB (-320.0f / 16.0f)

/* regulator-stress-amount = 216,216,0,0,0,0,0,0 */
#define SP11_REG_STRESS ((const short[SP11_REG_BANDS]){216,216,0,0,0,0,0,0})

enum { SP11_PROFILE_MUSIC = 0, SP11_PROFILE_DYNAMIC = 1 };

typedef struct { float b0,b1,b2,a1,a2,z1,z2; } Sp11Bq;

typedef struct {
    float target, amount, gain, env, atk, rel, max_gain, min_gain, makeup;
    int   drc;
} Sp11Leveler;

typedef struct {
    Sp11Bq split[SP11_REG_BANDS];
    float  ceiling[SP11_REG_BANDS];
    float  gain[SP11_REG_BANDS];
    Sp11Bq lp;
    float  env, atk, rel, timbre;
} Sp11Regulator;

typedef struct {
    float         sr;
    int           profile, peq_on, ieq_on, ieq_amount;
    Sp11Bq        peq[2][SP11_PEQ_BANDS];
    Sp11Leveler   lev[2];
    Sp11Regulator reg[2];
} Sp11Dax;

extern const short SP11_PEQ_CH0[SP11_PEQ_BANDS];
extern const short SP11_PEQ_CH1[SP11_PEQ_BANDS];

void sp11_dax_init(Sp11Dax *d, float sample_rate, int profile);
void sp11_dax_process(Sp11Dax *d, float *const *ch, int nframes);

#endif
