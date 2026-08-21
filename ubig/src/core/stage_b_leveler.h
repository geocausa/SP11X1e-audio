#ifndef UBIG_STAGE_B_LEVELER_H
#define UBIG_STAGE_B_LEVELER_H
#include <stdint.h>
#include "stage_b_leveler_primitives.h"

typedef struct {
    float *values;
    float scalar;
    uint32_t reserved;
} UbigStageBLevelerRecord;

typedef struct {
    float history_step;
    float coeff_a;
    float coeff_b;
    uint32_t hold_limit;
    float base_decay;
    float adaptive_smooth;
} UbigStageBLevelerConfig;

typedef struct {
    float base;
    uint32_t hold_count;
    float adaptive_state;
    uint32_t reserved;
    UbigStageBLevelerRecord *primary;
    UbigStageBLevelerRecord *secondary;
    UbigStageBLevelerHistory history;
} UbigStageBLevelerState;

/* Reset the active Leveler controller lifecycle without reallocating record
 * storage. Primary records and pointer topology remain untouched. */
void ubig_stage_b_leveler_reset(UbigStageBLevelerState *state,
                                uint32_t record_count,
                                uint32_t width);

/* Apply the exact pair smoother to one scalar target pair and its row values.
 * target_b_flat stores scalar at [0], followed by count value lanes. */
void ubig_stage_b_leveler_pair_row(const UbigStageBLevelerPairCoefficients *coefficients,
                                   const float *target_b_flat,
                                   const UbigStageBLevelerPairControl *control,
                                   uint32_t count,
                                   const float *mix_values,
                                   const UbigStageBLevelerRecord *target_a,
                                   float *state_a_scalar,
                                   float *state_a_values,
                                   float *state_b_scalar,
                                   float *state_b_values,
                                   float scalar_mix);

/* Project one record family against another through the exact 17-float
 * scalar-transfer curve. output_rows stores 21 floats per row: scalar+20. */
void ubig_stage_b_leveler_curve_rows(const float curve[17],
                                     const UbigStageBLevelerRecord *anchor_records,
                                     const UbigStageBLevelerRecord *input_records,
                                     uint32_t width,
                                     uint32_t index,
                                     float *output_rows,
                                     float anchor_bias,
                                     float input_bias);

/* Clamp producer rows using curve-derived bounds propagated from one indexed
 * record backward through preceding records. output_rows is 21 floats/row. */
void ubig_stage_b_leveler_curve_bounds(const float *limits,
                                       const UbigStageBLevelerRecord *anchor_records,
                                       const UbigStageBLevelerRecord *compare_records,
                                       const float curve[17],
                                       uint32_t width,
                                       uint32_t index,
                                       float *output_rows,
                                       float anchor_bias,
                                       float input_bias);

/* Propagate a ceiling across linked producer rows. The logarithmic threshold
 * vector is supplied as caller-owned configuration rather than embedded data. */
void ubig_stage_b_leveler_link_ceiling(const UbigStageBLevelerRecord *records,
                                       const UbigStageBLevelerRecord *compare_records,
                                       const float *thresholds,
                                       uint32_t width,
                                       uint32_t index,
                                       float *output_rows);

/* Complete curve-projection/bounds/link/final-mix pipeline. Logarithmic
 * thresholds remain explicit caller-owned configuration. */
void ubig_stage_b_leveler_curve_pipeline(const float curve[17],
                                         const UbigStageBLevelerRecord *records,
                                         const UbigStageBLevelerRecord *compare_records,
                                         const float *limits,
                                         const float *thresholds,
                                         uint32_t width,
                                         uint32_t index,
                                         float *output_rows,
                                         uint32_t preserve_rows,
                                         float anchor_bias,
                                         float input_bias);


#define UBIG_STAGE_B_LEVELER_PRODUCER_ROWS 4u
#define UBIG_STAGE_B_LEVELER_PRODUCER_WIDTH 20u

typedef struct {
    float state_b_values[UBIG_STAGE_B_LEVELER_PRODUCER_ROWS][UBIG_STAGE_B_LEVELER_PRODUCER_WIDTH];
    float state_b_scalar[UBIG_STAGE_B_LEVELER_PRODUCER_ROWS];
    float state_a_values[UBIG_STAGE_B_LEVELER_PRODUCER_ROWS][UBIG_STAGE_B_LEVELER_PRODUCER_WIDTH];
    float state_a_scalar[UBIG_STAGE_B_LEVELER_PRODUCER_ROWS];
    uint32_t negative_mode;
    uint32_t hold_count;
} UbigStageBLevelerProducerState;

typedef struct {
    const UbigStageBLevelerSymmetricFilter *filter;
    UbigStageBLevelerPairCoefficients pair;
    float exp_drive;
    uint32_t hold_limit;
} UbigStageBLevelerProducerConfig;

typedef struct {
    float **row_ptrs;
    UbigStageBLevelerRecord *records;
} UbigStageBLevelerProducerRows;

#define UBIG_STAGE_B_LEVELER_ADAPTIVE_WIDTH 20u
#define UBIG_STAGE_B_LEVELER_ADAPTIVE_INDEX 2u

typedef struct {
    float *fast;
    float *slow;
} UbigStageBLevelerAdaptiveState;

typedef struct {
    const float *rise_mix;
    const float *blend;
    float mix;
    uint32_t reserved;
} UbigStageBLevelerAdaptiveControl;

typedef struct {
    const float *source;
    float gate;
    uint32_t reserved;
} UbigStageBLevelerSourceGate;

/* Exact SP11 20-band/index-2 adaptive filter/update block. The 20-band
 * statistic vector is caller-owned configuration and is not embedded by UbiG. */
void ubig_stage_b_leveler_adaptive_filter_process(
        UbigStageBLevelerAdaptiveState *state,
        const UbigStageBLevelerAdaptiveControl *control,
        const UbigStageBLevelerSymmetricFilter *filter,
        const UbigStageBLevelerSourceGate *source_gate,
        const float *reference_source,
        const float *band_weights,
        uint32_t emit,
        uint32_t reset,
        uint32_t direct_update,
        UbigStageBLevelerProducerRows *output,
        int32_t *telemetry,
        float reference_bias,
        float output_scale,
        float slow_mix,
        float rise_modulation,
        float target_scale);

/* Exact linked-row Leveler producer. The logarithmic threshold vector remains
 * explicit caller-owned configuration; no reference-image table is embedded. */
void ubig_stage_b_leveler_producer_process(UbigStageBLevelerProducerState *state,
                                           const UbigStageBLevelerProducerConfig *config,
                                           const UbigStageBLevelerRecord *input_records,
                                           const UbigStageBLevelerRecord *anchor_records,
                                           uint32_t update_mode,
                                           uint32_t width,
                                           uint32_t index,
                                           const float curve[17],
                                           float control0,
                                           float control1,
                                           float curve_bias,
                                           float input_bias,
                                           const UbigStageBLevelerPairCoefficients *override_coefficients,
                                           uint32_t reset,
                                           float *error_rows,
                                           UbigStageBLevelerProducerRows *output,
                                           uint32_t preserve_rows,
                                           const float *log_thresholds);

/* Process one indexed adaptive record and its preceding/related vector records.
 * observed_records supplies read-only instantaneous targets. */
void ubig_stage_b_leveler_update(UbigStageBLevelerState *state,
                                 const UbigStageBLevelerConfig *config,
                                 uint32_t index,
                                 uint32_t width,
                                 float control_mix,
                                 float direct_control,
                                 float secondary_scale,
                                 const UbigStageBLevelerRecord *observed_records);


#define UBIG_STAGE_B_LEVELER_PARENT_ROWS 2u
#define UBIG_STAGE_B_LEVELER_PARENT_WIDTH 20u

typedef struct {
    float **matrix_rows;
    UbigStageBLevelerLookupState *lookup;
    UbigStageBLevelerRowState *lifecycle;
    float **transition_rows;
    UbigStageBLevelerProducerState *producer;
    UbigStageBLevelerState *writer;
    UbigStageBLevelerAdaptiveState *adaptive;
} UbigStageBLevelerParentState;

typedef struct {
    const float *base_row;
    const UbigStageBLevelerRowConfig *lifecycle;
    const UbigStageBLevelerLookupConfig *lookup;
    const UbigStageBLevelerTransitionRecord *transition_large_rise;
    const UbigStageBLevelerTransitionRecord *transition_normal;
    const UbigStageBLevelerAdaptiveControl *adaptive;
    const UbigStageBLevelerSymmetricFilter *filter;
    const UbigStageBLevelerTransitionRecord *matrix_transition;
    const UbigStageBLevelerConfig *writer;
    const UbigStageBLevelerProducerConfig *producer;
} UbigStageBLevelerParentConfig;

typedef struct {
    const UbigStageBLevelerLookupTables *lookup_tables;
    const UbigStageBLevelerInverseLookupTables *inverse_tables;
    const UbigStageBLevelerNormalizedCubic *cubic;
    const float *lookup_offsets;
    const float *producer_thresholds;
    const float *adaptive_band_weights;
    const float *tail_coefficients;
} UbigStageBLevelerParentTuning;

typedef struct {
    float matrix_bias;
    float row_bias_a;
    float row_bias_b;
    float adaptive_output_scale;
    float adaptive_target_scale;
    float lookup_control;
    uint32_t adaptive_emit;
    uint32_t lifecycle_force;
    uint32_t adaptive_direct;
    uint32_t preserve_rows;
} UbigStageBLevelerParentControl;

/* Exact deployed SP11 stereo Leveler parent. The shipped profile policy keeps
 * the legacy negative-remap branch disabled, so that branch and its private
 * tables are intentionally absent. All remaining non-algorithmic tables and
 * vectors are explicit caller-owned tuning. Input/output rows are both updated
 * in place, matching the original parent boundary. */
float ubig_stage_b_leveler_parent_process(UbigStageBLevelerParentState *state,
                                          const UbigStageBLevelerParentConfig *config,
                                          const UbigStageBLevelerParentTuning *tuning,
                                          const UbigStageBLevelerParentControl *control,
                                          const float previous_curve[17],
                                          const float curve_template[18],
                                          const UbigStageBLevelerPairCoefficients *override_coefficients,
                                          const UbigStageBLevelerSourceGate *source_gate,
                                          UbigStageBLevelerInputRows *input,
                                          UbigStageBLevelerInputRows *output,
                                          int32_t *telemetry);

typedef struct {
    float smoothed_limit;
    float parent_result;
    uint32_t force_target;
    uint32_t adaptive_direct;
} UbigStageBLevelerWrapperState;

typedef struct {
    uint32_t enabled;
    uint32_t adaptive_emit;
    uint32_t target_scale_override;
    uint32_t lookup_control_override;
    uint32_t preserve_rows;
    float smoothing_step;
    float base_limit;
    float target_limit;
    float adaptive_output_scale;
    float adaptive_target_scale;
    float lookup_control;
} UbigStageBLevelerWrapperConfig;

/* Exact deployed control wrapper around the stereo Leveler parent. The
 * reference's alternate generated-control branch is not used by any shipped
 * SP11 profile and is intentionally absent. previous_curve/curve_template and
 * all parent tuning remain caller-owned. */
void ubig_stage_b_leveler_wrapper_process(UbigStageBLevelerWrapperState *state,
                                          const UbigStageBLevelerWrapperConfig *config,
                                          UbigStageBLevelerParentState *parent_state,
                                          const UbigStageBLevelerParentConfig *parent_config,
                                          const UbigStageBLevelerParentTuning *parent_tuning,
                                          const float previous_curve[17],
                                          const float curve_template[18],
                                          const UbigStageBLevelerSourceGate *source_gate,
                                          UbigStageBLevelerInputRows *input,
                                          UbigStageBLevelerInputRows *output,
                                          int32_t *telemetry,
                                          float *control_a,
                                          float *control_b);
#endif
