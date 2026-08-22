# UbiG

UbiG is the native userspace speaker-DSP replacement project for the Surface Pro 11 audio path.

The project goal is to replace the two proprietary Windows userspace DSP binaries used by the Golden Linux reference path with independently maintained native code, while preserving the already-working Qualcomm AudioReach/ADSP firmware path and Golden v32 kernel/hardware integration.

## Rules

- UbiG code never loads or links a proprietary Windows binary.
- Proprietary binaries remain oracle-only and outside this tree.
- Native behavior is implemented from documented behavioral/state contracts and differential tests.
- Golden v32 is never modified by UbiG experiments.
- 48 kHz stereo internal-speaker playback is the first supported target.
- Profile changes must be in-place and must not destroy long-memory adaptive state unless the reference lifecycle does so.
- The realtime process path performs no file I/O, IPC, heap allocation, logging, or blocking synchronization.
- All seven public profiles use the exact native SP11 Stage-A audio path; Movie/Music retain their distinct recovered staged family state, proven bit-transparent at the Stage-A boundary.

## Components

- `libubig-core` — deterministic native DSP engine and state.
- `UbiG control page` — versioned realtime-safe request/ack bridge.
- `ubigctl` — initial CLI controller and ABI exerciser.
- `ubigd` — planned user-session policy/persistence service.
- `UbiG Control` — planned GUI using the same control API.
- `ubig-lab` — optional non-production LADSPA adapter for differential testing.

See `docs/ROADMAP.md` and `docs/ARCHITECTURE.md`.

## Current native playback boundary

`ubig_engine_process()` executes the native SP11 Stage-A path for all seven public profiles behind the decoded 256-frame accumulator. The tracked SP11 LADSPA candidate combines that engine with the source-owned Stage-B realtime path, consumes only an owner-supplied external corrected-v3 state/tuning pack, and fails closed when the pack is absent. It has no PE-loader boundary and does not open either proprietary Dolby userspace binary.

The disposable candidate is currently passing the first live M6 integration gates on SP11: profile and non-flat Custom switching, endpoint postgain, mute/unmute, idle/wake, repeated stream lifecycle, and bounded WSA status/fault observation all preserve the Golden v32 kernel and hardware contract. It is not yet promoted as the permanent default. Remaining promotion gates are audible physical acoustic comparison against Windows, seek behavior, longer physical-output protection telemetry, and an over-eight-hour stability run. See `docs/STATUS.md` and `docs/ROADMAP.md` for the evidence and exact qualification boundary.
