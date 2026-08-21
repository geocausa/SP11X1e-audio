#ifndef UBIG_STAGE_B_RT_H
#define UBIG_STAGE_B_RT_H
#include <stdint.h>

#define UBIG_STAGE_B_RT_MAX_SELECTED_ROWS 25u
#define UBIG_STAGE_B_RT_MAX_BANDS 20u

typedef struct {
    uint32_t group_count;
    uint32_t vectors_per_group;
    float ***groups;
} UbigStageBRtComplexGroups;

typedef struct {
    uint32_t vectors_per_group;
    float ***groups;
} UbigStageBRtExtraGroups;

typedef struct {
    uint32_t row_count;
    uint32_t band_count;
    float **rows;
    uint32_t capacity;
} UbigStageBRtBandRows;

typedef struct {
    int32_t **rows;
} UbigStageBRtTelemetryRows;

/* Exact complex-energy reducer used by the Stage-B RT band analyzer. Rows are
 * interleaved {real,imag}; begin/end are complex-bin indices. */
float ubig_stage_b_rt_complex_energy(float *const *rows,
                                     uint32_t row_count,
                                     uint32_t begin,
                                     uint32_t end);

/* Exact deployed Stage-B band-energy/log row builder. group_to_output selects
 * which main groups contribute to each output row. For selected main groups
 * 0 and 1, optional extra vectors are appended. band_ends contains cumulative
 * complex-bin boundaries. */
void ubig_stage_b_rt_band_log_process(float offset0,
                                      float offset1,
                                      const UbigStageBRtComplexGroups *main_groups,
                                      const UbigStageBRtExtraGroups *extra_groups,
                                      const uint32_t *band_ends,
                                      const int32_t *group_to_output,
                                      UbigStageBRtBandRows *output,
                                      UbigStageBRtTelemetryRows *telemetry);
#define UBIG_STAGE_B_RT_TARGET_PLANES 4u
#define UBIG_STAGE_B_RT_SP11_ROWS 2u
#define UBIG_STAGE_B_RT_SP11_BANDS 20u
#define UBIG_STAGE_B_RT_SP11_BINS 77u

typedef struct {
    float *plane[UBIG_STAGE_B_RT_TARGET_PLANES];
} UbigStageBRtTargetObject;

typedef struct {
    uint32_t object_count;
    uint32_t bin_count;
    UbigStageBRtTargetObject *objects;
} UbigStageBRtTargetSet;

/* Exact meaningful-bin contract of the universal deployed SP11 output shaper.
 * The reference vector loop touches one additional two-float SIMD padding bin;
 * UbiG deliberately excludes that non-audio padding from the semantic API. */
void ubig_stage_b_rt_output_shape(float row_offset,
                                  float linked_ceiling,
                                  const UbigStageBRtBandRows *input,
                                  UbigStageBRtBandRows *output,
                                  const uint32_t *object_to_row,
                                  const uint32_t *band_ends,
                                  UbigStageBRtTargetSet *targets);
typedef struct {
    float bias;
    float control_scale;
} UbigStageBRtMixSmootherConfig;

/* Exact 20-lane tail clear used by the multiband sibling setup. */
void ubig_stage_b_rt_zero_band_tail(UbigStageBRtBandRows *rows);

/* Exact stateful source/destination smoother. The 8-lane vector prefix and
 * scalar tail preserve their distinct reference FMA ordering. */
void ubig_stage_b_rt_mix_smooth(const UbigStageBRtMixSmootherConfig *config,
                                float control,
                                const float *source,
                                float *state,
                                uint32_t count);

typedef struct {
    float quadratic;
    float linear;
    float constant;
} UbigStageBRtCurveRecord;

/* Exact 20-lane per-row curve smoother beneath the profile-selective
 * multiband block. Rise/fall polynomial records are caller-owned. */
void ubig_stage_b_rt_curve_smooth(float offset,
                                  float *const *input_rows,
                                  float *state_rows,
                                  uint32_t row_count,
                                  const UbigStageBRtCurveRecord *fall,
                                  const UbigStageBRtCurveRecord *rise);

/* Exact row-wise Stage-B Horner exp2 conversion at the multiband scale. Each
 * row has a fixed 20-lane stride; inactive tail lanes and one status word per
 * row are cleared exactly as in the deployed helper. */
void ubig_stage_b_rt_exp_rows(float *output,
                              uint32_t *row_status,
                              const float *input,
                              uint32_t active_width,
                              uint32_t row_count);

/* Exact 20-lane circular history/sum primitive used by the multiband state path. */
typedef struct {
    uint32_t depth;
    uint32_t index;
    float *buffer; /* depth x 20 floats */
} UbigStageBRtRowHistory;

int ubig_stage_b_rt_row_history_update(UbigStageBRtRowHistory *state,
                                       float output[UBIG_STAGE_B_RT_MAX_BANDS],
                                       const float input[UBIG_STAGE_B_RT_MAX_BANDS]);

/* Exact multiband correlation/history controller. All backing rings are caller-owned. */
typedef struct {
    UbigStageBRtRowHistory primary;
    uint32_t secondary_depth;
    uint32_t secondary_index;
    float *secondary_buffer; /* secondary_depth x 20 */
    uint32_t *secondary_status;
    uint32_t integrator_depth;
    uint32_t integrator_index;
    float correlation_scale;
    float accumulator_a;
    float accumulator_b;
    float *integrator_ring;
    float output_state;
} UbigStageBRtCorrelationState;

void ubig_stage_b_rt_correlation_process(UbigStageBRtCorrelationState *states,
                                         uint32_t row_count,
                                         const float *input_rows,
                                         float *output);

/* Exact scaled 20-lane sliding-window accumulator used by the next multiband stage. */
typedef struct {
    uint32_t depth;
    uint32_t index;
    float *history; /* depth x 20 floats */
    float scale;
    float accumulator[UBIG_STAGE_B_RT_MAX_BANDS];
    float window_sum[UBIG_STAGE_B_RT_MAX_BANDS];
} UbigStageBRtWindowSum;

float *ubig_stage_b_rt_window_sum_update(UbigStageBRtWindowSum *state,
                                         const float input[UBIG_STAGE_B_RT_MAX_BANDS]);

/* Exact SP11 ARM64 per-lane deviation/RMS mapper. The AArch64 prefix preserves
 * the reference reciprocal-square-root estimate/refinement instruction schedule. */
void ubig_stage_b_rt_rms_deviation(float scale,
                                   float output[UBIG_STAGE_B_RT_MAX_BANDS],
                                   const float current[UBIG_STAGE_B_RT_MAX_BANDS],
                                   const float *history,
                                   uint32_t active_width,
                                   uint32_t depth);

/* Exact enclosing two-window RMS/blend state path. */
typedef struct {
    UbigStageBRtWindowSum input_window;
    UbigStageBRtWindowSum rms_window;
    float rms_scale;
    float blend_bias;
    float blend_scale;
} UbigStageBRtWindowBlendState;

void ubig_stage_b_rt_window_blend_process(UbigStageBRtWindowBlendState *state,
                                          uint32_t active_width,
                                          const float input[UBIG_STAGE_B_RT_MAX_BANDS],
                                          float output[UBIG_STAGE_B_RT_MAX_BANDS]);

/* Exact tail estimator used by the final small multiband branch. The weight
 * vector is caller-owned; proven counts are 2..20. */
float ubig_stage_b_rt_tail_estimate(float previous,
                                    const float *input,
                                    uint32_t count,
                                    const float *weights);

typedef struct {
    float estimate;
    float tail_state;
} UbigStageBRtTailState;

/* Exact enclosing two-scalar tail controller. Weights remain caller-owned. */
float ubig_stage_b_rt_tail_control(UbigStageBRtTailState *state,
                                   const float *input,
                                   uint32_t count,
                                   const float *weights);

/* Exact in-place recursive band-chain smoother used by the remaining
 * multiband state path. Five boundary coefficients are caller-owned; proven
 * counts are 9..20, including the deployed 20-band contract. */
void ubig_stage_b_rt_chain_smooth(float *state,
                                  uint32_t count,
                                  uint32_t activity,
                                  const float boundary_coeff[5]);

#endif
