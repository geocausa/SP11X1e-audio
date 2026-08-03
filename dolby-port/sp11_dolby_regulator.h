/*
 * sp11_dolby_regulator.h — Dolby regulator decoded from the binary.
 *
 * Block layout mirrors instance + 0xef8 in DolbyAudioProcessing.dll, as
 * recovered from 0x180051b38 (init) and 0x180051cf0 (commit).
 *
 *   +8            band count
 *   +12           dirty flag
 *   +16, +20      param A (16), param B (1)
 *   +28           f_overdrive
 *   +40, +44      relaxation pair (96, 96)
 *   +52, +60      shadow copies of +56, +64
 *   +556 + 4i     low threshold, per band
 *   +636 + 4i     high threshold, per band
 *   +716 + 4i     band flag
 *   +796 + 4i     f_low  (converted)
 *   +876 + 4i     f_high (converted)
 *   +956 + 4i     band flag, active
 *
 * The secondary block at instance + 0x1308 holds two parallel 20-entry
 * int arrays which the commit zeroes in full.
 */

#ifndef SP11_DOLBY_REGULATOR_H
#define SP11_DOLBY_REGULATOR_H

#define SP11_REG_MAX_BANDS 20

typedef struct {
    int a[20];
    int b[20];
} Sp11RegAux;

typedef struct {
    int   nbands;              /* +8   */
    int   dirty;               /* +12  */
    int   param_a, param_b;    /* +16, +20 */
    int   relax_a, relax_b;    /* +40, +44 */
    int   shadow_a, shadow_b;  /* +52, +60 */
    float smooth;              /* 3/65 */

    int   overdrive;
    float f_overdrive;         /* +28  */

    int   timbre_pending, timbre_active;
    float f_timbre;            /* +16 float */

    int   slope_pending, slope_active;
    float f_slope;             /* +40 float */

    const int *centres;

    int   low_threshold [SP11_REG_MAX_BANDS];   /* +556 */
    int   high_threshold[SP11_REG_MAX_BANDS];   /* +636 */
    int   band_flag_pending[SP11_REG_MAX_BANDS];/* +716 */
    float f_low [SP11_REG_MAX_BANDS];           /* +796 */
    float f_high[SP11_REG_MAX_BANDS];           /* +876 */
    int   band_flag_active[SP11_REG_MAX_BANDS]; /* +956 */
} Sp11Reg;

void sp11_reg_init(Sp11Reg *r, int nbands, const int *band_centres);
void sp11_reg_commit(Sp11Reg *r, Sp11RegAux *aux);
void sp11_reg_set_tuning(Sp11Reg *r, int overdrive, int timbre, int relax,
                         int slope, const int *stress, int nstress);

#endif
