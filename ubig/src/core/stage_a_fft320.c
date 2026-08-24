#include "stage_a_fft320.h"
#include "stage_a_fft320_tables.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define UBIG_FFT320_N 320u
#define UBIG_FFT320_FLOATS (UBIG_FFT320_N * 2u)

static inline float ubig_mla(float acc, float a, float b)
{
    return fmaf(a, b, acc);
}

static inline float ubig_mls(float acc, float a, float b)
{
    return fmaf(-a, b, acc);
}

/*
 * First Stockham radix-4 pass. The input is 320 interleaved complex values.
 * Each quarter-spaced quartet becomes one 8-float SoA packet:
 *   Re(Y0..Y3), Im(Y0..Y3).
 */
static void ubig_fft320_radix4_entry(float out[UBIG_FFT320_FLOATS],
                                    const float in[UBIG_FFT320_FLOATS])
{
    for (unsigned k = 0; k < 80u; ++k) {
        const unsigned ia = 2u * k;
        const unsigned ib = 2u * (k + 80u);
        const unsigned ic = 2u * (k + 160u);
        const unsigned id = 2u * (k + 240u);
        const float ar = in[ia], ai = in[ia + 1u];
        const float br = in[ib], bi = in[ib + 1u];
        const float cr = in[ic], ci = in[ic + 1u];
        const float dr = in[id], di = in[id + 1u];

        const float r0 = ar + cr;
        const float r1 = br + dr;
        const float r2 = ar - cr;
        const float ti = bi - di;
        const float y2r = r0 - r1;
        const float y0r = r0 + r1;
        const float brd = br - dr;
        const float y3r = r2 - ti;
        const float y1r = r2 + ti;

        const float i0 = ai + ci;
        const float i1 = bi + di;
        const float idf = ai - ci;
        const float y2i = i0 - i1;
        const float y0i = i0 + i1;
        const float y3i = idf + brd;
        const float y1i = idf - brd;

        float *o = out + 8u * k;
        o[0] = y0r; o[1] = y1r; o[2] = y2r; o[3] = y3r;
        o[4] = y0i; o[5] = y1i; o[6] = y2i; o[7] = y3i;
    }
}

/*
 * Generic middle Stockham radix-4 pass. `step` is 4 or 16 for the 320-point
 * kernel. The twiddle table is generated from ordinary roots of unity.
 */
static void ubig_fft320_radix4_stage(float out[UBIG_FFT320_FLOATS],
                                    const float src[UBIG_FFT320_FLOATS],
                                    const float *twiddle,
                                    unsigned step)
{
    const unsigned quarter = 80u;
    const unsigned half = 160u;
    const unsigned s2 = step * 2u;
    const unsigned s4 = step * 4u;
    const unsigned s6 = step * 6u;
    unsigned src_base = 0u;
    unsigned outer = 0u;
    unsigned remaining = quarter;

    while (remaining != 0u) {
        for (unsigned inner = 0u; inner < step; inner += 4u) {
            const unsigned off = inner * 2u;
            for (unsigned lane = 0u; lane < 4u; ++lane) {
                const unsigned a0 = src_base + off + lane;
                const unsigned a1 = src_base + 4u + off + lane;
                const unsigned b0 = src_base + half + off + lane;
                const unsigned b1 = src_base + half + 4u + off + lane;
                const unsigned c0 = src_base + 2u * half + off + lane;
                const unsigned c1 = src_base + 2u * half + 4u + off + lane;
                const unsigned d0 = src_base + 3u * half + off + lane;
                const unsigned d1 = src_base + 3u * half + 4u + off + lane;

                const float tw0 = twiddle[off + lane];
                const float tw1 = twiddle[4u + off + lane];
                const float tw20 = twiddle[s2 + off + lane];
                const float tw21 = twiddle[s2 + 4u + off + lane];
                const float tw30 = twiddle[s4 + off + lane];
                const float tw31 = twiddle[s4 + 4u + off + lane];

                const float rb = ubig_mls(src[b0] * tw0, src[b1], tw1);
                const float ib = ubig_mla(src[b1] * tw0, src[b0], tw1);
                const float ic = ubig_mla(src[c1] * tw20, src[c0], tw21);
                const float rc = ubig_mls(src[c0] * tw20, src[c1], tw21);
                const float rd = ubig_mls(src[d0] * tw30, src[d1], tw31);
                const float id = ubig_mla(src[d1] * tw30, src[d0], tw31);

                const float a1_minus_ic = src[a1] - ic;
                const float a0_plus_rc = src[a0] + rc;
                const float a0_minus_rc = src[a0] - rc;
                const float r_sum = rb + rd;
                const float r_diff = rb - rd;
                const float a1_plus_ic = src[a1] + ic;
                const float i_sum = ib + id;
                const float i_diff = ib - id;

                out[outer + off + lane] = a0_plus_rc + r_sum;
                out[outer + 4u + off + lane] = a1_plus_ic + i_sum;
                out[outer + s2 + off + lane] = a0_minus_rc + i_diff;
                out[outer + s2 + 4u + off + lane] = a1_minus_ic - r_diff;
                out[outer + s4 + off + lane] = a0_plus_rc - r_sum;
                out[outer + s4 + 4u + off + lane] = a1_plus_ic - i_sum;
                out[outer + s6 + off + lane] = a0_minus_rc - i_diff;
                out[outer + s6 + 4u + off + lane] = a1_minus_ic + r_diff;
            }
        }
        remaining -= step;
        src_base += step * 2u;
        outer += step * 8u;
    }
}

static void ubig_fft320_radix5_unscaled(float data[UBIG_FFT320_FLOATS])
{
    float src[UBIG_FFT320_FLOATS];
    memcpy(src, data, sizeof(src));
    const unsigned span = 128u;
    const float c0 = ubig_fft320_radix5_c0;
    const float c1 = ubig_fft320_radix5_c1;
    const float c2 = ubig_fft320_radix5_c2;
    const float c3 = ubig_fft320_radix5_c3;

    for (unsigned base = 0u; base < span; base += 8u) {
        for (unsigned lane = 0u; lane < 4u; ++lane) {
            const float a0 = src[base + lane];
            const float a1 = src[base + 4u + lane];
            const float b0 = src[base + span + lane];
            const float b1 = src[base + span + 4u + lane];
            const float cc0 = src[base + 2u * span + lane];
            const float cc1 = src[base + 2u * span + 4u + lane];
            const float d0 = src[base + 3u * span + lane];
            const float d1 = src[base + 3u * span + 4u + lane];
            const float e0 = src[base + 4u * span + lane];
            const float e1 = src[base + 4u * span + 4u + lane];

            const float tb0 = ubig_fft320_final_twiddle[base + lane];
            const float tb1 = ubig_fft320_final_twiddle[base + 4u + lane];
            const float tc0 = ubig_fft320_final_twiddle[base + span + lane];
            const float tc1 = ubig_fft320_final_twiddle[base + span + 4u + lane];
            const float td0 = ubig_fft320_final_twiddle[base + 2u * span + lane];
            const float td1 = ubig_fft320_final_twiddle[base + 2u * span + 4u + lane];
            const float te0 = ubig_fft320_final_twiddle[base + 3u * span + lane];
            const float te1 = ubig_fft320_final_twiddle[base + 3u * span + 4u + lane];

            const float rb = ubig_mls(b0 * tb0, b1, tb1);
            const float ib = ubig_mla(b1 * tb0, b0, tb1);
            const float rc = ubig_mls(cc0 * tc0, cc1, tc1);
            const float ic = ubig_mla(cc1 * tc0, cc0, tc1);
            const float rd = ubig_mls(d0 * td0, d1, td1);
            const float id = ubig_mla(d1 * td0, d0, td1);
            const float re = ubig_mls(e0 * te0, e1, te1);
            const float ie = ubig_mla(e1 * te0, e0, te1);

            const float rbm = rb - re;
            const float ibm = ib - ie;
            const float rbp = rb + re;
            const float ibp = ib + ie;
            const float rcm = rc - rd;
            const float icm = ic - id;
            const float rcp = rc + rd;
            const float icp = ic + id;

            const float x0 = ubig_mls(rbm * c0, rcm, c1);
            const float x1 = ubig_mls(ibm * c0, icm, c1);
            const float y0 = ubig_mla(ibm * c1, icm, c0);
            const float y1 = ubig_mla(rbm * c1, rcm, c0);
            const float z0 = ubig_mla(rbp * c2, rcp, c3);
            const float z1 = ubig_mla(rbp * c3, rcp, c2);
            const float w0 = ubig_mla(ibp * c2, icp, c3);
            const float w1 = ubig_mla(ibp * c3, icp, c2);

            const float sum_r = rbp + rcp;
            const float sum_i = ibp + icp;
            const float p1r = z0 + a0;
            const float p1i = w0 + a1;
            const float p2r = z1 + a0;
            const float p2i = w1 + a1;

            data[base + 2u * lane] = sum_r + a0;
            data[base + 2u * lane + 1u] = sum_i + a1;
            data[base + span + 2u * lane] = p1r + y0;
            data[base + span + 2u * lane + 1u] = p1i - y1;
            data[base + 2u * span + 2u * lane] = p2r + x1;
            data[base + 2u * span + 2u * lane + 1u] = p2i - x0;
            data[base + 3u * span + 2u * lane] = p2r - x1;
            data[base + 3u * span + 2u * lane + 1u] = p2i + x0;
            data[base + 4u * span + 2u * lane] = p1r - y0;
            data[base + 4u * span + 2u * lane + 1u] = p1i + y1;
        }
    }
}

/* Analysis kernel: the reference folds 1/320 into the final radix-5 input. */
static void ubig_fft320_radix5_scaled(float data[UBIG_FFT320_FLOATS])
{
    float src[UBIG_FFT320_FLOATS];
    memcpy(src, data, sizeof(src));
    const unsigned span = 128u;
    const float scale = 0x1.99999ap-9f;
    const float c0 = ubig_fft320_radix5_c0;
    const float c1 = ubig_fft320_radix5_c1;
    const float c2 = ubig_fft320_radix5_c2;
    const float c3 = ubig_fft320_radix5_c3;

    for (unsigned base = 0u; base < span; base += 8u) {
        for (unsigned lane = 0u; lane < 4u; ++lane) {
            const float a0 = src[base + lane] * scale;
            const float a1 = src[base + 4u + lane] * scale;
            const float b0 = src[base + span + lane] * scale;
            const float b1 = src[base + span + 4u + lane] * scale;
            const float cc0 = src[base + 2u * span + lane] * scale;
            const float cc1 = src[base + 2u * span + 4u + lane] * scale;
            const float d0 = src[base + 3u * span + lane] * scale;
            const float d1 = src[base + 3u * span + 4u + lane] * scale;
            const float e0 = src[base + 4u * span + lane] * scale;
            const float e1 = src[base + 4u * span + 4u + lane] * scale;

            const float tb0 = ubig_fft320_final_twiddle[base + lane];
            const float tb1 = ubig_fft320_final_twiddle[base + 4u + lane];
            const float tc0 = ubig_fft320_final_twiddle[base + span + lane];
            const float tc1 = ubig_fft320_final_twiddle[base + span + 4u + lane];
            const float td0 = ubig_fft320_final_twiddle[base + 2u * span + lane];
            const float td1 = ubig_fft320_final_twiddle[base + 2u * span + 4u + lane];
            const float te0 = ubig_fft320_final_twiddle[base + 3u * span + lane];
            const float te1 = ubig_fft320_final_twiddle[base + 3u * span + 4u + lane];

            const float rb = ubig_mls(b0 * tb0, b1, tb1);
            const float ib = ubig_mla(b1 * tb0, b0, tb1);
            const float rc = ubig_mls(cc0 * tc0, cc1, tc1);
            const float ic = ubig_mla(cc1 * tc0, cc0, tc1);
            const float rd = ubig_mls(d0 * td0, d1, td1);
            const float id = ubig_mla(d1 * td0, d0, td1);
            const float re = ubig_mls(e0 * te0, e1, te1);
            const float ie = ubig_mla(e1 * te0, e0, te1);

            const float c_plus_d_i = ic + id;
            const float c_minus_d_r = rc - rd;
            const float b_minus_e_r = rb - re;
            const float b_plus_e_r = rb + re;
            const float b_minus_e_i = ib - ie;
            const float b_plus_e_i = ib + ie;
            const float c_minus_d_i = ic - id;
            const float c_plus_d_r = rc + rd;

            const float x0 = ubig_mls(b_minus_e_i * c0, c_minus_d_i, c1);
            const float y0 = ubig_mla(b_minus_e_i * c1, c_minus_d_i, c0);
            const float x1 = ubig_mls(b_minus_e_r * c0, c_minus_d_r, c1);
            const float y1 = ubig_mla(b_minus_e_r * c1, c_minus_d_r, c0);
            const float z0 = ubig_mla(b_plus_e_i * c2, c_plus_d_i, c3);
            const float z1 = ubig_mla(b_plus_e_i * c3, c_plus_d_i, c2);
            const float w0 = ubig_mla(b_plus_e_r * c2, c_plus_d_r, c3);
            const float w1 = ubig_mla(b_plus_e_r * c3, c_plus_d_r, c2);

            const float out0_r = b_plus_e_r + c_plus_d_r;
            const float out0_i = b_plus_e_i + c_plus_d_i;
            const float out1_r = w0 + a0;
            const float out1_i = z0 + a1;
            const float out2_base_r = w1 + a0;
            const float out2_base_i = z1 + a1;

            data[base + 2u * lane] = out0_r + a0;
            data[base + 2u * lane + 1u] = out0_i + a1;
            data[base + span + 2u * lane] = out1_r + y0;
            data[base + span + 2u * lane + 1u] = out1_i - y1;
            data[base + 2u * span + 2u * lane] = out2_base_r + x0;
            data[base + 2u * span + 2u * lane + 1u] = out2_base_i - x1;
            data[base + 3u * span + 2u * lane] = out2_base_r - x0;
            data[base + 3u * span + 2u * lane + 1u] = out2_base_i + x1;
            data[base + 4u * span + 2u * lane] = out1_r - y0;
            data[base + 4u * span + 2u * lane + 1u] = out1_i + y1;
        }
    }
}

static void ubig_fft320_core(float *out, const float *in, int scaled)
{
    float stage0[UBIG_FFT320_FLOATS];
    float stage1[UBIG_FFT320_FLOATS];

    ubig_fft320_radix4_entry(stage0, in);
    ubig_fft320_radix4_stage(stage1, stage0, ubig_fft320_stage4_twiddle, 4u);
    ubig_fft320_radix4_stage(out, stage1, ubig_fft320_stage16_twiddle, 16u);
    if (scaled)
        ubig_fft320_radix5_scaled(out);
    else
        ubig_fft320_radix5_unscaled(out);
}

void ubig_stage_a_fft320(float *out, const float *in, unsigned n, void *opaque)
{
    (void)opaque;
    if (n != UBIG_FFT320_N) {
        if (out != in)
            memset(out, 0, (size_t)n * 2u * sizeof(float));
        return;
    }
    ubig_fft320_core(out, in, 0);
}

void ubig_stage_a_fft320_norm320(float *out, const float *in, unsigned n, void *opaque)
{
    (void)opaque;
    if (n != UBIG_FFT320_N) {
        if (out != in)
            memset(out, 0, (size_t)n * 2u * sizeof(float));
        return;
    }
    ubig_fft320_core(out, in, 1);
}
