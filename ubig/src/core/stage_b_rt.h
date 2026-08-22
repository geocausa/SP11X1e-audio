#ifndef UBIG_STAGE_B_RT_H
#define UBIG_STAGE_B_RT_H
#include <stdint.h>

#define UBIG_STAGE_B_RT_MAX_SELECTED_ROWS 25u
#define UBIG_STAGE_B_RT_MAX_BANDS 20u

typedef struct {
    float response_a;
    float response_b;
    float response_c;
    float input;
    float countdown_scale;
    float countdown_bias;
    float smoothed_input;
    float toggle_keep;
    float toggle_state;
    int32_t countdown;
    uint32_t toggle;
} UbigStageBRtHysteresisState;

/* Exact scalar hysteresis/activity gate used by the universal RT controller. */
float ubig_stage_b_rt_hysteresis_process(UbigStageBRtHysteresisState *state);

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

typedef struct {
    float decay_step;
    float correction_step;
    const float *reference;
    float keep;
    float inject;
    const float *slope;
} UbigStageBRtBandGateConfig;

typedef struct {
    float value[UBIG_STAGE_B_RT_MAX_BANDS];
    uint32_t counter[UBIG_STAGE_B_RT_MAX_BANDS];
} UbigStageBRtBandGateRowState;

/* Exact enclosing per-row multiband gate/state path. All coefficient vectors
 * and chain boundary weights are caller-owned. */
void ubig_stage_b_rt_band_gate_process(float control,
                                       const UbigStageBRtBandGateConfig *config,
                                       UbigStageBRtBandGateRowState *row_state,
                                       uint32_t row_count,
                                       uint32_t active_width,
                                       const float *plane_a,
                                       const float *plane_b,
                                       const float *plane_c,
                                       const float *row_control,
                                       float *output,
                                       const float boundary_coeff[5]);

typedef struct {
    float polarity;
    float mix;
} UbigStageBRtCrossfadeState;

/* Exact bounded crossfade/polarity controller. */
void ubig_stage_b_rt_crossfade_process(UbigStageBRtCrossfadeState *state,
                                       const float *metric_a,
                                       const float *metric_b,
                                       int32_t control_a,
                                       int32_t control_b,
                                       uint32_t count,
                                       const float *source,
                                       float *destination);

typedef struct {
    float counter_scale;
    int32_t counter[UBIG_STAGE_B_RT_MAX_BANDS];
    float output_scale;
    float adaptive[UBIG_STAGE_B_RT_MAX_BANDS];
    float smoothed[UBIG_STAGE_B_RT_MAX_BANDS];
    float input_state0;
    float input_state1;
    float gate_state;
    float input_mix;
    float adaptive_mix;
    const float *history;
} UbigStageBRtStereoBlendState;

/* Exact deployed SP11 stereo state blender. The generic second-bank path from
 * the reference routine is outside this fixed two-row endpoint contract. */
void ubig_stage_b_rt_stereo_blend_process(float input0,
                                          float input1,
                                          UbigStageBRtStereoBlendState *state,
                                          uint32_t active_width,
                                          const float *trigger_row,
                                          const float *coefficient_row,
                                          const float *comparison_row,
                                          const float *input_row,
                                          float *destination);

typedef struct {
    const UbigStageBRtCurveRecord *curve_fall;
    const UbigStageBRtCurveRecord *curve_rise;
    const float *tail_weights;
    const float *chain_coeff;
    const float *gate_reference;
    const float *gate_slope;
} UbigStageBRtMultibandTuning;

typedef struct {
    float curve_rows[2][UBIG_STAGE_B_RT_MAX_BANDS];
    uint32_t curve_mode;
    UbigStageBRtCorrelationState correlation[2];
    UbigStageBRtMixSmootherConfig optional_mix;
    UbigStageBRtWindowBlendState window[2];
    float post_rows[2][UBIG_STAGE_B_RT_MAX_BANDS];
    float blend_rows[2][UBIG_STAGE_B_RT_MAX_BANDS];
    float blend_alpha;
    UbigStageBRtTailState tail;
    float gate_decay_step;
    float gate_correction_step;
    float gate_keep;
    float gate_inject;
    UbigStageBRtBandGateRowState gate_rows[2];
    float stereo_alpha;
    float stereo_row[UBIG_STAGE_B_RT_MAX_BANDS];
    UbigStageBRtStereoBlendState stereo;
    UbigStageBRtCrossfadeState crossfade;
    uint32_t active_mode;
    float enable_value;
} UbigStageBRtMultibandState;

/* Exact deployed SP11 stereo multiband parent. Tuning vectors/tables are
 * caller-owned; this API contains only native orchestration and persistent
 * semantic state. Proven endpoint geometry is two rows x 20 bands. */
void ubig_stage_b_rt_multiband_process(float enable_value,
                                       float curve_offset,
                                       UbigStageBRtMultibandState *state,
                                       uint32_t mode,
                                       const float *optional_control,
                                       UbigStageBRtBandRows *rows,
                                       UbigStageBRtBandRows *work,
                                       int32_t *telemetry,
                                       const UbigStageBRtMultibandTuning *tuning);


#define UBIG_STAGE_B_RT_SPECTRAL_BINS 77u

typedef struct {
    uint32_t period;
    uint32_t counter;
    int32_t exponent_offset;
    float output_scale;
    float energy[UBIG_STAGE_B_RT_SPECTRAL_BINS];
    int32_t shift[UBIG_STAGE_B_RT_SPECTRAL_BINS];
    int32_t global_shift;
} UbigStageBRtSpectralAccumulator;

typedef struct {
    float bins[UBIG_STAGE_B_RT_SPECTRAL_BINS];
    uint32_t count;
    int32_t exponent;
    float aggregate;
} UbigStageBRtSpectralExport;

/* Exact deployed two-row / 77-bin spectral accumulator. Each input row is
 * interleaved complex {real,imag}. A fresh accumulation window begins whenever
 * counter is zero; the result is exported when counter reaches period. */
void ubig_stage_b_rt_spectral_accumulate(UbigStageBRtSpectralAccumulator *state,
                                         const float *row0,
                                         const float *row1,
                                         UbigStageBRtSpectralExport *output);

#define UBIG_STAGE_B_RT_VARIATION_HISTORY_DEPTH 32u
#define UBIG_STAGE_B_RT_VARIATION_MAX_SEGMENTS 8u

typedef struct {
    float history[UBIG_STAGE_B_RT_VARIATION_HISTORY_DEPTH][UBIG_STAGE_B_RT_VARIATION_MAX_SEGMENTS];
    uint32_t index;
} UbigStageBRtVariationHistory;

typedef struct {
    uint32_t segment_count;
    const uint32_t *boundaries;
    const float *weights;
} UbigStageBRtVariationConfig;

/* Exact segmented variation-history update used by the universal RT analysis
 * scheduler. boundaries contains segment_count+1 strictly increasing sample
 * offsets beginning at zero; weights contains segment_count caller-owned gains. */
void ubig_stage_b_rt_variation_history_process(UbigStageBRtVariationHistory *state,
                                                const UbigStageBRtVariationConfig *config,
                                                const float *input,
                                                uint32_t input_count);

#define UBIG_STAGE_B_RT_CHANGE_HISTORY_DEPTH 32u

typedef struct {
    float history[UBIG_STAGE_B_RT_CHANGE_HISTORY_DEPTH];
    float previous_bins[UBIG_STAGE_B_RT_SPECTRAL_BINS];
    float previous_aggregate;
    int32_t previous_exponent;
    uint32_t index;
} UbigStageBRtSpectralChangeHistory;

/* Exact spectral-frame change metric over the semantic 77-bin export. */
void ubig_stage_b_rt_spectral_change_process(UbigStageBRtSpectralChangeHistory *state,
                                             const UbigStageBRtSpectralExport *input);

/* Exact scalar ratio map used by the RT statistic paths. The deployed
 * segmented-ratio path uses mode 0; other scheduler paths use small positive
 * mode values. */
float ubig_stage_b_rt_ratio_map_mode(float ratio,int32_t mode);
float ubig_stage_b_rt_ratio_map(float ratio);

#define UBIG_STAGE_B_RT_SEGMENT_RATIO_DEPTH 32u
#define UBIG_STAGE_B_RT_SEGMENT_RATIO_COUNT 8u

typedef struct {
    float history[UBIG_STAGE_B_RT_SEGMENT_RATIO_DEPTH][UBIG_STAGE_B_RT_SEGMENT_RATIO_COUNT];
    uint32_t index;
} UbigStageBRtSegmentRatioHistory;

typedef struct {
    const uint32_t *boundaries;
} UbigStageBRtSegmentRatioConfig;

/* Exact deployed eight-segment upper/lower-band ratio history update. */
void ubig_stage_b_rt_segment_ratio_process(UbigStageBRtSegmentRatioHistory *state,
                                           const UbigStageBRtSegmentRatioConfig *config,
                                           const UbigStageBRtSpectralExport *input);

#define UBIG_STAGE_B_RT_PEAK_HISTORY_DEPTH 32u

typedef struct {
    float history[UBIG_STAGE_B_RT_PEAK_HISTORY_DEPTH][3];
    uint32_t index;
} UbigStageBRtPeakResidualHistory;

/* Exact two-peak residual extractor. scratch receives a copy of the input bins
 * with the strongest-peak neighborhood zeroed. */
void ubig_stage_b_rt_peak_residual_process(UbigStageBRtPeakResidualHistory *state,
                                           const UbigStageBRtSpectralExport *input,
                                           float scratch[UBIG_STAGE_B_RT_SPECTRAL_BINS]);

#define UBIG_STAGE_B_RT_FEATURE_COUNT 8u
#define UBIG_STAGE_B_RT_FEATURE_CHANGE_DEPTH 32u

typedef struct {
    float history[UBIG_STAGE_B_RT_FEATURE_CHANGE_DEPTH];
    float previous[UBIG_STAGE_B_RT_FEATURE_COUNT];
    uint32_t index;
} UbigStageBRtFeatureChangeHistory;

/* Exact eight-feature exponent-normalization and frame-change metric. */
void ubig_stage_b_rt_feature_change_process(UbigStageBRtFeatureChangeHistory *state,
                                            const float input[UBIG_STAGE_B_RT_FEATURE_COUNT],
                                            float normalized[UBIG_STAGE_B_RT_FEATURE_COUNT]);

/* Exact exponent-scaled FMA sum used by the RT feature scheduler. */
float ubig_stage_b_rt_scaled_sum(const float *input,uint32_t count,int32_t exponent);

/* Exact 32-value normalized mean/deviation statistic shared by the cadence paths. */
void ubig_stage_b_rt_stat32(const float input[32],float *mean,float *deviation);

typedef struct {
    uint32_t step;
    uint32_t index;
} UbigStageBRtStatCursor;

/* Copy one 32-value row to caller scratch, compute the exact statistic, then
 * advance the 32-slot cursor by its caller-owned step. */
void ubig_stage_b_rt_stat32_step(UbigStageBRtStatCursor *cursor,
                                 const float input[32],
                                 float scratch[32],
                                 float output[2]);

#define UBIG_STAGE_B_RT_STAT_COLUMNS 8u

/* Gather active columns from a 32x8 row-major matrix, compute one exact
 * statistic per column, then advance the shared 32-slot cursor. */
void ubig_stage_b_rt_stat32_columns(UbigStageBRtStatCursor *cursor,
                                    const float matrix[32][UBIG_STAGE_B_RT_STAT_COLUMNS],
                                    uint32_t count,
                                    float scratch[32],
                                    float mean[UBIG_STAGE_B_RT_STAT_COLUMNS],
                                    float deviation[UBIG_STAGE_B_RT_STAT_COLUMNS]);

/* Build each column's circular 32-sample window beginning at cursor->index,
 * append prefix_count rows after wrap into the visible 64-float scratch, then
 * compute the exact statistic from the first 32 samples. */
void ubig_stage_b_rt_stat32_ring_columns(UbigStageBRtStatCursor *cursor,
                                         const float matrix[32][UBIG_STAGE_B_RT_STAT_COLUMNS],
                                         uint32_t prefix_count,
                                         float scratch[64],
                                         float mean[UBIG_STAGE_B_RT_STAT_COLUMNS],
                                         float deviation[UBIG_STAGE_B_RT_STAT_COLUMNS]);

#define UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH 32u
#define UBIG_STAGE_B_RT_FEATURE_RECORD_VALUES 20u
#define UBIG_STAGE_B_RT_FEATURE_SEGMENTS 8u

typedef struct {
    float records[UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH][UBIG_STAGE_B_RT_FEATURE_RECORD_VALUES];
    uint32_t index;
    uint32_t phase;
    float segment_sum[UBIG_STAGE_B_RT_FEATURE_SEGMENTS];
    float delta_sum[UBIG_STAGE_B_RT_FEATURE_SEGMENTS-1u];
    uint32_t segment_shift[UBIG_STAGE_B_RT_FEATURE_SEGMENTS];
    uint32_t delta_shift[UBIG_STAGE_B_RT_FEATURE_SEGMENTS-1u];
} UbigStageBRtFeatureHistory;

typedef struct {
    const uint32_t *boundaries; /* 9 entries: segment starts plus final end */
    uint32_t scaled_sum_count;
} UbigStageBRtFeatureHistoryConfig;

/* Exact 32-slot / 20-value feature-history controller. The per-call record is
 * built from the semantic spectral export; when ((index+2)&31)==phase the
 * temporal reducers refresh segment_sum/delta_sum and clear the future slot. */
void ubig_stage_b_rt_feature_history_process(UbigStageBRtFeatureHistory *state,
                                             const UbigStageBRtFeatureHistoryConfig *config,
                                             const UbigStageBRtSpectralExport *input);

/* Exact lower-cadence mean of feature-history record column 1, with the
 * reference's positive zero floor. */
float ubig_stage_b_rt_feature_history_mean(const float records[UBIG_STAGE_B_RT_FEATURE_HISTORY_DEPTH][UBIG_STAGE_B_RT_FEATURE_RECORD_VALUES]);

#define UBIG_STAGE_B_RT_PROJECTION_HISTORY_DEPTH 32u
#define UBIG_STAGE_B_RT_PROJECTION_VALUES 8u
#define UBIG_STAGE_B_RT_PROJECTION_MEASUREMENTS 19u
#define UBIG_STAGE_B_RT_PROJECTION_LUT 76u

typedef struct {
    uint32_t start;
    uint32_t count;
    const float *weights;
} UbigStageBRtProjectionBand;

typedef struct {
    UbigStageBRtProjectionBand bands[UBIG_STAGE_B_RT_PROJECTION_MEASUREMENTS];
    const float *projection_lut; /* 76 caller-owned coefficients */
} UbigStageBRtProjectionConfig;

typedef struct {
    float records[UBIG_STAGE_B_RT_PROJECTION_HISTORY_DEPTH][UBIG_STAGE_B_RT_PROJECTION_VALUES];
    uint32_t index;
    uint32_t phase;
    float sum[UBIG_STAGE_B_RT_PROJECTION_VALUES];
    float delta_sum[UBIG_STAGE_B_RT_PROJECTION_VALUES-1u];
    uint32_t shift[UBIG_STAGE_B_RT_PROJECTION_VALUES];
    uint32_t delta_shift[UBIG_STAGE_B_RT_PROJECTION_VALUES-1u];
} UbigStageBRtProjectionHistory;

/* Exact 19-measurement -> 8-value projection history controller. Measurement
 * weights and the 76-entry projection lookup are caller-owned configuration. */
void ubig_stage_b_rt_projection_history_process(UbigStageBRtProjectionHistory *state,
                                                const UbigStageBRtProjectionConfig *config,
                                                const UbigStageBRtSpectralExport *input);
#endif
