CC ?= aarch64-linux-gnu-gcc
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -fPIC -ffast-math -march=armv8-a+crypto
LDFLAGS ?= -shared -lm -Wl,--no-undefined
EXACT_CFLAGS ?= $(filter-out -ffast-math,$(CFLAGS))

VLLDP_PLUGIN = sp11_vlldp_v19.so
EXACT_PLUGIN = sp11_vlldp_exact.so
LEGACY_PLUGIN = sp11_native.so
REPO_ROOT ?= ../../../..
CHAIN_MANIFEST ?= $(REPO_ROOT)/outputs/windows_audio_chain_manifest_20260616.json
GUARDRAIL_CHECK ?= $(REPO_ROOT)/tools/VlldpModel/check_sp11_runtime_guardrails.py

.PHONY: all clean install legacy native legacy-native exact-native exact-plugin-check exact-plugin-smoke contract-check native-bridge-check full-chain-seed-check guardrail-check check

all: $(VLLDP_PLUGIN)

$(VLLDP_PLUGIN): sp11_vlldp_v19.c sp11_vlldp_init_pack.h sp11_vlldp_v8_runtime_contract.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(LEGACY_PLUGIN): sp11_native.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(EXACT_PLUGIN): sp11_vlldp_exact.c sp11_vlldp_pe_loader.h sp11_vlldp_init_pack.h sp11_vlldp_v8_runtime_contract.h sp11_vlldp_fun18001de90.h
	$(CC) $(EXACT_CFLAGS) -o $@ $< $(LDFLAGS)

legacy: $(LEGACY_PLUGIN)

install: guardrail-check $(VLLDP_PLUGIN)
	install -d $(DESTDIR)/usr/lib/ladspa/
	install -m 644 $(VLLDP_PLUGIN) $(DESTDIR)/usr/lib/ladspa/
	install -d $(DESTDIR)/etc/pipewire/filter-chain/
	install -m 644 sp11-vlldp-live.conf $(DESTDIR)/etc/pipewire/filter-chain/

clean:
	rm -f $(VLLDP_PLUGIN) $(LEGACY_PLUGIN) sp11_vlldp_v8_contract_check \
		$(EXACT_PLUGIN) pe_test init_test leveler_test analyzer_test synth_test full_chain_test \
		sp11_vlldp_exact_ladspa_smoke

# Native build for local testing (not ARM)
native: CC = gcc
native: CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -fPIC -ffast-math -march=native
native: $(VLLDP_PLUGIN)

legacy-native: CC = gcc
legacy-native: CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -fPIC -ffast-math -march=native
legacy-native: $(LEGACY_PLUGIN)

exact-native: CC = gcc
exact-native: CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -fPIC -ffast-math -march=native
exact-native: $(EXACT_PLUGIN)

exact-plugin-check: exact-native
	test -s $(EXACT_PLUGIN)

exact-plugin-smoke: CC = gcc
exact-plugin-smoke: CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter
exact-plugin-smoke: WAV_PATH ?= $(REPO_ROOT)/outputs/vlldp_state_runs/20260615_223202_dolby_dynamic_clean_tone_v8_analyzer/test_tone_997hz_5s.wav
exact-plugin-smoke: exact-plugin-check sp11_vlldp_exact_ladspa_smoke.c
	$(CC) $(CFLAGS) -o sp11_vlldp_exact_ladspa_smoke sp11_vlldp_exact_ladspa_smoke.c -ldl -lm
	./sp11_vlldp_exact_ladspa_smoke ./$(EXACT_PLUGIN) "$(WAV_PATH)"

contract-check: CC = gcc
contract-check: CFLAGS = -std=c11 -Wall -Wextra -Werror
contract-check: sp11_vlldp_v8_contract_check.c sp11_vlldp_v8_runtime_contract.h sp11_vlldp_fun18001de90.h sp11_vlldp_fun18001de90_cdb_vectors.h
	$(CC) $(CFLAGS) -o sp11_vlldp_v8_contract_check sp11_vlldp_v8_contract_check.c
	./sp11_vlldp_v8_contract_check

native-bridge-check: CC = gcc
native-bridge-check: CFLAGS = -O2 -Wall -Wextra
native-bridge-check: DLL_PATH ?= $(abspath ../../SpeakerDLLs/DolbyAPOvlldp150.dll)
native-bridge-check: sp11_vlldp_pe_loader.h sp11_pe_loader_test.c sp11_init_state_test.c sp11_leveler_core_test.c sp11_analyzer_core_test.c sp11_synth_core_test.c leveler_inputs.bin analyzer_inputs.bin synth_inputs.bin
	$(CC) $(CFLAGS) -o pe_test sp11_pe_loader_test.c
	./pe_test "$(DLL_PATH)"
	$(CC) $(CFLAGS) -o init_test sp11_init_state_test.c
	./init_test "$(DLL_PATH)"
	$(CC) $(CFLAGS) -o leveler_test sp11_leveler_core_test.c -lm
	./leveler_test "$(DLL_PATH)" leveler_inputs.bin
	$(CC) $(CFLAGS) -o analyzer_test sp11_analyzer_core_test.c -lm
	./analyzer_test "$(DLL_PATH)" analyzer_inputs.bin
	$(CC) $(CFLAGS) -o synth_test sp11_synth_core_test.c -lm
	./synth_test "$(DLL_PATH)" synth_inputs.bin

full-chain-seed-check: CC = gcc
full-chain-seed-check: CFLAGS = -O2 -Wall -Wextra
full-chain-seed-check: DLL_PATH ?= $(abspath ../../SpeakerDLLs/DolbyAPOvlldp150.dll)
full-chain-seed-check: WAV_PATH ?= $(REPO_ROOT)/outputs/vlldp_state_runs/20260615_223202_dolby_dynamic_clean_tone_v8_analyzer/test_tone_997hz_5s.wav
full-chain-seed-check: sp11_full_chain_core_test.c sp11_vlldp_pe_loader.h sp11_vlldp_fun18001de90.h sp11_vlldp_v8_runtime_contract.h
	cd $(REPO_ROOT) && python3 SOURCE/Dolby/Prototype/sp11-processor/dump_full_chain_seed.py
	$(CC) $(CFLAGS) -o full_chain_test sp11_full_chain_core_test.c -lm
	./full_chain_test "$(DLL_PATH)" "$(WAV_PATH)" 0 full_chain_seed.bin

guardrail-check:
	python3 $(GUARDRAIL_CHECK) --manifest $(CHAIN_MANIFEST) --prototype sp11_vlldp_v19.c --handoff HANDOFF.md --fun18001de90-helper sp11_vlldp_fun18001de90.h

check: contract-check native-bridge-check full-chain-seed-check exact-plugin-smoke guardrail-check
