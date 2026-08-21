#include "stage_a_compressor_worker.h"
#include "stage_a_compressor_primitives.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t lu32(const unsigned char *p, size_t o){uint32_t v;memcpy(&v,p+o,4);return v;}
static uint64_t lu64(const unsigned char *p, size_t o){uint64_t v;memcpy(&v,p+o,8);return v;}
static float lf32(const unsigned char *p, size_t o){float v;memcpy(&v,p+o,4);return v;}
static void su32(unsigned char *p,size_t o,uint32_t v){memcpy(p+o,&v,4);}
static void sf32(unsigned char *p,size_t o,float v){memcpy(p+o,&v,4);}

float ubig_comp_transition5(const float config[5], float previous, float target)
{
    const float half_previous = previous * 0.5f;
    const float half_delta = fmaf(target, 0.5f, -half_previous);
    if (half_delta < 0.0f) {
        const float scaled = config[1] * half_delta;
        const float limited = (scaled > config[0]) ? scaled : config[0];
        const float s = limited + half_previous;
        return s + s;
    }
    if (config[3] < half_delta) {
        const float s = (config[4] + half_delta) + half_previous;
        return s + s;
    }
    const float x = half_delta * 4.0f;
    const float x2 = x * x;
    const float s = (x2 * config[2]) + half_previous;
    return s + s;
}

void ubig_comp_rise_gate_worker(void *state,
                                const struct ubig_float_rows *rows,
                                const float *matrix20,
                                uint32_t matrix_rows,
                                uint32_t *matrix_gate,
                                uint32_t *aggregate_gate)
{
    unsigned char *p = state;
    const unsigned char *cfg = (const unsigned char *)(uintptr_t)lu64(p,0x00);
    const float *transition = (const float *)(cfg + 0x18);
    const uint32_t count = lu32(p,0x08);
    float low_agg = -1.0f;
    float high_agg = -1.0f;

    for (uint32_t band=0; band<count; ++band) {
        float maximum = -1.0f;
        for (uint32_t ch=0; ch<rows->count; ++ch) {
            const float v = rows->rows[ch][band];
            if (v > maximum) maximum = v;
        }
        const float old = lf32(p,0x64 + 4u*band);
        const float smoothed = ubig_comp_transition5(transition, old, maximum);
        sf32(p,0x64 + 4u*band, smoothed);
        su32(p,0x14 + 4u*band, smoothed < maximum);
        if (band >= 14)
            high_agg = ubig_comp_soft_max(maximum, high_agg);
        else
            low_agg = ubig_comp_soft_max(maximum, low_agg);
    }

    float high_state = ubig_comp_transition5(transition, lf32(p,0xb8), high_agg);
    sf32(p,0xb8,high_state);
    float low_state = ubig_comp_transition5(transition, lf32(p,0xb4), low_agg);
    sf32(p,0xb4,low_state);

    const float low_rise_raw = low_agg - low_state;
    const float low_rise = (low_rise_raw > 0.0f) ? low_rise_raw : 0.0f;
    const float high_rise = high_agg - high_state;

    float matrix_min = 0.0f;
    for (uint32_t band=3; band<11; ++band) {
        for (uint32_t row=0; row<matrix_rows; ++row) {
            const float v = matrix20[row*20u + band];
            if (v < matrix_min) matrix_min = v;
        }
    }

    const float gate_step = 0x1.3b13b2p-6f;
    *aggregate_gate = (low_rise + gate_step < high_rise);
    const float stress = (-matrix_min > gate_step) ? -matrix_min : gate_step;
    *matrix_gate = (stress + low_rise < high_rise);
}

void ubig_comp_band_controller(void *state,
                               const float *activity,
                               const struct ubig_float_rows *rows,
                               const float *matrix20,
                               uint32_t matrix_rows,
                               float severity_drive,
                               float activity_weight,
                               float ratio_gate,
                               float ratio_margin)
{
    unsigned char *p = state;
    uint32_t matrix_gate = 0, aggregate_gate = 0;
    ubig_comp_rise_gate_worker(state, rows, matrix20, matrix_rows,
                               &matrix_gate, &aggregate_gate);

    const uint32_t count = lu32(p, 0x08);
    uint32_t end = count < 19u ? count : 19u;
    const float half = 0.5f;
    const float one = 1.0f;
    float floor_state = fmaf(severity_drive, half, -one);
    float maximum = -1.0f;
    float accumulator = -1.0f;

    if (end > 2u) {
        const unsigned char *cfg = (const unsigned char *)(uintptr_t)lu64(p,0x00);
        const float *coef = *(const float *const *)(cfg + 0x30);
        for (uint32_t band = 2; band < end; ++band) {
            const float v = lf32(p, 0x64 + 4u*band);
            if (v > maximum) maximum = v;
            const float hv = v * 0.5f;
            const float carried_floor = fmaf(-severity_drive, half, floor_state);
            const float a = hv - 0x1.3b13b2p-3f;
            const float b = hv - 0x1.3b13b2p-5f;
            floor_state = (a > carried_floor) ? a : carried_floor;
            if (b <= floor_state) {
                const float delta = floor_state - b;
                const float scaled = coef[band] * delta;
                accumulator = fmaf(scaled, 0.25f, accumulator);
            }
        }
    }

    float target;
    if (maximum < -0x1.d89d8ap-2f) {
        const float current = lf32(p, 0xc0);
        target = (current < 0.3f) ? current : 0.3f;
    } else {
        float severity = accumulator + 1.0f;
        if (severity < 0.0f) severity = 0.0f;
        if (severity > 0x1.99999ap-8f) severity = 0x1.99999ap-8f;
        const float scaled = severity * 0.625f;
        target = fmaf(-scaled, 256.0f, 1.0f);
    }

    ubig_comp_band_state_update(state, activity, matrix_gate, aggregate_gate,
                                activity_weight, ratio_gate, ratio_margin, target);
}

float ubig_comp_transition5_cubic(const float config[5], float previous, float target)
{
    const float half_previous = previous * 0.5f;
    const float half_delta = fmaf(target, 0.5f, -half_previous);
    if (half_delta < 0.0f) {
        const float scaled = config[1] * half_delta;
        const float limited = (scaled > config[0]) ? scaled : config[0];
        const float s = limited + half_previous;
        return s + s;
    }
    if (config[3] < half_delta) {
        const float s = (config[4] + half_delta) + half_previous;
        return s + s;
    }
    const float x = half_delta + half_delta;
    const float x2 = x * x;
    const float x3 = x2 * x;
    const float shaped = (x3 * config[2]) * 4.0f;
    const float s = shaped + half_previous;
    return s + s;
}

void ubig_comp_dual_plane_update(struct ubig_dual_floor_state *state,
                                 const struct ubig_float_rows *rows,
                                 const float **primary_out,
                                 const float **secondary_out,
                                 int32_t *rise_flags,
                                 float bias)
{
    const float *primary_cfg = state->config;
    const float *secondary_cfg = state->config + 5;
    for (uint32_t band = 0; band < state->count; ++band) {
        float maximum = -1.0f;
        for (uint32_t ch = 0; ch < rows->count; ++ch) {
            const float v = rows->rows[ch][band];
            if (v > maximum) maximum = v;
        }
        float target = maximum + bias;
        if (target < -1.0f) target = -1.0f;
        if (target > 1.0f) target = 1.0f;
        rise_flags[band] = state->primary[band] < target;
        state->primary[band] = ubig_comp_transition5(primary_cfg, state->primary[band], target);
        state->secondary[band] = ubig_comp_transition5_cubic(secondary_cfg, state->secondary[band], target);
    }
    if (primary_out) *primary_out = state->primary;
    if (secondary_out) *secondary_out = state->secondary;
}
