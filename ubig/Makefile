CC ?= gcc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror -ffp-contract=off
CPPFLAGS += -Iinclude -Isrc/core
BUILD := build
CORE_OBJS := $(BUILD)/engine.o $(BUILD)/profiles.o $(BUILD)/adapter256.o $(BUILD)/stage_a_limiter.o
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
$(BUILD)/control_page.o: src/control/control_page.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/libubig-core.a: $(CORE_OBJS)
	ar rcs $@ $^

$(BUILD)/ubigctl: tools/ubigctl.c $(CORE_OBJS) $(CONTROL_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD)/test_adapter256: tests/test_adapter256.c $(BUILD)/adapter256.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
$(BUILD)/test_engine: tests/test_engine.c $(CORE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
$(BUILD)/test_control: tests/test_control.c $(BUILD)/profiles.o $(CONTROL_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
$(BUILD)/test_stage_a_limiter: tests/test_stage_a_limiter.c $(BUILD)/stage_a_limiter.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

check: $(BUILD)/test_adapter256 $(BUILD)/test_engine $(BUILD)/test_control $(BUILD)/test_stage_a_limiter
	$(BUILD)/test_adapter256
	$(BUILD)/test_engine
	$(BUILD)/test_control
	$(BUILD)/test_stage_a_limiter

clean:
	rm -rf $(BUILD)

$(BUILD)/ubig-render: tools/ubig_render.c $(CORE_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

render-tool: $(BUILD)/ubig-render
