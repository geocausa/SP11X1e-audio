# UbiG initial RE/evidence inventory

Created from the protected Golden v32 repository (`main` at `8118a64`) and the dedicated late userspace-RE worktree (`agent/dolby-completion-2026-08-05` at `e2c4e4b`).

No private binary has been copied into UbiG.

## High-value native-replacement evidence

Legacy RE worktree findings:

- `2026-08-05-LIVE-VLLDP-CORE-RECOVERY.md` — live object identity, state regions, profile discrimination, original setter behavior and captured-state replay.
- `2026-08-05-VR-LEVELER-LONG-MEMORY-STATE.md` — long-memory adaptive controller ownership and update behavior.
- `2026-08-06-VLLDP-MB-COMPRESSOR-INPUT-AND-WARM-STATE-CLOSURE.md` — exact compressor input provenance, tuning parser and nested state closure.
- `2026-08-06-VLLDP-FINAL-LIMITER-SAMPLE-DOMAIN-LOCALIZATION.md` — stage localization and limiter nonlinear behavior.
- `2026-08-05-DOLBY-NATIVE-PROFILES.md` — seven recovered profile families, Custom 20-band path and validation.
- `2026-08-05-WINDOWS-DYNAMIC-MUSIC-INPLACE-LIFECYCLE.md` — reference in-place retune lifecycle.

Legacy executable/harness sources useful as *oracles/spec extractors*, not code to rename and ship:

- `sp11_vlldp_scheduler_ladspa.c`
- `sp11_vlldp_orchestrator_ladspa.c`
- `sp11_vlldp_state_oracle.c`
- `sp11_dapvr_native_measure.c`
- `sp11_vr_outer_probe.c`
- `sp11_dolby_windows_chain_profile_lifecycle_test.c`

Golden/main evidence:

- `2026-08-17-W02-FRESH-WINDOWS-DUMP-BITEXACT-DSP-BOUNDARY.md` — direct stage-boundary identity evidence.
- `2026-08-12-WINDOWS-LINUX-RENDER-PARITY.md` and current Golden v32 checkpoint — end-to-end reference quality.

## Frozen private oracle binaries

The current private reference bridge uses two ARM64 Windows DSP binaries with hashes already recorded by the parent project. UbiG does not copy them. They remain available only to private differential harnesses until both native stages are complete.

## First implementation facts transcribed into UbiG

- target format: 48 kHz stereo float32
- persistent 256-frame accumulator semantics, including one-block startup/history latency
- seven public profile families mapped to UbiG names
- in-place retune requirement with adaptive-history preservation
- Custom 20-band target vector and recovered raw range
- stereo endpoint effective-mode policy separated from raw profile tuning

## Next inventory work

1. Hash all public spec-source documents used by UbiG.
2. Produce a machine-readable evidence ledger linking each UbiG constant/state rule to its source class.
3. Extract the complete 432-domain scheduling contract into a brand-neutral UbiG specification.
4. Build a private oracle runner that emits only generated inputs, outputs, state fingerprints and metadata into UbiG-compatible result files.
