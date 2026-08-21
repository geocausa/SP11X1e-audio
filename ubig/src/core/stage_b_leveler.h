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
#endif
