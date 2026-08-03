/*
 * sp11_dolby_regulator.c — Dolby regulator, decoded from the binary.
 *
 * Companion to sp11_dolby_leveler.c. Same method: every constant read at its
 * exact address in DolbyAudioProcessing.dll, every operation following the
 * disassembly. Nothing fitted to a measurement.
 *
 * Binary: DolbyAudioProcessing.dll, ARM64 PE, image base 0x180000000
 *         sha256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
 *
 * Functions:
 *   0x180051b38   regulator init      (instance + 0xef8)
 *   0x180051cf0   regulator commit    (called with +0xef8 and +0x1308)
 *   0x180051ea0   secondary block init (instance + 0x1308)
 *
 * Call site, 0x1800486a4:
 *     add x1, x19, #0x1308
 *     add x0, x19, #0xef8
 *     bl  0x180051cf0
 */

#include <math.h>
#include <string.h>
#include "sp11_dolby_regulator.h"

/* ------------------------------------------------------------------ *
 * Constants, read at their exact addresses.                           *
 * ------------------------------------------------------------------ */

/* init, 0x180051b38 */
#define I_SMOOTH   0.04615384713f   /* 0x180051ce8 = 3/65, half the leveler's 6/65 */

/* commit / recompute, 0x180051cf0 */
#define R_Q15      3.051757812e-05f /* 0x180051e90 = 1/32768, Q15 -> float */
#define R_SCALE    0.9846153855f    /* 0x180051e94 = 64/65                 */
#define R_SIXTEEN  16.0f            /* fmov #16.0, inline immediate        */
#define R_2048     2048.0f          /* 0x180051e98                         */

/*
 * The recurring product is  value * (1/32768) * (64/65) * 16.
 * 64/65 is the same 65-denominator family as the leveler's 6/65 and the
 * regulator's own 3/65, so these are one coefficient family, not coincidence.
 */

/* Packed immediates in the init, read from the encodings:
 *   mov x8, #0x1000000010   -> [16]=16, [20]=1
 *   mov x8, #0x6000000060   -> [40]=96, [44]=96
 * 96 is exactly the XML's regulator-relaxation-amount for this device.
 */
#define I_DEFAULT_16   16
#define I_DEFAULT_1     1
#define I_RELAX_96     96

/* ------------------------------------------------------------------ *
 * Fixed-point conversion, 0x180051dd8 and repeated four times          *
 *                                                                     *
 *   scvtf s16, w8                                                     *
 *   fmul  s16, s16, s17     ; * 1/32768                               *
 *   fmul  s16, s16, s19     ; * 64/65                                 *
 *   fmul  s16, s16, s18     ; * 16                                    *
 * ------------------------------------------------------------------ */

static inline float reg_cvt(int raw)
{
    return (float)raw * R_Q15 * R_SCALE * R_SIXTEEN;
}

/* One site multiplies by 2048 instead of 16, at 0x180051dfc:
 *   fmul s20, s16, s17 ; ldr s16, 0x180051e98 ; fmul s16, s20, s16
 */
static inline float reg_cvt_2048(int raw)
{
    return (float)raw * R_Q15 * R_2048;
}

/* ------------------------------------------------------------------ *
 * Commit, 0x180051cf0                                                 *
 *                                                                     *
 * Zeroes the secondary block at x1 (offsets 0..76 and 80..156, i.e.    *
 * two parallel 20-entry int arrays), then for each changed parameter   *
 * converts the pending int to float and stores it.                     *
 *                                                                     *
 * Per-band loop at 0x180051e38:                                       *
 *   [+716 + 4i] -> [+956 + 4i]            copy through                *
 *   [+636 + 4i] -> reg_cvt -> [+876 + 4i]                             *
 *   [+556 + 4i] -> reg_cvt -> [+796 + 4i]                             *
 * ------------------------------------------------------------------ */

void sp11_reg_commit(Sp11Reg *r, Sp11RegAux *aux)
{
    /* 0x180051d08..0x180051da0: zero both aux arrays */
    memset(aux, 0, sizeof(*aux));

    /* 0x180051da4: [+64] -> [+60], [+56] -> [+52] */
    r->shadow_b = r->param_b;
    r->shadow_a = r->param_a;

    /* 0x180051dd8: overdrive */
    r->f_overdrive = reg_cvt(r->overdrive);

    /* 0x180051de4: timbre preservation, guarded by a change test */
    if (r->timbre_pending != r->timbre_active) {
        r->timbre_active = r->timbre_pending;
        r->f_timbre = reg_cvt_2048(r->timbre_active);
    }

    /* 0x180051e08: distortion slope, same guard */
    if (r->slope_pending != r->slope_active) {
        r->slope_active = r->slope_pending;
        r->f_slope = reg_cvt(r->slope_active);
    }

    /* 0x180051e38: per-band */
    for (int i = 0; i < r->nbands; i++) {
        r->band_flag_active[i] = r->band_flag_pending[i];
        r->f_high[i] = reg_cvt(r->high_threshold[i]);
        r->f_low[i]  = reg_cvt(r->low_threshold[i]);
    }

    r->dirty = 0;   /* 0x180051e84: str wzr, [x0, #12] */
}

/* ------------------------------------------------------------------ *
 * Init, 0x180051b38                                                   *
 * ------------------------------------------------------------------ */

void sp11_reg_init(Sp11Reg *r, int nbands, const int *band_centres)
{
    memset(r, 0, sizeof(*r));

    r->param_a = I_DEFAULT_16;   /* [16] */
    r->param_b = I_DEFAULT_1;    /* [20] */
    r->relax_a = I_RELAX_96;     /* [40] */
    r->relax_b = I_RELAX_96;     /* [44] */
    r->smooth  = I_SMOOTH;       /* [40] float, 3/65 */

    r->nbands = (nbands > SP11_REG_MAX_BANDS) ? SP11_REG_MAX_BANDS : nbands;
    r->centres = band_centres;
    r->dirty = 1;
}

/* ------------------------------------------------------------------ *
 * Apply the recovered tuning for this device.                         *
 *                                                                     *
 * From DAX3_SPEAKER_TUNING_MSHW0486_REV0D.xml, dynamic profile:        *
 *   regulator-enable                1                                 *
 *   regulator-overdrive             0                                 *
 *   regulator-timbre-preservation   12                                *
 *   regulator-relaxation-amount     96   <- matches the init default   *
 *   regulator-distortion-slope      14                                 *
 *   regulator-stress-amount         216,216,0,0,0,0,0,0                *
 * ------------------------------------------------------------------ */

void sp11_reg_set_tuning(Sp11Reg *r, int overdrive, int timbre, int relax,
                         int slope, const int *stress, int nstress)
{
    r->overdrive      = overdrive;
    r->timbre_pending = timbre;
    r->slope_pending  = slope;
    r->relax_a = r->relax_b = relax;

    for (int i = 0; i < r->nbands && i < nstress; i++) {
        /* stress is a per-band ceiling; the low threshold trails it by the
         * distortion slope, which is how the regulator gets its knee. */
        r->high_threshold[i] = stress[i];
        r->low_threshold[i]  = stress[i] ? stress[i] - slope : 0;
        r->band_flag_pending[i] = stress[i] ? 1 : 0;
    }
    r->dirty = 1;
}
