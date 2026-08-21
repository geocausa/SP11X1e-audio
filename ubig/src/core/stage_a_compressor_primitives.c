#include "stage_a_compressor_primitives.h"

#include <math.h>
#include <stddef.h>

void ubig_comp_dual_floor_init(struct ubig_dual_floor_state *s,
                               const float *config, uint32_t count)
{
    s->config = config;
    s->count = count;
    for (uint32_t i = 0; i < count; ++i) {
        s->primary[i] = -1.0f;
        s->secondary[i] = -1.0f;
    }
}

void ubig_comp_flag_state_init(struct ubig_flag_state *s,
                               const float *config, uint32_t count)
{
    s->config = config;
    s->count = count;
    s->phase = 0;
}

void ubig_comp_scalar_state_init(struct ubig_scalar_state *s,
                                 const float *config, uint32_t count)
{
    s->config = config;
    s->count = count;
    s->value = 0x1.1b8ab8p-4f;
}

void *ubig_comp_scalar_payload(void *state_base)
{
    return (unsigned char *)state_base + 0xc4;
}

void ubig_comp_uniform_state_init(struct ubig_directional_smoother *s,
                                  const float *config, uint32_t count)
{
    s->coeff = config;
    s->count = count;
    for (uint32_t i = 0; i < count; ++i)
        s->value[i] = 0x1.f81f82p-9f;
}

void ubig_comp_directional_smooth(struct ubig_directional_smoother *s,
                                  const int32_t *direction_flags,
                                  const float *target)
{
    const float keep = s->coeff[0];
    const float take = s->coeff[1];
    for (uint32_t i = 0; i < s->count; ++i) {
        const float current = s->value[i];
        const float next = target[i];
        const int flag = direction_flags[i] != 0;
        const int smooth = (next < current) ? !flag : ((current < next) ? flag : flag);
        s->value[i] = smooth ? fmaf(current, keep, next * take) : next;
    }
}

void ubig_comp_slow_gain_bounds(struct ubig_scalar_state *s,
                                const float *input_a,
                                const float *input_b,
                                float *lower,
                                float *upper,
                                const float *mix,
                                float drive,
                                float bias,
                                float deviation_a,
                                float deviation_b,
                                float mix_level)
{
    static const float drive_knee = -0x1.f81f82p-8f;
    static const float state_knee = -0x1.7a17a2p-6f;
    static const float state_offset = 0x1.6d7d3ep-6f;
    static const float bias_offset = 0x1.1b8ab8p-4f;
    static const float bound_span = 0x1.7a17a2p-5f;

    const float *cfg = s->config;
    float state = s->value;
    if (drive < drive_knee) {
        const float delta = drive - drive_knee;
        state += ((state > state_knee) ? cfg[1] : cfg[3]) * delta;
    } else if (state < state_knee) {
        const float base = state * cfg[4];
        state = fmaf(-cfg[5], state_offset, base);
    } else {
        const float add = (bias + bias_offset) * cfg[5];
        state = fmaf(state, cfg[4], add);
    }
    s->value = state;

    const float hi = state * 0.5f;
    const float lo = hi - bound_span;
    if (!input_a || !input_b) {
        for (uint32_t i = 0; i < s->count; ++i) {
            upper[i] = hi;
            lower[i] = lo;
        }
        return;
    }

    const float half_dev_b = deviation_b * 0.5f;
    const float half_mix = mix_level * 0.5f;
    const float center = fmaf(-deviation_a, 0.5f, half_dev_b);
    for (uint32_t i = 0; i < s->count; ++i) {
        const float mix_term = fmaf(mix[i] - 1.0f, half_mix, center);
        const float candidate_hi = fmaf(input_b[i], 0.5f, mix_term);
        if (candidate_hi >= hi) {
            upper[i] = hi;
            lower[i] = lo;
        } else {
            upper[i] = candidate_hi;
            const float candidate_lo = fmaf(input_a[i], 0.5f, mix_term);
            const float span_floor = candidate_hi - bound_span;
            lower[i] = (span_floor < candidate_lo) ? span_floor : candidate_lo;
        }
    }
}

static float ubig_recip_count(uint32_t n)
{
    static const float r[20] = {
        1.0f, 1.0f/2.0f, 1.0f/3.0f, 1.0f/4.0f, 1.0f/5.0f,
        1.0f/6.0f, 0x1.249248p-3f, 1.0f/8.0f, 1.0f/9.0f, 1.0f/10.0f,
        1.0f/11.0f, 1.0f/12.0f, 1.0f/13.0f, 1.0f/14.0f, 1.0f/15.0f,
        1.0f/16.0f, 1.0f/17.0f, 1.0f/18.0f, 1.0f/19.0f, 1.0f/20.0f
    };
    return r[n - 1u];
}

void ubig_comp_nonlinear_correction(struct ubig_scalar_state *s,
                                    const float *input,
                                    const float *floor,
                                    const float *ceiling,
                                    const int32_t *mask,
                                    float *half_drive,
                                    float *correction,
                                    float aggregate_scale,
                                    float common_drive)
{
    const float old_state = s->value;
    const uint32_t count = s->count;
    if (count == 0) {
        s->value = 0.0f;
        return;
    }

    for (uint32_t i = 0; i < count; ++i) {
        const float t0 = fmaf(input[i], 0.5f, old_state);
        const float driven = fmaf(common_drive, 0.5f, t0);
        half_drive[i] = driven - old_state;
        if (ceiling[i] >= driven) {
            correction[i] = 0.0f;
            continue;
        }
        const float span = ceiling[i] - floor[i];
        const float bent = span - floor[i];
        if (bent + driven >= 0.0f) {
            correction[i] = floor[i] - driven;
        } else {
            const float denom_f = span * 4.0f;
            const float distance = driven - ceiling[i];
            const float distance2 = distance * distance;
            const float inv = (float)(1.0 / (double)denom_f);
            correction[i] = inv * distance2;
        }
    }

    float accumulator = 0.0f;
    uint32_t active = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (mask[i] == 0) {
            ++active;
            accumulator = fmaf(correction[i], 0.03125f, accumulator);
        }
    }

    if (active == 0) {
        s->value = 0.0f;
    } else {
        float target = ubig_recip_count(active) * accumulator;
        target *= 32.0f;
        target *= aggregate_scale;
        const float *cfg = s->config;
        if (target < old_state) {
            const float add = cfg[1] * target;
            s->value = fmaf(old_state, cfg[0], add);
        } else {
            const float add = cfg[2] * old_state;
            s->value = fmaf(cfg[3], target, add);
        }
    }

    for (uint32_t i = 0; i < count; ++i)
        correction[i] += old_state;
}

void ubig_comp_linked_deviation(const int32_t *mask,
                                const float *reference,
                                uint32_t count,
                                float *upper,
                                float *lower,
                                float blend)
{
    if (count == 0)
        return;

    float scratch[20];
    float out_lower[20];
    float minimum = 1.0f;
    const float keep = 1.0f - blend;
    for (uint32_t i = 0; i < count; ++i) {
        scratch[i] = fmaf(reference[i], 0.5f, -upper[i]);
        if (mask[i] == 0 && scratch[i] < minimum)
            minimum = scratch[i];
    }

    float accumulator = 0.0f;
    float maximum = 0.0f;
    uint32_t unmasked = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (mask[i] != 0)
            continue;
        const float delta = scratch[i] - minimum;
        if (delta > 0x1.934c68p-12f) {
            accumulator = fmaf(delta, 0.03125f, accumulator);
            if (delta > maximum)
                maximum = delta;
        }
        ++unmasked;
    }
    if (unmasked == 0)
        return;

    float aggregate = ubig_recip_count(unmasked) * accumulator;
    aggregate *= 32.0f;
    aggregate *= 0x1.bea246p-2f;
    aggregate = fmaf(maximum, 0x1.20aedcp-1f, aggregate);
    const float threshold = aggregate + minimum;

    for (uint32_t i = 0; i < count; ++i) {
        const float original_hi = upper[i];
        const float original_lo = lower[i];
        float corrected_hi = original_hi;
        if (mask[i] == 0)
            corrected_hi = (scratch[i] - threshold) + original_hi;
        if (original_hi < corrected_hi)
            corrected_hi = original_hi;
        const float delta = corrected_hi - original_hi;
        const float span_floor = corrected_hi - 0x1.7a17a2p-5f;
        const float candidate_lo = delta + original_lo;
        const float corrected_lo = (candidate_lo < span_floor) ? candidate_lo : span_floor;
        const float hi_scaled = corrected_hi * blend;
        const float lo_scaled = corrected_lo * blend;
        scratch[i] = fmaf(original_hi, keep, hi_scaled);
        out_lower[i] = fmaf(original_lo, keep, lo_scaled);
    }

    for (uint32_t i = 0; i < count; ++i) {
        upper[i] = scratch[i];
        lower[i] = out_lower[i];
    }
}

void ubig_comp_neighbor_limit(uint32_t count,
                              const int32_t *mask,
                              const float *input,
                              float *output)
{
    static const float kernel[8][3] = {
        {0x1.54fdf4p-2f, 0x1.560418p-2f, 0x1.54fdf4p-2f},
        {0x1.54fdf4p-2f, 0x1.558106p-1f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0x1.558106p-1f, 0x1.54fdf4p-2f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };
    if (count == 0)
        return;

    float value[22];
    int32_t flag[22];
    value[0] = 0.0f;
    flag[0] = 1;
    for (uint32_t i = 0; i < count; ++i) {
        value[i + 1] = input[i];
        flag[i + 1] = mask[i] != 0;
    }
    value[count + 1] = 0.0f;
    flag[count + 1] = 1;

    for (uint32_t i = 0; i < count; ++i) {
        const unsigned pattern = (unsigned)flag[i + 2]
                               + ((unsigned)flag[i + 1] << 1)
                               + ((unsigned)flag[i] << 2);
        const float *c = kernel[pattern];
        float weighted = value[i] * c[0];
        weighted = fmaf(value[i + 1], c[1], weighted);
        weighted = fmaf(value[i + 2], c[2], weighted);
        const float limited = (value[i + 1] < weighted) ? value[i + 1] : weighted;
        output[i] = limited + limited;
    }
}

static void ubig_store_u32(unsigned char *p, size_t off, uint32_t v)
{
    __builtin_memcpy(p + off, &v, sizeof(v));
}

static void ubig_store_u64(unsigned char *p, size_t off, uint64_t v)
{
    __builtin_memcpy(p + off, &v, sizeof(v));
}

static void ubig_store_f32(unsigned char *p, size_t off, float v)
{
    __builtin_memcpy(p + off, &v, sizeof(v));
}

void ubig_comp_band_state_init(void *state, const void *config, uint32_t count)
{
    unsigned char *p = state;
    const uint64_t cfg = (uint64_t)(uintptr_t)config;
    ubig_store_u64(p, 0x00, cfg);
    ubig_store_u32(p, 0x08, count);
    ubig_store_u64(p, 0x0c, 0);
    for (uint32_t i = 0; i < 20; ++i)
        ubig_store_u32(p, 0x14 + 4u * i, 0);
    for (uint32_t i = 0; i < 20; ++i)
        ubig_store_f32(p, 0x64 + 4u * i, -1.0f);
    ubig_store_f32(p, 0xb4, -1.0f);
    ubig_store_f32(p, 0xb8, -1.0f);
    ubig_store_u64(p, 0xbc, 0);
    for (uint32_t i = 0; i < 20; ++i)
        ubig_store_u32(p, 0xc4 + 4u * i, 0);
}

float ubig_comp_soft_max(float a, float b)
{
    const float maximum = (b > a) ? b : a;
    const float d = a - b;
    const float ad = (-d > d) ? -d : d;
    if (ad >= 0x1.3b13b2p-3f)
        return maximum;

    float t = fmaf(-ad, 0x1.45328cp+3f, 0x1.e44c28p+1f);
    t = fmaf(t, ad, -0x1.f7e15p-2f);
    t = fmaf(t, ad, 0x1.7b6302p-6f);
    float out = maximum + t;
    if (out < -1.0f)
        out = -1.0f;
    if (out > 1.0f)
        out = 1.0f;
    return out;
}
