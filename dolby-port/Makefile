CC ?= aarch64-linux-gnu-gcc
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -fPIC -ffast-math -march=armv8-a+crypto
LDFLAGS ?= -shared -lm -Wl,--no-undefined
EXACT_CFLAGS ?= $(filter-out -ffast-math,$(CFLAGS))

VLLDP_PLUGIN = sp11_vlldp_v19.so
EXACT_PLUGIN = sp11_vlldp_exact.so
LEGACY_PLUGIN = sp11_native.so
REPO_ROOT ?= ../../../..
SP11_DOLBY_BUNDLE ?= $(HOME)/.local/lib/sp11-dolby
SP11_DOLBY_FIXTURES ?= $(HOME)/.local/share/sp11-dolby/fixtures
DLL_PATH ?= $(SP11_DOLBY_BUNDLE)/DolbyAPOvlldp150.dll
LEVELER_FIXTURE ?= $(SP11_DOLBY_FIXTURES)/leveler_inputs.bin
ANALYZER_FIXTURE ?= $(SP11_DOLBY_FIXTURES)/analyzer_inputs.bin
SYNTH_FIXTURE ?= $(SP11_DOLBY_FIXTURES)/synth_inputs.bin
FULL_CHAIN_SEED ?= $(SP11_DOLBY_FIXTURES)/full_chain_seed.bin
WAV_PATH ?= $(SP11_DOLBY_FIXTURES)/test_tone_997hz_5s.wav
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
native-bridge-check: sp11_vlldp_pe_loader.h sp11_pe_loader_test.c sp11_init_state_test.c sp11_leveler_core_test.c sp11_analyzer_core_test.c sp11_synth_core_test.c
	python3 ../tools/dolby/verify_private_fixtures.py --required-only "$(SP11_DOLBY_FIXTURES)"
	test -f "$(DLL_PATH)" || { echo "missing private DLL: $(DLL_PATH)" >&2; exit 2; }
	$(CC) $(CFLAGS) -o pe_test sp11_pe_loader_test.c
	./pe_test "$(DLL_PATH)"
	$(CC) $(CFLAGS) -o init_test sp11_init_state_test.c
	./init_test "$(DLL_PATH)"
	$(CC) $(CFLAGS) -o leveler_test sp11_leveler_core_test.c -lm
	./leveler_test "$(DLL_PATH)" "$(LEVELER_FIXTURE)"
	$(CC) $(CFLAGS) -o analyzer_test sp11_analyzer_core_test.c -lm
	./analyzer_test "$(DLL_PATH)" "$(ANALYZER_FIXTURE)"
	$(CC) $(CFLAGS) -o synth_test sp11_synth_core_test.c -lm
	./synth_test "$(DLL_PATH)" "$(SYNTH_FIXTURE)"

full-chain-seed-check: CC = gcc
full-chain-seed-check: CFLAGS = -O2 -Wall -Wextra
full-chain-seed-check: sp11_full_chain_core_test.c sp11_vlldp_pe_loader.h sp11_vlldp_fun18001de90.h sp11_vlldp_v8_runtime_contract.h
	python3 ../tools/dolby/verify_private_fixtures.py --required-only "$(SP11_DOLBY_FIXTURES)"
	test -f "$(DLL_PATH)" || { echo "missing private DLL: $(DLL_PATH)" >&2; exit 2; }
	test -f "$(WAV_PATH)" || { echo "missing private/reference WAV: $(WAV_PATH)" >&2; exit 2; }
	$(CC) $(CFLAGS) -o full_chain_test sp11_full_chain_core_test.c -lm
	./full_chain_test "$(DLL_PATH)" "$(WAV_PATH)" 0 "$(FULL_CHAIN_SEED)"

guardrail-check:
	python3 $(GUARDRAIL_CHECK) --manifest $(CHAIN_MANIFEST) --prototype sp11_vlldp_v19.c --handoff HANDOFF.md --fun18001de90-helper sp11_vlldp_fun18001de90.h

check: contract-check native-bridge-check full-chain-seed-check exact-plugin-smoke guardrail-check

.PHONY: windows-chain-production windows-chain-profile-lifecycle-check windows-chain-postgain-control-check
windows-chain-production:
	../deploy/dolby/build-production.sh $(CURDIR)/sp11_dolby_windows_chain.production.so

windows-chain-profile-lifecycle-check: CC = gcc
windows-chain-profile-lifecycle-check: CFLAGS = -O2 -Wall -Wextra
windows-chain-profile-lifecycle-check: sp11_dolby_windows_chain_profile_lifecycle_test.c sp11_dolby_windows_chain_ladspa.c
	$(CC) $(CFLAGS) -o sp11_dolby_windows_chain_profile_lifecycle_test sp11_dolby_windows_chain_profile_lifecycle_test.c -ldl -lm
	./sp11_dolby_windows_chain_profile_lifecycle_test

windows-chain-postgain-control-check: CC = gcc
windows-chain-postgain-control-check: CFLAGS = -O2 -Wall -Wextra
windows-chain-postgain-control-check: sp11_dolby_windows_chain_postgain_control_test.c sp11_dolby_windows_chain_ladspa.c
	$(CC) $(CFLAGS) -o sp11_dolby_windows_chain_postgain_control_test sp11_dolby_windows_chain_postgain_control_test.c -ldl -lm
	./sp11_dolby_windows_chain_postgain_control_test
