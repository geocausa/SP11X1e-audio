# UbiG status — 2026-08-21

Branch: `ubig/deblob-main`
Workspace: `/home/geoca/Documents/SP11-PROJECT/03-UbiG`
Protected integration baseline: Golden v32 (`main` at creation: `8118a64`)

## Completed today

- Dedicated UbiG Git worktree created without modifying Golden v32.
- Native project naming, architecture, provenance rules and roadmap established.
- Stable 48 kHz / stereo engine ABI created.
- Versioned realtime control-page ABI created for future `ubigd`, CLI and GUI.
- Seven UbiG profile identities established: Dynamic, Movie, Music, Game, Voice, Course, Custom.
- Custom 20-band control transport established with recovered raw range validation.
- Native persistent 256-frame accumulator implemented and proven chunk-invariant.
- Deterministic generated oracle corpus tooling created.
- Generic float32 differential comparison tooling created.
- Private whole-chain and Stage-A-only reference runners established outside Git.

## First proven native algorithm block

UbiG Stage-A final limiter is implemented natively in:

`src/core/stage_a_limiter.c`

Private direct-function differential test result against the original ARM64 reference function:

> **80 consecutive 256-frame blocks: bit-exact stereo output and bit-exact modeled state.**

Compared state includes:

- both 64-frame lookahead delay lines
- 16-slot peak history
- 16-slot predictor history
- primary and secondary envelopes
- current / previous / target gain
- delay and history indices

The public regression suite retains an oracle-proven deterministic hash so this exactness can be protected without requiring private binaries.

## Additional native Stage-A closures

Since the first limiter checkpoint, UbiG has also closed these direct boundaries:

- `0x1800247c0` fast log2 helper: 200k+ direct inputs bit-exact.
- `0x180023d20` scaled exp2 helper: 1,000,000 direct lanes bit-exact.
- `0x18001de90` 20-band persistent export/smoother: 400,000 band transitions bit-exact, using exact constructor coefficient bits rather than rounded historical values.
- `0x1800240e0` synthesis wrapper: bit-exact with an injected transform for both phases; output and persistent overlap state match.
- `0x180023db0` analyzer wrapper: bit-exact with an injected transform for two consecutive blocks; output, phase, history and all modeled spectral state match.

The 320-point transform itself has been independently specified as a standard complex forward DFT. UbiG now has a clean generated-mathematics 5x64 implementation. It is **not yet called bit-exact**: the current arithmetic order measures roughly 123.8 dB SNR against the generated ARM64 kernels. Exact transform arithmetic-order parity remains open and isolated behind the filterbank callback interface.

This means the proprietary analysis/synthesis wrappers have been eliminated as algorithmic unknowns; only the transform arithmetic-order cleanup and the central adaptive/multiband blocks remain in this Stage-A region.

## Stage-A structural boundary

Stage-A-only impulse oracle:

- reference first nonzero: frame **320**
- UbiG partial Stage A first nonzero: frame **320**

This decomposes as:

- 256-frame persistent accumulator latency
- 64-frame final-limiter lookahead

Host chunking is bit-transparent in both reference and UbiG for the tested 480-frame and chaotic schedules.

The remaining Stage-A waveform mismatch begins at the first real output frame, so it is now localized to unreplaced upstream Stage-A processing rather than scheduling/latency or final limiter behavior.

## Next technical target

The next major target is `0x180021e80`, the Stage-A multiband compressor/regulator block. Its public inputs and tuning/state ownership are already mapped from the late RE branch. Continue with the same rule used for limiter/filterbank work: isolate callable state and I/O, inject or freeze adjacent helpers, then accept native code only after a direct differential gate.

Do not install UbiG into the live PipeWire path yet.
