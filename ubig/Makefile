CC ?= gcc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror -ffp-contract=off
CPPFLAGS += -Iinclude -Isrc/core
BUILD := build
CORE_OBJS := $(BUILD)/engine.o $(BUILD)/profiles.o $(BUILD)/adapter256.o $(BUILD)/stage_a_limiter.o $(BUILD)/stage_a_math.o $(BUILD)/stage_a_export.o $(BUILD)/stage_a_synth.o $(BUILD)/stage_a_fft320.o $(BUILD)/stage_a_fft320_sp11.o $(BUILD)/stage_a_analyzer.o $(BUILD)/stage_a_filterbank_sp11.o $(BUILD)/stage_a_sp11_tuning.o $(BUILD)/stage_a_core_helpers.o $(BUILD)/stage_a_core.o $(BUILD)/stage_a_compressor_primitives.o $(BUILD)/stage_a_compressor_worker.o $(BUILD)/stage_a_compressor.o $(BUILD)/stage_a_lowband.o $(BUILD)/stage_a_regulator.o $(BUILD)/stage_b_leveler_primitives.o $(BUILD)/stage_b_leveler.o $(BUILD)/stage_b_rt.o
CONTROL_OBJS := $(BUILD)/control_page.o

.PHONY: all check clean
all: $(BUILD)/libubig-core.a $(BUILD)/ubigctl

$(BUILD):
	mkdir -p $@

$(BUILD)/engine.o: src/core/engine.c src/core/profiles_internal.h src/core/stage_a_core.h src/core/stage_a_sp11_tuning.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/profiles.o: src/core/profiles.c src/core/profiles_internal.h src/core/stage_a_sp11_tuning.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/adapter256.o: src/core/adapter256.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/stage_a_limiter.o: src/core/stage_a_limiter.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/stage_a_math.o: src/core/stage_a_math.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_export.o: src/core/stage_a_export.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_synth.o: src/core/stage_a_synth.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_fft320.o: src/core/stage_a_fft320.c src/core/stage_a_fft320_tables.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/core/stage_a_fft320.c -o $@
$(BUILD)/stage_a_fft320_sp11.o: src/core/stage_a_fft320_sp11.c src/core/stage_a_fft320_sp11.h src/core/stage_a_fft320_tables.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/core/stage_a_fft320_sp11.c -o $@

$(BUILD)/stage_a_analyzer.o: src/core/stage_a_analyzer.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_filterbank_sp11.o: src/core/stage_a_filterbank_sp11.c src/core/stage_a_filterbank_sp11_tables.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/core/stage_a_filterbank_sp11.c -o $@

$(BUILD)/stage_a_sp11_tuning.o: src/core/stage_a_sp11_tuning.c src/core/stage_a_sp11_tuning.h src/core/stage_a_sp11_tuning_tables.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/core/stage_a_sp11_tuning.c -o $@

$(BUILD)/stage_a_core_helpers.o: src/core/stage_a_core_helpers.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_core.o: src/core/stage_a_core.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_compressor_primitives.o: src/core/stage_a_compressor_primitives.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_compressor_worker.o: src/core/stage_a_compressor_worker.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_compressor.o: src/core/stage_a_compressor.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_lowband.o: src/core/stage_a_lowband.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/stage_a_regulator.o: src/core/stage_a_regulator.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/stage_b_leveler_primitives.o: src/core/stage_b_leveler_primitives.c src/core/stage_b_leveler_primitives.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/stage_b_leveler.o: src/core/stage_b_leveler.c src/core/stage_b_leveler.h src/core/stage_b_leveler_primitives.h src/core/stage_a_math.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/stage_b_rt.o: src/core/stage_b_rt.c src/core/stage_b_rt.h src/core/stage_a_math.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/control_page.o: src/control/control_page.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/libubig-core.a: $(CORE_OBJS)
	ar rcs $@ $^

$(BUILD)/ubigctl: tools/ubigctl.c $(CORE_OBJS) $(CONTROL_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_adapter256: tests/test_adapter256.c $(BUILD)/adapter256.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
$(BUILD)/test_engine: tests/test_engine.c $(CORE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_control: tests/test_control.c $(BUILD)/profiles.o $(CONTROL_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
$(BUILD)/test_stage_a_limiter: tests/test_stage_a_limiter.c $(BUILD)/stage_a_limiter.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_export: tests/test_stage_a_export.c $(BUILD)/stage_a_export.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_a_math: tests/test_stage_a_math.c $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_fft320: tests/test_stage_a_fft320.c $(BUILD)/stage_a_fft320.o $(BUILD)/stage_a_fft320_sp11.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_filterbank_sp11: tests/test_stage_a_filterbank_sp11.c $(BUILD)/stage_a_filterbank_sp11.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD)/test_stage_a_sp11_tuning: tests/test_stage_a_sp11_tuning.c $(BUILD)/stage_a_sp11_tuning.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD)/test_stage_a_core_helpers: tests/test_stage_a_core_helpers.c $(BUILD)/stage_a_core_helpers.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_a_core: tests/test_stage_a_core.c $(CORE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_a_compressor_primitives: tests/test_stage_a_compressor_primitives.c $(BUILD)/stage_a_compressor_primitives.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_compressor_worker: tests/test_stage_a_compressor_worker.c $(BUILD)/stage_a_compressor_primitives.o $(BUILD)/stage_a_compressor_worker.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_compressor: tests/test_stage_a_compressor.c $(BUILD)/stage_a_compressor_primitives.o $(BUILD)/stage_a_compressor_worker.o $(BUILD)/stage_a_compressor.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_compressor_init: tests/test_stage_a_compressor_init.c $(BUILD)/stage_a_compressor_primitives.o $(BUILD)/stage_a_compressor_worker.o $(BUILD)/stage_a_compressor.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_lowband: tests/test_stage_a_lowband.c $(BUILD)/stage_a_math.o $(BUILD)/stage_a_lowband.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_regulator: tests/test_stage_a_regulator.c $(BUILD)/stage_a_regulator.o $(BUILD)/stage_a_compressor_worker.o $(BUILD)/stage_a_compressor_primitives.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_b_leveler_primitives: tests/test_stage_b_leveler_primitives.c $(BUILD)/stage_b_leveler_primitives.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_b_leveler: tests/test_stage_b_leveler.c $(BUILD)/stage_b_leveler.o $(BUILD)/stage_b_leveler_primitives.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_b_leveler_parent: tests/test_stage_b_leveler_parent.c $(BUILD)/stage_b_leveler.o $(BUILD)/stage_b_leveler_primitives.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_leveler_wrapper: tests/test_stage_b_leveler_wrapper.c $(BUILD)/stage_b_leveler.o $(BUILD)/stage_b_leveler_primitives.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt: tests/test_stage_b_rt.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_hysteresis: tests/test_stage_b_rt_hysteresis.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_spectral: tests/test_stage_b_rt_spectral.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_variation: tests/test_stage_b_rt_variation.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_spectral_change: tests/test_stage_b_rt_spectral_change.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_ratio_mode: tests/test_stage_b_rt_ratio_mode.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_segment_ratio: tests/test_stage_b_rt_segment_ratio.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_peak_residual: tests/test_stage_b_rt_peak_residual.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_feature_change: tests/test_stage_b_rt_feature_change.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_scaled_sum: tests/test_stage_b_rt_scaled_sum.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_deviation32: tests/test_stage_b_rt_deviation32.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_spectrum32: tests/test_stage_b_rt_spectrum32.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_slope32: tests/test_stage_b_rt_slope32.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_slope32_features: tests/test_stage_b_rt_slope32_features.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_cadence_summary: tests/test_stage_b_rt_cadence_summary.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_rank_metrics: tests/test_stage_b_rt_rank_metrics.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_stat32: tests/test_stage_b_rt_stat32.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_stat_columns: tests/test_stage_b_rt_stat_columns.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_stat_ring: tests/test_stage_b_rt_stat_ring.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_feature_history: tests/test_stage_b_rt_feature_history.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_feature_mean: tests/test_stage_b_rt_feature_mean.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_feature_cadence: tests/test_stage_b_rt_feature_cadence.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_scheduler: tests/test_stage_b_rt_scheduler.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_control_select: tests/test_stage_b_rt_control_select.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_analysis_controller: tests/test_stage_b_rt_analysis_controller.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_control_aggregate: tests/test_stage_b_rt_control_aggregate.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_control_export: tests/test_stage_b_rt_control_export.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_sparse_mix: tests/test_stage_b_rt_sparse_mix.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_sparse_remap: tests/test_stage_b_rt_sparse_remap.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_symmetric_history: tests/test_stage_b_rt_symmetric_history.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_max_abs4: tests/test_stage_b_rt_max_abs4.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_envelope: tests/test_stage_b_rt_envelope.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_dual_envelope: tests/test_stage_b_rt_dual_envelope.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_neighbor_smooth: tests/test_stage_b_rt_neighbor_smooth.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_deep_residual: tests/test_stage_b_rt_deep_residual.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_deep_controller: tests/test_stage_b_rt_deep_controller.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_linked_accumulate: tests/test_stage_b_rt_linked_accumulate.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_late_pipeline: tests/test_stage_b_rt_late_pipeline.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_telemetry_smooth: tests/test_stage_b_rt_telemetry_smooth.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_q31: tests/test_stage_b_rt_q31.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_pair_transform: tests/test_stage_b_rt_pair_transform.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_fft64: tests/test_stage_b_rt_fft64.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_transform64: tests/test_stage_b_rt_transform64.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_late_controller: tests/test_stage_b_rt_late_controller.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_rank_history: tests/test_stage_b_rt_rank_history.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_projection_history: tests/test_stage_b_rt_projection_history.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_b_rt_parent: tests/test_stage_b_rt_parent.c $(BUILD)/stage_b_rt.o $(BUILD)/stage_a_math.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

check: $(BUILD)/test_adapter256 $(BUILD)/test_engine $(BUILD)/test_control $(BUILD)/test_stage_a_limiter $(BUILD)/test_stage_a_math $(BUILD)/test_stage_a_export $(BUILD)/test_stage_a_fft320 $(BUILD)/test_stage_a_filterbank_sp11 $(BUILD)/test_stage_a_sp11_tuning $(BUILD)/test_stage_a_core_helpers $(BUILD)/test_stage_a_core $(BUILD)/test_stage_a_compressor_primitives $(BUILD)/test_stage_a_compressor_worker $(BUILD)/test_stage_a_compressor $(BUILD)/test_stage_a_compressor_init $(BUILD)/test_stage_a_lowband $(BUILD)/test_stage_a_regulator $(BUILD)/test_stage_b_leveler_primitives $(BUILD)/test_stage_b_leveler $(BUILD)/test_stage_b_leveler_parent $(BUILD)/test_stage_b_leveler_wrapper $(BUILD)/test_stage_b_rt $(BUILD)/test_stage_b_rt_hysteresis $(BUILD)/test_stage_b_rt_spectral $(BUILD)/test_stage_b_rt_variation $(BUILD)/test_stage_b_rt_spectral_change $(BUILD)/test_stage_b_rt_ratio_mode $(BUILD)/test_stage_b_rt_segment_ratio $(BUILD)/test_stage_b_rt_peak_residual $(BUILD)/test_stage_b_rt_feature_change $(BUILD)/test_stage_b_rt_scaled_sum $(BUILD)/test_stage_b_rt_deviation32 $(BUILD)/test_stage_b_rt_spectrum32 $(BUILD)/test_stage_b_rt_slope32 $(BUILD)/test_stage_b_rt_slope32_features $(BUILD)/test_stage_b_rt_cadence_summary $(BUILD)/test_stage_b_rt_rank_metrics $(BUILD)/test_stage_b_rt_stat32 $(BUILD)/test_stage_b_rt_stat_columns $(BUILD)/test_stage_b_rt_stat_ring $(BUILD)/test_stage_b_rt_feature_history $(BUILD)/test_stage_b_rt_feature_mean $(BUILD)/test_stage_b_rt_feature_cadence $(BUILD)/test_stage_b_rt_scheduler $(BUILD)/test_stage_b_rt_control_select $(BUILD)/test_stage_b_rt_analysis_controller $(BUILD)/test_stage_b_rt_control_aggregate $(BUILD)/test_stage_b_rt_control_export $(BUILD)/test_stage_b_rt_sparse_mix $(BUILD)/test_stage_b_rt_sparse_remap $(BUILD)/test_stage_b_rt_symmetric_history $(BUILD)/test_stage_b_rt_max_abs4 $(BUILD)/test_stage_b_rt_envelope $(BUILD)/test_stage_b_rt_dual_envelope $(BUILD)/test_stage_b_rt_neighbor_smooth $(BUILD)/test_stage_b_rt_deep_residual $(BUILD)/test_stage_b_rt_deep_controller $(BUILD)/test_stage_b_rt_linked_accumulate $(BUILD)/test_stage_b_rt_late_pipeline $(BUILD)/test_stage_b_rt_telemetry_smooth $(BUILD)/test_stage_b_rt_q31 $(BUILD)/test_stage_b_rt_pair_transform $(BUILD)/test_stage_b_rt_fft64 $(BUILD)/test_stage_b_rt_transform64 $(BUILD)/test_stage_b_rt_late_controller $(BUILD)/test_stage_b_rt_rank_history $(BUILD)/test_stage_b_rt_projection_history $(BUILD)/test_stage_b_rt_parent
	$(BUILD)/test_adapter256
	$(BUILD)/test_engine
	$(BUILD)/test_control
	$(BUILD)/test_stage_a_limiter
	$(BUILD)/test_stage_a_math
	$(BUILD)/test_stage_a_export
	$(BUILD)/test_stage_a_fft320
	$(BUILD)/test_stage_a_filterbank_sp11
	$(BUILD)/test_stage_a_sp11_tuning
	$(BUILD)/test_stage_a_core_helpers
	$(BUILD)/test_stage_a_core
	$(BUILD)/test_stage_a_compressor_primitives
	$(BUILD)/test_stage_a_compressor_worker
	$(BUILD)/test_stage_a_compressor
	$(BUILD)/test_stage_a_compressor_init
	$(BUILD)/test_stage_a_lowband
	$(BUILD)/test_stage_a_regulator
	$(BUILD)/test_stage_b_leveler_primitives
	$(BUILD)/test_stage_b_leveler
	$(BUILD)/test_stage_b_leveler_parent
	$(BUILD)/test_stage_b_leveler_wrapper
	$(BUILD)/test_stage_b_rt
	$(BUILD)/test_stage_b_rt_hysteresis
	$(BUILD)/test_stage_b_rt_spectral
	$(BUILD)/test_stage_b_rt_variation
	$(BUILD)/test_stage_b_rt_spectral_change
	$(BUILD)/test_stage_b_rt_ratio_mode
	$(BUILD)/test_stage_b_rt_segment_ratio
	$(BUILD)/test_stage_b_rt_peak_residual
	$(BUILD)/test_stage_b_rt_feature_change
	$(BUILD)/test_stage_b_rt_scaled_sum
	$(BUILD)/test_stage_b_rt_deviation32
	$(BUILD)/test_stage_b_rt_spectrum32
	$(BUILD)/test_stage_b_rt_slope32
	$(BUILD)/test_stage_b_rt_slope32_features
	$(BUILD)/test_stage_b_rt_cadence_summary
	$(BUILD)/test_stage_b_rt_rank_metrics
	$(BUILD)/test_stage_b_rt_stat32
	$(BUILD)/test_stage_b_rt_stat_columns
	$(BUILD)/test_stage_b_rt_stat_ring
	$(BUILD)/test_stage_b_rt_feature_history
	$(BUILD)/test_stage_b_rt_feature_mean
	$(BUILD)/test_stage_b_rt_feature_cadence
	$(BUILD)/test_stage_b_rt_scheduler
	$(BUILD)/test_stage_b_rt_control_select
	$(BUILD)/test_stage_b_rt_analysis_controller
	$(BUILD)/test_stage_b_rt_control_aggregate
	$(BUILD)/test_stage_b_rt_control_export
	$(BUILD)/test_stage_b_rt_sparse_mix
	$(BUILD)/test_stage_b_rt_sparse_remap
	$(BUILD)/test_stage_b_rt_symmetric_history
	$(BUILD)/test_stage_b_rt_max_abs4
	$(BUILD)/test_stage_b_rt_envelope
	$(BUILD)/test_stage_b_rt_dual_envelope
	$(BUILD)/test_stage_b_rt_neighbor_smooth
	$(BUILD)/test_stage_b_rt_deep_residual
	$(BUILD)/test_stage_b_rt_deep_controller
	$(BUILD)/test_stage_b_rt_linked_accumulate
	$(BUILD)/test_stage_b_rt_late_pipeline
	$(BUILD)/test_stage_b_rt_telemetry_smooth
	$(BUILD)/test_stage_b_rt_q31
	$(BUILD)/test_stage_b_rt_pair_transform
	$(BUILD)/test_stage_b_rt_fft64
	$(BUILD)/test_stage_b_rt_transform64
	$(BUILD)/test_stage_b_rt_late_controller
	$(BUILD)/test_stage_b_rt_rank_history
	$(BUILD)/test_stage_b_rt_projection_history
	$(BUILD)/test_stage_b_rt_parent

clean:
	rm -rf $(BUILD)

$(BUILD)/ubig-render: tools/ubig_render.c $(CORE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

render-tool: $(BUILD)/ubig-render
