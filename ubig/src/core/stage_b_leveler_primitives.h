#ifndef UBIG_STAGE_B_LEVELER_PRIMITIVES_H
#define UBIG_STAGE_B_LEVELER_PRIMITIVES_H
#include <stdint.h>

/* Exact coefficient mapper used by the SP11 Stage-B Volume-Leveler/DRC
 * adaptive controller. config[1]/config[2] are the two branch coefficients;
 * config[0] is not read by this bounded primitive. */
void ubig_stage_b_leveler_coeff_triplet(uint32_t mode,
                                        const float config[3],
                                        float blend,
                                        float history,
                                        float drive,
                                        float *out_a,
                                        float *out_b,
                                        float *out_adaptive);
/* Exact 80-slot adaptive-history accumulator used by the Leveler/DRC writer. */
typedef struct {
    float bins[51];
    float total;
    uint32_t count;
    uint32_t ring_bin[80];
    float ring_lo[80];
    float ring_hi[80];
    float ring_total[80];
    uint32_t ring_pos;
    float phase;
    uint32_t reset_max;
    float max_a;
    float max_b;
} UbigStageBLevelerHistory;

void ubig_stage_b_leveler_history_init(UbigStageBLevelerHistory *state);

void ubig_stage_b_leveler_history_update(UbigStageBLevelerHistory *state,
                                         float step,
                                         float value_a,
                                         float value_b);
/* Exact 17-float post-controller scalar-transfer curve builder/evaluator.
 * The builder updates only the dynamic fields owned by the reference helper;
 * caller-owned threshold/static fields remain untouched. */
void ubig_stage_b_leveler_curve_build(float curve[17],
                                      float anchor,
                                      float slope_control,
                                      float delta);
float ubig_stage_b_leveler_piecewise(const float curve[17], float input);

/* Exact bounded row-history/lifecycle primitive used by the Leveler producer. */
typedef struct {
    float *previous;
    float *current;
    uint32_t hold;
    int32_t event_age;
    float coefficient;
    uint32_t reserved;
} UbigStageBLevelerRowState;

typedef struct {
    uint32_t reserved;
    float delta_threshold;
    float release;
    uint32_t hold_limit;
} UbigStageBLevelerRowConfig;

typedef struct {
    uint32_t event;
    uint32_t hold_expired;
    float coefficient;
} UbigStageBLevelerRowResult;

void ubig_stage_b_leveler_row_update(UbigStageBLevelerRowState *state,
                                     const UbigStageBLevelerRowConfig *config,
                                     const float *input,
                                     uint32_t count,
                                     uint32_t force_event,
                                     UbigStageBLevelerRowResult *result,
                                     float metric);

/* Exact producer-side per-band floor clamp. Valid SP11 rows have >=7 lanes. */
void ubig_stage_b_leveler_apply_row_floors(uint32_t count,float *values);

/* Exact row preparation/linking subpath used by the Leveler producer. */
typedef struct {
    uint32_t count;
    uint32_t width;
    float **rows;
} UbigStageBLevelerInputRows;

typedef struct {
    uint32_t count;
    uint32_t width;
    float **rows;
    uint32_t row_capacity;
    uint32_t width_capacity;
} UbigStageBLevelerPreparedRows;

void ubig_stage_b_leveler_prepare_rows(const float *base,
                                       const UbigStageBLevelerInputRows *input,
                                       UbigStageBLevelerPreparedRows *output,
                                       float bias);

/* Exact per-lane row transition helper used by the Leveler producer. */
typedef struct {
    float previous_offset;
    float input_offset;
    float rise_previous;
    float rise_input;
    float fall_previous;
    float fall_input;
} UbigStageBLevelerTransitionRecord;

float *ubig_stage_b_leveler_transition_row(const float *input,
                                           uint32_t count,
                                           uint32_t copy_only,
                                           uint32_t common_config,
                                           const UbigStageBLevelerTransitionRecord *large_rise,
                                           const UbigStageBLevelerTransitionRecord *normal,
                                           float *state,
                                           float rise_threshold);

/* Exact symmetric finite-row filter used by the Leveler producer. */
typedef struct {
    const float *coefficients;
    const float *post_scale;
    uint32_t taps;
    uint32_t reserved;
} UbigStageBLevelerSymmetricFilter;

void ubig_stage_b_leveler_symmetric_filter(const UbigStageBLevelerSymmetricFilter *filter,
                                           const float *input,
                                           uint32_t count,
                                           float *output);

/* Exact symmetric-filter + conditional overshoot blend wrapper. */
void ubig_stage_b_leveler_filter_blend(const UbigStageBLevelerSymmetricFilter *filter,
                                       uint32_t count,
                                       const float *blend,
                                       const float *input,
                                       float *output);

/* Exact in-place ceiling clamp over scalar+row storage (count+1 floats). */
void ubig_stage_b_leveler_row_ceiling(float *values,uint32_t count,float ceiling);

/* Exact two-state coefficient selector/smoother used by the Leveler mixer. */
typedef struct {
    float positive_near;
    float positive_far;
    float neutral_primary;
    float negative_primary;
    float neutral_mix;
    float negative_mix;
} UbigStageBLevelerPairCoefficients;

typedef struct {
    float base;
    float negative;
    float alternate;
    uint32_t negative_mode;
    uint32_t compare_enable;
    uint32_t use_alternate;
} UbigStageBLevelerPairControl;

/* Exact centered-distribution statistic used by the Leveler producer. */
float ubig_stage_b_leveler_distribution_stat(uint32_t count,const float *values);

void ubig_stage_b_leveler_pair_smooth(const UbigStageBLevelerPairCoefficients *coefficients,
                                      const UbigStageBLevelerPairControl *control,
                                      float *state_a,
                                      float *state_b,
                                      float target_a,
                                      float target_b,
                                      float mix);
#endif
