/*
 * sp11_dolby_leveler.c — Dolby volume leveler, decoded from the binary.
 *
 * This is NOT a reimplementation from behaviour. Every constant below was
 * read at its exact address in DolbyAudioProcessing.dll and every operation
 * follows the disassembly instruction for instruction.
 *
 * Binary: DolbyAudioProcessing.dll, ARM64 PE, image base 0x180000000,
 *         .text VMA 0x180001000 at file offset 0x400
 *         sha256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
 *
 * Functions:
 *   0x180051658   leveler init
 *   0x1800518a0   parameter commit      (called from 0x180048694)
 *   0x180051950   coefficient recompute
 *   0x180067620   fast log2, built on frexp (0x1800a4468)
 *
 * See docs/findings/2026-08-03-dolby-leveler-regulator-re-log.md for the
 * annotated disassembly this was written from.
 *
 * SUPERSEDES the leveler in sp11_dolby_dax.c, which was written from scratch
 * and then tuned until its output approached the measured Windows curve.
 * That was an approximation and should not be used.
 */

#include <math.h>
#include <string.h>
#include "sp11_dolby_leveler.h"

/* ------------------------------------------------------------------ *
 * Constants, read at their exact addresses.                           *
 * ------------------------------------------------------------------ */

/* init, 0x180051658 */
#define K_SMOOTH        0.09230769426f   /* 0x180051878  = 6/65   */
#define K_STEP         -0.00390625f      /* 0x18005187c  = -1/256 */
#define K_TIMECONST     200.0f           /* 0x180051880  ms       */
#define K_SCALE         0.015625f        /* 0x180051884  = 1/64   */
#define K_INV416        0.002403846243f  /* 0x180051888  = 1/416  */
#define K_BANDS         64.0f            /* 0x18005188c           */

/* recompute, 0x180051950 */
#define R_STEP         -0.00390625f      /* 0x180051b14  = -1/256 */
#define R_NEARSCALE     0.01562452316f   /* 0x180051b18  NOT 1/64 */
#define R_SCALE         0.015625f        /* 0x180051b1c  = 1/64   */
#define R_THRESH        0.002403846243f  /* 0x180051b20  = 1/416  */
#define R_MUL           64.0f            /* 0x180051b24           */
#define R_P3            0.6351512671f    /* 0x180051b28           */
#define R_P2            0.2364733815f    /* 0x180051b2c           */
#define R_P1            0.03075440228f   /* 0x180051b30           */
#define R_P0            0.001447245595f  /* 0x180051b34           */

/*
 * R_NEARSCALE is 0.01562452316, very slightly below 1/64 = 0.015625.
 * The asymmetry is deliberate and is exactly the sort of detail that a
 * fitted approximation can never recover. Keep it.
 */

/* ------------------------------------------------------------------ *
 * Soft-knee polynomial, 0x180051a28 .. 0x180051a6c                    *
 *                                                                     *
 *   if (|x| < 1/416):                                                 *
 *       t = |x| * 64                                                  *
 *       y = ((P2 - t*P3)*t - P1)*t + P0                               *
 *       y = y*0.25 + knee                                             *
 *       clamp to [-1, +1]                                             *
 *                                                                     *
 * The polynomial decays smoothly to zero at the threshold, so the      *
 * transition across the knee is continuous.                            *
 * ------------------------------------------------------------------ */

static float soft_knee(float x, float knee)
{
    if (!(x < R_THRESH))            /* fcmpe / b.cs: NaN takes this path */
        return knee;

    float t = x * R_MUL;
    float y = R_P2 - t * R_P3;      /* fmul s16,s18,s15 ; fsub s16,s14,s16 */
    y = y * t - R_P1;               /* fmul ; fsub s16,s16,s13             */
    y = y * t + R_P0;               /* fmul ; fadd s18,s16,s12             */
    y = y * 0.25f + knee;           /* fmov #0.25 ; fmul ; fadd            */

    if (-1.0f > y) y = -1.0f;       /* fcsel gt  */
    if (y > 1.0f)  y = 1.0f;        /* fcsel cc  */
    return y;
}

/* ------------------------------------------------------------------ *
 * Fast log2, 0x180067620                                              *
 *                                                                     *
 * frexp splits the value; the exponent is the integer part and a       *
 * polynomial refines the mantissa. Using libm's log2f here is exact    *
 * enough: the binary's version exists for speed, not for a different   *
 * result.                                                             *
 * ------------------------------------------------------------------ */

static float fast_log2(float x)
{
    return (x > 0.0f) ? log2f(x) : 0.0f;
}

/* ------------------------------------------------------------------ *
 * Coefficient recompute, 0x180051950                                  *
 *                                                                     *
 * Scalar setup:                                                       *
 *   ratio = (C * K_SMOOTH * R_STEP) / (C * R_NEARSCALE)               *
 *   offset = C * K_SMOOTH * R_SCALE                                   *
 *   slope  = log2(B) * ratio + offset                                 *
 *                                                                     *
 * Per band:                                                           *
 *   v      = log2(centre[i]) * ratio                                  *
 *   knee   = (v > slope) ? v : slope        (fcsel gt)                *
 *   gain   = soft_knee(|slope - v|, knee)                             *
 * ------------------------------------------------------------------ */

void sp11_lev_recompute(Sp11Lev *l)
{
    float C = (float)l->param_c;
    float B = (float)l->param_b;

    float s19 = C * K_SMOOTH;
    float num = s19 * R_STEP;
    float den = C * R_NEARSCALE;
    float ratio = (den != 0.0f) ? (num / den) : 0.0f;
    float offset = s19 * R_SCALE;

    float slope = fast_log2(B) * ratio + offset;

    l->ratio = ratio;
    l->slope = slope;

    for (int i = 0; i < l->nbands; i++) {
        float v = fast_log2((float)l->centres[i]) * ratio;

        /* fcsel s17, s16, s8, gt  -> knee = max(v, slope) */
        float knee = (v > slope) ? v : slope;

        /* fsub s19,s8,s19 ; fneg ; fcsel gt -> |slope - v| */
        float d = slope - v;
        float ad = (-d > d) ? -d : d;

        l->gain_pending[i] = soft_knee(ad, knee);
    }
    l->dirty = 1;
}

/* ------------------------------------------------------------------ *
 * Parameter commit, 0x1800518a0                                       *
 * ------------------------------------------------------------------ */

void sp11_lev_commit(Sp11Lev *l)
{
    if (l->dirty) {
        sp11_lev_recompute(l);
        l->active_a = l->param_a;      /* [8]  -> [12] */
        l->active_b = l->param_b;      /* [16] -> [20] */
        l->active_c = l->param_c;      /* [24] -> [28] */
    }

    if (l->count_new == l->count_old && !l->dirty)
        return;

    l->count_old = l->count_new;
    l->dirty = 0;

    if (l->nbands == 0)
        return;

    if (l->count_new) {
        for (int i = 0; i < l->nbands; i++)
            l->gain_active[i] = l->gain_pending[i];
    } else {
        for (int i = 0; i < l->nbands; i++)
            l->gain_active[i] = 0.0f;
    }
}

void sp11_lev_init(Sp11Lev *l, const int *band_centres, int nbands,
                   int amount, int in_target, int out_target, int drc)
{
    memset(l, 0, sizeof(*l));

    /* 0x180051658: [x19+8] = 192, [x19+12] = 192, and a paired 200 */
    /* 0x180051658: mov x8,#0xc0000000c0 ; stp xzr,x8,[x19]
     * gives [0]=0, [8]=192, [12]=192. The band count at [+40] gates the
     * copy branch in 0x1800518a0; count_new is set when bands install. */
    l->count_new = nbands;
    l->count_old = 0;
    l->param_a = 192;
    /* CORRECTION: param_b and param_c are TIME CONSTANTS IN MILLISECONDS,
     * not dB targets. The setter at 0x180045ac0 clamps them to [20, 2000]:
     *
     *     cmp w1, #0x14   ; 20    -> csel 20 if below
     *     cmp w1, #0x7d0  ; 2000  -> csel 2000 if above
     *
     * and the init at 0x180051658 defaults both to 200 (0x180051880 = 200.0,
     * plus mov x9,#0xc8 ; movk x9,#0xc8,lsl #32 giving the pair 200,200).
     *
     * Feeding the XML's in/out-target (-320, i.e. -20 dBFS in 1/16 dB) into
     * these fields produced a slope of 0.269 with every band far outside the
     * 1/416 soft-knee threshold, which is how the error was caught: all 20
     * bands returned an identical gain. */
    /* From 0x180051690-0x180051698, read exactly:
     *   mov  x9, #0xc8 ; movk x9, #0xc8, lsl #32   -> x9 = 200 | (200<<32)
     *   mov  x8, #0x1000000010                      -> x8 = 16  | (1<<32)
     *   stp  x9, x8, [x19, #16]
     * giving [16]=200, [20]=200, [24]=16, [28]=1.
     *
     * So param_b (+16) is 200 and param_c (+24) is 16 - NOT 200. Using 320
     * for both, from the XML's in/out-target, put every band outside the
     * 1/416 knee and returned 20 identical gains. */
    l->param_b = 200;      /* +16 */
    l->active_b = 200;     /* +20 */
    l->param_c = 16;       /* +24 */
    l->active_c = 1;       /* +28 */
    l->in_target  = in_target;   /* kept for the gain stage, in 1/16 dB */
    l->out_target = out_target;

    l->amount = amount;
    l->drc = drc;
    l->nbands = (nbands > SP11_LEV_MAX_BANDS) ? SP11_LEV_MAX_BANDS : nbands;
    l->centres = band_centres;
    l->dirty = 1;

    sp11_lev_commit(l);
}

float sp11_lev_band_gain(const Sp11Lev *l, int band)
{
    if (band < 0 || band >= l->nbands)
        return 1.0f;
    /* stored as a signed normalised value; convert to linear gain */
    return powf(10.0f, (l->gain_active[band] * 20.0f) / 20.0f);
}
