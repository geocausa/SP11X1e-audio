/*
 * sp11_dolby_vlldp.c — Dolby VLLDP primitives decoded from the binary.
 *
 * Binary: DolbyAudioProcessing.dll, ARM64 PE, image base 0x180000000
 *         sha256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
 *
 * HOW THE AUDIO PATH WAS FOUND
 * ----------------------------
 * The DLL is not stripped: it carries 49 C++ symbols in its logging strings,
 * including source paths. Locating a function is then a matter of finding the
 * adrp/add pair that materialises its name string:
 *
 *   dolby::dapvr::CDapVRModule::Process   string VA 0x180316b10 -> FUN_18001c23c
 *   dolby::dap::VlldpModule::Process      string VA 0x180324d20 -> FUN_18001e1a8
 *   C:\b\0\SampleCodecPack_v2\DolbyAudioProcessing\src\DAPVRModule.cpp
 *
 * FUN_18001e1a8 is a thin wrapper; the real per-block processor is
 * FUN_1800922f8, whose callee list is the chain in execution order:
 *
 *   FUN_18008e970   block setup
 *   FUN_180094100   buffer marshalling (3 call sites)
 *   FUN_180097178   pre-processing
 *   FUN_180098290   per-channel analysis
 *   FUN_180095c20   conditional stage, gated on [0x3ca]
 *   FUN_180096968   conditional stage, gated on [0x49e]
 *   FUN_180095460   per-channel main processing
 *   FUN_180096c50   dB -> linear conversion          <- decoded below
 *   FUN_1800986a8   per-channel apply
 *   FUN_180098c78   cross-channel
 *   FUN_180097228   final gain, writes [0x1bb4]
 *   FUN_180096fd8   output staging
 *   FUN_18008fc60 / FUN_18008fac0   finalisation
 *
 * Structure visible in the processor: 20 bands per channel (0x14), 0x50-byte
 * per-channel blocks, a 3-slot rotating parameter buffer (index % 3), and
 * FUN_180098dd0 computing a scalar from [0x1bb4] that feeds the final gain.
 */

#include <math.h>
#include "sp11_dolby_vlldp.h"

/* ------------------------------------------------------------------ *
 * FUN_180096c50 - fast dB-to-linear (2^x) conversion - RESOLVED       *
 *                                                                     *
 * Signature: (float scale, float *out, float *in, uint count)         *
 *                                                                     *
 *   x   = in[i] * scale                                               *
 *   i   = (int)x            integer part                              *
 *   f   = x - i             fractional part                           *
 *   2^i = (float)((i + 0x7f) << 23)   IEEE-754 exponent written direct*
 *   out = 2^i * (1 + c1*f + c2*f^2 + c3*f^3)                          *
 *                                                                     *
 * The loop is unrolled 4x with a scalar tail; the tail calls a libm    *
 * helper (FUN_1800a5db0) rather than repeating the inline form.       *
 *                                                                     *
 * Constants read at their exact addresses:                            *
 *   0x180096e58 = 0.6862792969   c1                                   *
 *   0x180096e5c = 0.2548217773   c2                                   *
 *   0x180096e60 = 0.05889892578  c3                                   *
 *                                                                     *
 * Verified: this is a cubic fit to 2^f over [0,1) with a maximum       *
 * absolute error of 0.00116. It is not a generic polynomial that       *
 * happens to look like one.                                           *
 * ------------------------------------------------------------------ */

#define VLLDP_EXP2_C1 0.6862792969f
#define VLLDP_EXP2_C2 0.2548217773f
#define VLLDP_EXP2_C3 0.05889892578f

float sp11_vlldp_exp2(float x)
{
    float ip = (float)(int)x;
    float f  = x - ip;

    /* (int)ip + 0x7f) << 23 builds 2^ip by writing the exponent field */
    union { unsigned u; float f; } two_pow;
    two_pow.u = (unsigned)(((int)ip + 0x7f) << 23);

    return two_pow.f * (1.0f + VLLDP_EXP2_C1 * f
                             + VLLDP_EXP2_C2 * f * f
                             + VLLDP_EXP2_C3 * f * f * f);
}

void sp11_vlldp_db_to_lin(float scale, float *out, const float *in, unsigned n)
{
    for (unsigned i = 0; i < n; i++)
        out[i] = sp11_vlldp_exp2(in[i] * scale);
}

/* ------------------------------------------------------------------ *
 * Chain scalars, read at their exact addresses                        *
 *                                                                     *
 *   0x1800940f8 = 0.04631230608   applied to FUN_180098dd0's result   *
 *   0x1800940fc = 21.59277344     passed to FUN_180096c50 as `scale`  *
 *                                                                     *
 * 21.59277344 is BASS_CROSSOVER_INPUT_GAIN, verified bit-exact in     *
 * bass_coefficients.h months ago from a completely separate           *
 * extraction. That the same constant appears here, reached by a       *
 * different route, is independent confirmation the decode is correct. *
 *                                                                     *
 * Note 0.04631230608 is close to 3/64.8; and 1/21.59277344 = 0.046313,*
 * so the two are reciprocals. One converts into the log domain, the   *
 * other back out.                                                     *
 * ------------------------------------------------------------------ */

const float SP11_VLLDP_LOG_SCALE     = 21.59277344f;    /* 0x1800940fc */
const float SP11_VLLDP_INV_LOG_SCALE = 0.04631230608f;  /* 0x1800940f8 */
