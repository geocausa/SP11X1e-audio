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

/* Exact deployed 32-sample real-spectrum helper. Produces normalized
 * magnitudes for bins 1..16; DC is intentionally omitted. */
void ubig_stage_b_rt_spectrum32(const float input[32],float output[16]);

#define UBIG_STAGE_B_RT_SLOPE32_VALUES 32u

typedef struct {
    float combined[UBIG_STAGE_B_RT_SLOPE32_VALUES];
    float positive_row0[UBIG_STAGE_B_RT_SLOPE32_VALUES];
    float positive_row1[UBIG_STAGE_B_RT_SLOPE32_VALUES];
    float normalized_row0[UBIG_STAGE_B_RT_SLOPE32_VALUES];
    float normalized_row1[UBIG_STAGE_B_RT_SLOPE32_VALUES];
} UbigStageBRtSlope32;

/* Exact deployed dual-row positive-slope preparation. The two input rows are
 * normalized by one shared binary exponent, filtered by the reference centered
 * first-difference kernel, half-wave rectified, summed, then normalized by the
 * mean energy of both normalized rows. Intermediate banks remain visible
 * because the following lower-scheduler stage consumes them. */
void ubig_stage_b_rt_slope32_prepare(const float row0[UBIG_STAGE_B_RT_SLOPE32_VALUES],
                                     const float row1[UBIG_STAGE_B_RT_SLOPE32_VALUES],
                                     UbigStageBRtSlope32 *output);

#define UBIG_STAGE_B_RT_SLOPE_FEATURES 4u

/* Exact deployed feature reducer for the dual-row slope workspace. The
 * reducer intentionally reuses the positive-row banks as correlation/peak
 * scratch, matching the following lower-scheduler lifetime. */
void ubig_stage_b_rt_slope32_features(UbigStageBRtSlope32 *workspace,
                                      float output[UBIG_STAGE_B_RT_SLOPE_FEATURES]);

/* Exact 32-value RMS deviation around a caller-supplied mean. shift is the
 * binary normalization exponent selected by the enclosing cadence transform. */
float ubig_stage_b_rt_deviation32(float mean,const float input[32],uint32_t shift);

/* Exact 32-value normalized mean/deviation statistic shared by the cadence paths. */
void ubig_stage_b_rt_stat32(const float input[32],float *mean,float *deviation);

typedef struct {
    uint32_t step;
    uint32_t index;
} UbigStageBRtStatCursor;

#define UBIG_STAGE_B_RT_CADENCE_COLUMNS 8u
#define UBIG_STAGE_B_RT_CADENCE_DELTAS 7u
#define UBIG_STAGE_B_RT_CADENCE_OUTPUTS 30u

typedef struct {
    float matrix[32][UBIG_STAGE_B_RT_CADENCE_COLUMNS];
    UbigStageBRtStatCursor cursor;
    float column_accumulator[UBIG_STAGE_B_RT_CADENCE_COLUMNS];
    float delta_accumulator[UBIG_STAGE_B_RT_CADENCE_DELTAS];
    uint32_t column_shift[UBIG_STAGE_B_RT_CADENCE_COLUMNS];
    uint32_t delta_shift[UBIG_STAGE_B_RT_CADENCE_DELTAS];
} UbigStageBRtCadenceSummary;

/* Exact lower-cadence 8-column / 7-adjacent-difference summary transform.
 * Accumulators and their binary normalization shifts are caller-owned state. */
void ubig_stage_b_rt_cadence_summary_process(UbigStageBRtCadenceSummary *state,
                                             float output[UBIG_STAGE_B_RT_CADENCE_OUTPUTS],
                                             float scratch[32]);

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

#define UBIG_STAGE_B_RT_FEATURE_CADENCE_OUTPUTS 186u
#define UBIG_STAGE_B_RT_FEATURE_CADENCE_SPECTRA 8u
#define UBIG_STAGE_B_RT_FEATURE_CADENCE_SPECTRUM_BINS 16u

/* Exact lower-cadence feature-bank controller beneath the deployed universal
 * scheduler. The caller invokes it when state->index == state->phase. It
 * consumes the already-native 32x20 feature history plus its reducer state,
 * emits scalar statistics, eight energy-weighted 16-bin spectra and the
 * dual-row slope descriptors, then advances phase by cadence_step modulo 32. */
void ubig_stage_b_rt_feature_cadence_process(UbigStageBRtFeatureHistory *state,
                                             uint32_t cadence_step,
                                             float output[UBIG_STAGE_B_RT_FEATURE_CADENCE_OUTPUTS]);

/* Exact sorted 32-value peak/shoulder metrics used by a lower cadence path.
 * scratch receives the sorted input and may alias input. */
void ubig_stage_b_rt_rank_metrics(float gain,
                                  const float input[32],
                                  float scratch[32],
                                  float *peak_metric,
                                  float *ratio_metric);

#define UBIG_STAGE_B_RT_RANK_HISTORY_ROWS 32u
#define UBIG_STAGE_B_RT_RANK_HISTORY_COLUMNS 3u
#define UBIG_STAGE_B_RT_RANK_OUTPUTS 10u

typedef struct {
    float matrix[UBIG_STAGE_B_RT_RANK_HISTORY_ROWS][UBIG_STAGE_B_RT_RANK_HISTORY_COLUMNS];
    UbigStageBRtStatCursor cursor;
} UbigStageBRtRankHistory;

/* Exact lower-cadence three-column rank/statistics controller. */
void ubig_stage_b_rt_rank_history_process(UbigStageBRtRankHistory *state,
                                          float control,
                                          float output[UBIG_STAGE_B_RT_RANK_OUTPUTS],
                                          float scratch[64]);

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

#define UBIG_STAGE_B_RT_SCHED_UPPER   0x1u
#define UBIG_STAGE_B_RT_SCHED_LOWER_A 0x2u
#define UBIG_STAGE_B_RT_SCHED_LOWER_B 0x4u

typedef struct {
    uint32_t upper_count;
    uint32_t upper_period;
    uint32_t lower_count;
    uint32_t lower_period;
    uint32_t lower_reset;
    uint32_t lower_toggle;
} UbigStageBRtSchedulerClock;

/* Exact reference-VA 0x18008C6A8 cadence gate. The returned bitmask says
 * which transform group executes on this call; state is advanced first, just
 * as in the deployed controller. */
uint32_t ubig_stage_b_rt_scheduler_step(UbigStageBRtSchedulerClock *clock);

typedef struct {
    uint16_t feature_index;
    uint16_t exponent;
    float scale;
    float weight;
    float center;
} UbigStageBRtControlTerm;

typedef struct {
    uint32_t term_count;
    float transfer_gain;
    float transfer_bias;
    const UbigStageBRtControlTerm *terms;
} UbigStageBRtControlDescriptor;

typedef struct {
    uint32_t output_index;
    const UbigStageBRtControlDescriptor *descriptor;
} UbigStageBRtControlGroup;

#define UBIG_STAGE_B_RT_CONTROL_GROUPS 4u
#define UBIG_STAGE_B_RT_CONTROL_SLOTS 7u
#define UBIG_STAGE_B_RT_CONTROL_RESULT_WORDS (1u+2u*UBIG_STAGE_B_RT_CONTROL_SLOTS)

/* Exact table-free scalar transfer at reference VA 0x18008CAA0. */
float ubig_stage_b_rt_control_transfer(float score,float gain,float bias);

/* Exact single-descriptor scorer at reference VA 0x18008CC38. output[0] is
 * the transfer value and output[1] is the raw weighted score. */
void ubig_stage_b_rt_control_score_process(const float *features,
                                           const UbigStageBRtControlDescriptor *descriptor,
                                           float output[2]);

/* Exact four-descriptor selector at reference VA 0x18008CE60. result_words[0]
 * receives the winning output_index; pair (2*i+1,2*i+2) receives the transfer
 * and raw score for each caller-selected slot i. */
void ubig_stage_b_rt_control_select_process(const float *features,
                                            const UbigStageBRtControlGroup groups[UBIG_STAGE_B_RT_CONTROL_GROUPS],
                                            uint32_t result_words[UBIG_STAGE_B_RT_CONTROL_RESULT_WORDS]);

#define UBIG_STAGE_B_RT_UNIVERSAL_FEATURES 262u
#define UBIG_STAGE_B_RT_EXTENDED_FEATURES 296u

typedef struct {
    uint32_t counter;
    uint32_t period;
    uint32_t cycle;
    uint32_t target;
    uint32_t reset;
    uint32_t armed;
    uint32_t primary_result[UBIG_STAGE_B_RT_CONTROL_RESULT_WORDS];
    float secondary_result[2];
    uint32_t updated;
} UbigStageBRtControlCadence;

typedef struct {
    UbigStageBRtControlGroup groups[UBIG_STAGE_B_RT_CONTROL_GROUPS];
    const UbigStageBRtControlDescriptor *secondary;
} UbigStageBRtControlCadenceConfig;

/* Exact slow control cadence beneath reference parent 0x18007B2F0. features is
 * the deployed flattened 262-float universal-analysis vector. On an evaluation
 * call it is halved in place before the four-way selector runs. The secondary
 * descriptor may address base indices 0..261 and appended indices 292..295. */
void ubig_stage_b_rt_control_cadence_process(UbigStageBRtControlCadence *state,
                                             const UbigStageBRtControlCadenceConfig *config,
                                             float features[UBIG_STAGE_B_RT_UNIVERSAL_FEATURES]);

typedef struct {
    uint32_t winner;
    float slot1_transfer;
    float slot2_transfer;
    float slot5_transfer;
    float slot6_transfer;
    float secondary_transfer;
} UbigStageBRtControlAggregateItem;

typedef struct {
    uint32_t enabled;
    float slot2_state;
    UbigStageBRtHysteresisState hysteresis;
    float smoothing_keep;
    float activity_alpha_low;
    float activity_alpha_high;
    float activity_state;
    float final_blend;
    float final_state;
} UbigStageBRtControlAggregateState;

#define UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS 5u

/* Exact scalar aggregation above reference parent 0x18007B2F0, corresponding
 * to reference VA 0x180058480 after the child result has been produced. */
void ubig_stage_b_rt_control_aggregate_process(UbigStageBRtControlAggregateState *state,
                                               const UbigStageBRtControlAggregateItem *items,
                                               uint32_t item_count,
                                               float output[UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS]);

typedef struct {
    float above_pivot_slope;
    float below_pivot_slope;
    float blend_keep;
    float blend_drive;
} UbigStageBRtPairBoundsConfig;

typedef struct {
    const UbigStageBRtPairBoundsConfig *config;
    uint32_t active_width;
    float baseline;
} UbigStageBRtPairBoundsState;

typedef struct {
    float down_keep;
    float down_inject;
    float up_keep;
    float up_inject;
} UbigStageBRtResidualMeanConfig;

typedef struct {
    const UbigStageBRtResidualMeanConfig *config;
    uint32_t active_width;
    float scalar;
} UbigStageBRtResidualMeanState;

/* Exact always-live 0x180080920 / 0x180080AE0 / 0x180080ED8 leaves.
 * The two averaging stages generate the reference reciprocal sequence on demand;
 * its sole legacy rounding quirk is the one-ulp-low 1/7 entry. */
void ubig_stage_b_rt_pair_bounds_process(float control,float subtract,
                                         float base_offset,float modulation_scale,
                                         UbigStageBRtPairBoundsState *state,
                                         const float *lower_source,
                                         const float *upper_source,
                                         float *lower_output,float *upper_output,
                                         const float *modulation);
void ubig_stage_b_rt_residual_balance_process(float alpha,const int32_t *status,
                                              const float *input,uint32_t count,
                                              float *primary,float *secondary);
void ubig_stage_b_rt_residual_mean_process(float gain,float bias,
                                           UbigStageBRtResidualMeanState *state,
                                           const float *primary_envelope,
                                           const float *lower_bound,
                                           const float *upper_bound,
                                           const int32_t *status,
                                           float *base_output,float *residual_output);



typedef struct {
    float primary_lower_limit;
    float primary_negative_slope;
    float primary_quadratic_scale;
    float primary_quadratic_limit;
    float primary_linear_offset;
    float secondary_lower_limit;
    float secondary_negative_slope;
    float secondary_cubic_scale;
    float secondary_cubic_limit;
    float secondary_linear_offset;
} UbigStageBRtDualEnvelopeConfig;

typedef struct {
    const UbigStageBRtDualEnvelopeConfig *config;
    uint32_t active_width;
    float primary[UBIG_STAGE_B_RT_MAX_BANDS];
    float secondary[UBIG_STAGE_B_RT_MAX_BANDS];
} UbigStageBRtDualEnvelopeState;

/* Exact always-live 0x18007FE80 dual-envelope stage and table-free semantic
 * form of the 0x18007FC08 three-neighbor smoother. */
void ubig_stage_b_rt_dual_envelope_process(float offset,
                                           UbigStageBRtDualEnvelopeState *state,
                                           const UbigStageBRtBandRows *rows,
                                           uint32_t status[UBIG_STAGE_B_RT_MAX_BANDS]);
void ubig_stage_b_rt_neighbor_smooth(uint32_t count,const int32_t *status,
                                     const float *input,float *output);

typedef struct {
    float smooth_keep;
    float smooth_inject;
    float lower_limit;
    float negative_slope;
    float quadratic_scale;
    float quadratic_limit;
    float linear_offset;
    const float *lane_weight;
} UbigStageBRtEnvelopeConfig;

typedef struct {
    const UbigStageBRtEnvelopeConfig *config;
    uint32_t active_width;
    uint32_t status[UBIG_STAGE_B_RT_MAX_BANDS];
    float envelope[UBIG_STAGE_B_RT_MAX_BANDS];
    float scalar_envelope;
    float activity_state;
    float lane_activity[UBIG_STAGE_B_RT_MAX_BANDS];
} UbigStageBRtEnvelopeState;

/* Exact 0x180080278 max-row envelope tracker and its enclosing 0x180080658
 * activity smoother. All curve/smoothing coefficients remain caller-owned. */
int ubig_stage_b_rt_envelope_track(UbigStageBRtEnvelopeState *state,
                                   const UbigStageBRtBandRows *rows,
                                   const float *lane_offset);
void ubig_stage_b_rt_envelope_activity_process(UbigStageBRtEnvelopeState *state,
                                               const UbigStageBRtBandRows *rows,
                                               const float *lane_offset);

typedef struct {
    uint32_t active_width;
    UbigStageBRtPairBoundsConfig pair_bounds;
    UbigStageBRtDualEnvelopeConfig dual_envelope;
    UbigStageBRtResidualMeanConfig residual_mean;
    UbigStageBRtEnvelopeConfig envelope;
    float post_new;
    float post_old;
} UbigStageBRtDeepControllerConfig;

typedef struct {
    float gain;
    float subtract;
    float bias;
    float base_offset;
    float dual_offset;
    float modulation_scale;
} UbigStageBRtDeepControllerControls;

typedef struct {
    const UbigStageBRtDeepControllerConfig *config;
    uint32_t mode;
    uint32_t row_count_cache;
    UbigStageBRtDualEnvelopeState dual;
    UbigStageBRtEnvelopeState envelope;
    UbigStageBRtPairBoundsState pair_bounds;
    UbigStageBRtResidualMeanState residual_mean;
    float intermediate[UBIG_STAGE_B_RT_MAX_BANDS];
    float output[UBIG_STAGE_B_RT_MAX_BANDS];
} UbigStageBRtDeepControllerState;

/* Exact deployed late Stage-B controller parent at 0x180064B38 plus its
 * one-time/row-count-change reset at 0x180064958. The semantic state composes
 * the independently exact native leaves rather than preserving raw DLL offsets. */
void ubig_stage_b_rt_deep_controller_reset(UbigStageBRtDeepControllerState *state,
                                           uint32_t row_count);
void ubig_stage_b_rt_deep_controller_process(float control,
                                             UbigStageBRtDeepControllerState *state,
                                             const float *lower_source,
                                             const float *upper_source,
                                             const int32_t *status,
                                             const UbigStageBRtDeepControllerControls *controls,
                                             UbigStageBRtBandRows *analysis_rows,
                                             UbigStageBRtBandRows *output_rows,
                                             int32_t *base_meter,
                                             int32_t *output_meter);

#define UBIG_STAGE_B_RT_FFT64_COMPLEX 64u
#define UBIG_STAGE_B_RT_FFT64_FLOATS 128u

/* Exact specialized forward complex FFT at 0x1800A68C0. The deployed
 * 0x49620 path fixes N=64; input/output are natural-order interleaved complex
 * binary32 values and the transform is unscaled. */
void ubig_stage_b_rt_fft64(float output[UBIG_STAGE_B_RT_FFT64_FLOATS],
                           const float input[UBIG_STAGE_B_RT_FFT64_FLOATS]);

/* Exact aligned four-lane max-absolute reducer at 0x1800BB6E0. Counts are
 * positive multiples of four; the deployed late Stage-B path uses 64. */
float ubig_stage_b_rt_max_abs4(const float *input,uint32_t count);

typedef struct {
    const float *kernel;
    uint32_t count;
    float reflected_scale_a;
    float reflected_scale_b;
    float forward_scale_a;
    float forward_scale_b;
    float *history; /* history slots x count */
} UbigStageBRtSymmetricHistoryMix;

/* Exact 0x18006DCF8 reflected/forward kernel history mixer. The deployed
 * count is 64 and therefore satisfies the reference four-float vector contract. */
void ubig_stage_b_rt_symmetric_history_mix(UbigStageBRtSymmetricHistoryMix *state,
                                             float *output,
                                             const float *input,
                                             uint32_t history_index);

typedef struct {
    const uint32_t *indices;
    const float *weights;
    uint32_t count;
} UbigStageBRtSparseMix;

/* Weighted sparse complex-row mixer recovered from 0x18004B890. Input rows
 * are indexed first by mix->indices[] and then by channel. A zero-count mix
 * clears 2*complex_bins output floats and returns 1; non-empty mixes return 0. */
int ubig_stage_b_rt_sparse_complex_mix(float *output,
                                       const float *const *const *rows,
                                       const UbigStageBRtSparseMix *mix,
                                       uint32_t channel,
                                       uint32_t complex_bins);

typedef struct {
    uint32_t row_count;
    uint32_t channel_count;
    uint32_t complex_bins;
    float ***rows;
} UbigStageBRtComplexMatrix;

typedef struct {
    const UbigStageBRtSparseMix *mixes;
    uint32_t source_rows;
    uint32_t target_rows;
} UbigStageBRtSparseRemapPlan;

/* Exact sparse row remapper at 0x18004BAB0. The source_rows prefix is mixed
 * through aligned workspace before write-back so source rows may overlap the
 * destination matrix; rows appended beyond the prefix are generated in place. */
uint32_t ubig_stage_b_rt_sparse_remap(UbigStageBRtComplexMatrix *matrix,
                                      const UbigStageBRtSparseRemapPlan *plan,
                                      void *workspace);

/* Exact five-word control export used by deployed outer parent 0x1800376B0:
 * scalar aggregation at 0x180058480 followed by signed-Q31 conversion of all
 * five outputs in their deployed 0x654..0x664 order. */
void ubig_stage_b_rt_control_export_process(UbigStageBRtControlAggregateState *state,
                                            const UbigStageBRtControlAggregateItem *items,
                                            uint32_t item_count,
                                            int32_t output[UBIG_STAGE_B_RT_CONTROL_AGGREGATE_OUTPUTS]);

/* Exact in-place two-row complex pair transform used immediately before and
 * after the universal Stage-B row workers. Scale is caller-owned. */
void ubig_stage_b_rt_pair_transform(float *row_a,float *row_b,
                                    uint32_t complex_bins,float scale);

/* Exact deployed unit-float to signed Q31 conversion used five times per
 * active outer Stage-B block. Values >= +1 saturate to INT32_MAX and values
 * <= -1 saturate to INT32_MIN; interior scaled values round to nearest-even. */
int32_t ubig_stage_b_rt_q31_encode(float value);

typedef struct {
    UbigStageBRtFeatureHistory feature_history;
    UbigStageBRtVariationHistory variation_history;
    UbigStageBRtSegmentRatioHistory segment_ratio_history;
    UbigStageBRtProjectionHistory projection_history;
    UbigStageBRtFeatureChangeHistory feature_change_history;
    UbigStageBRtSpectralChangeHistory spectral_change_history;
    UbigStageBRtPeakResidualHistory peak_residual_history;

    UbigStageBRtStatCursor segment_ratio_cursor;
    UbigStageBRtStatCursor variation_cursor;
    UbigStageBRtStatCursor spectral_change_cursor;
    UbigStageBRtStatCursor feature_change_cursor;
    UbigStageBRtStatCursor peak_residual_cursor;
    UbigStageBRtSchedulerClock clock;
} UbigStageBRtUniversalAnalysis;

typedef struct {
    const UbigStageBRtFeatureHistoryConfig *feature_history;
    const UbigStageBRtVariationConfig *variation;
    const UbigStageBRtSegmentRatioConfig *segment_ratio;
    const UbigStageBRtProjectionConfig *projection;
    uint32_t feature_cadence_step;
    uint32_t projection_cadence_step;
} UbigStageBRtUniversalConfig;

typedef struct {
    float feature_cadence[UBIG_STAGE_B_RT_FEATURE_CADENCE_OUTPUTS];
    float segment_ratio_mean[UBIG_STAGE_B_RT_STAT_COLUMNS];
    float segment_ratio_deviation[UBIG_STAGE_B_RT_STAT_COLUMNS];
    float variation_mean[UBIG_STAGE_B_RT_STAT_COLUMNS];
    float variation_deviation[UBIG_STAGE_B_RT_STAT_COLUMNS];
    float spectral_change[2];
    float projection_cadence[UBIG_STAGE_B_RT_CADENCE_OUTPUTS];
    float feature_change[2];
    float peak_rank[UBIG_STAGE_B_RT_RANK_OUTPUTS];
} UbigStageBRtUniversalOutput;

/* Native composition of the deployed universal analysis scheduler. Every
 * numerical child is independently bit-exact; this function owns only their
 * reference call order, shared-history lower-cadence views and cadence gates. */
void ubig_stage_b_rt_universal_analysis_process(UbigStageBRtUniversalAnalysis *state,
                                                const UbigStageBRtUniversalConfig *config,
                                                const UbigStageBRtSpectralExport *input,
                                                UbigStageBRtUniversalOutput *output);

/* Pack/unpack the exact deployed 262-float feature order observed in the live
 * lower-output pointer array beneath reference parent 0x18007B2F0. */
void ubig_stage_b_rt_universal_pack_features(const UbigStageBRtUniversalOutput *output,
                                             float features[UBIG_STAGE_B_RT_UNIVERSAL_FEATURES]);
void ubig_stage_b_rt_universal_unpack_features(UbigStageBRtUniversalOutput *output,
                                               const float features[UBIG_STAGE_B_RT_UNIVERSAL_FEATURES]);

typedef struct {
    UbigStageBRtSpectralAccumulator spectral;
    UbigStageBRtSpectralExport spectral_export;
    UbigStageBRtUniversalAnalysis analysis;
    UbigStageBRtUniversalOutput analysis_output;
    UbigStageBRtControlCadence control;
} UbigStageBRtAnalysisController;

typedef struct {
    const UbigStageBRtUniversalConfig *analysis;
    const UbigStageBRtControlCadenceConfig *control;
} UbigStageBRtAnalysisControllerConfig;

/* Semantic numerical/control core of reference parent 0x18007B2F0: spectral
 * accumulation, universal scheduler, deployed 262-feature layout, and slow
 * primary/secondary control cadence. */
void ubig_stage_b_rt_analysis_controller_process(UbigStageBRtAnalysisController *state,
                                                 const UbigStageBRtAnalysisControllerConfig *config,
                                                 const float *row0,
                                                 const float *row1);
#endif
