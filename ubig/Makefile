CC ?= gcc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror -ffp-contract=off
CPPFLAGS += -Iinclude -Isrc/core
BUILD := build
CORE_OBJS := $(BUILD)/engine.o $(BUILD)/profiles.o $(BUILD)/adapter256.o $(BUILD)/stage_a_limiter.o $(BUILD)/stage_a_math.o $(BUILD)/stage_a_export.o $(BUILD)/stage_a_synth.o $(BUILD)/stage_a_fft320.o $(BUILD)/stage_a_analyzer.o $(BUILD)/stage_a_compressor_primitives.o $(BUILD)/stage_a_compressor_worker.o $(BUILD)/stage_a_compressor.o $(BUILD)/stage_a_lowband.o $(BUILD)/stage_a_regulator.o
CONTROL_OBJS := $(BUILD)/control_page.o

.PHONY: all check clean
all: $(BUILD)/libubig-core.a $(BUILD)/ubigctl

$(BUILD):
	mkdir -p $@

$(BUILD)/engine.o: src/core/engine.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/profiles.o: src/core/profiles.c | $(BUILD)
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

$(BUILD)/stage_a_analyzer.o: src/core/stage_a_analyzer.c | $(BUILD)
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

$(BUILD)/test_stage_a_fft320: tests/test_stage_a_fft320.c $(BUILD)/stage_a_fft320.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_stage_a_compressor_primitives: tests/test_stage_a_compressor_primitives.c $(BUILD)/stage_a_compressor_primitives.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_compressor_worker: tests/test_stage_a_compressor_worker.c $(BUILD)/stage_a_compressor_primitives.o $(BUILD)/stage_a_compressor_worker.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_compressor: tests/test_stage_a_compressor.c $(BUILD)/stage_a_compressor_primitives.o $(BUILD)/stage_a_compressor_worker.o $(BUILD)/stage_a_compressor.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_lowband: tests/test_stage_a_lowband.c $(BUILD)/stage_a_math.o $(BUILD)/stage_a_lowband.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_stage_a_regulator: tests/test_stage_a_regulator.c $(BUILD)/stage_a_regulator.o $(BUILD)/stage_a_compressor_worker.o $(BUILD)/stage_a_compressor_primitives.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

check: $(BUILD)/test_adapter256 $(BUILD)/test_engine $(BUILD)/test_control $(BUILD)/test_stage_a_limiter $(BUILD)/test_stage_a_math $(BUILD)/test_stage_a_export $(BUILD)/test_stage_a_fft320 $(BUILD)/test_stage_a_compressor_primitives $(BUILD)/test_stage_a_compressor_worker $(BUILD)/test_stage_a_compressor $(BUILD)/test_stage_a_lowband $(BUILD)/test_stage_a_regulator
	$(BUILD)/test_adapter256
	$(BUILD)/test_engine
	$(BUILD)/test_control
	$(BUILD)/test_stage_a_limiter
	$(BUILD)/test_stage_a_math
	$(BUILD)/test_stage_a_export
	$(BUILD)/test_stage_a_fft320
	$(BUILD)/test_stage_a_compressor_primitives
	$(BUILD)/test_stage_a_compressor_worker
	$(BUILD)/test_stage_a_compressor
	$(BUILD)/test_stage_a_lowband
	$(BUILD)/test_stage_a_regulator

clean:
	rm -rf $(BUILD)

$(BUILD)/ubig-render: tools/ubig_render.c $(CORE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

render-tool: $(BUILD)/ubig-render
