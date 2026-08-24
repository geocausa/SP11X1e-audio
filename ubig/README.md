# UbiG

UbiG is the native userspace speaker-DSP replacement project for the Surface Pro 11 audio path.

The project goal is to replace the two proprietary Windows userspace DSP binaries used by the Golden Linux reference path with independently maintained native code, while preserving the already-working Qualcomm AudioReach/ADSP firmware path and Golden v33 kernel/hardware integration.

## Rules

- UbiG code never loads or links a proprietary Windows binary.
- Proprietary binaries remain oracle-only and outside this tree.
- Native behavior is implemented from documented behavioral/state contracts and differential tests.
- Golden v33 is the production kernel/hardware baseline; Golden v32 remains rollback.
- 48 kHz stereo internal-speaker playback is the first supported target.
- Profile changes must be in-place and must not destroy long-memory adaptive state unless the reference lifecycle does so.
- The realtime process path performs no file I/O, IPC, heap allocation, logging, or blocking synchronization.
- All seven public profiles use the exact native SP11 Stage-A audio path; Movie/Music retain their distinct recovered staged family state, proven bit-transparent at the Stage-A boundary.

## Components

- `libubig-core` — deterministic native DSP engine and state.
- `UbiG control page` — versioned realtime-safe request/ack bridge.
- `ubigctl` — initial CLI controller and ABI exerciser.
- `UbiG Equalizer` — packaged GTK4 profile/20-band GEQ controller with
  per-user state persistence.
- `ubigd` — optional future D-Bus policy layer; it is not required by the
  finished realtime engine or the installed direct control application.
- `ubig-lab` — optional non-production LADSPA adapter for differential testing.

See `docs/ROADMAP.md` and `docs/ARCHITECTURE.md`.

## Current native playback boundary

`ubig_engine_process()` executes the native SP11 Stage-A path for all seven public profiles behind the decoded 256-frame accumulator. The tracked SP11 LADSPA candidate combines that engine with the source-owned Stage-B realtime path, consumes only an owner-supplied external corrected-v4 state/tuning pack, and fails closed when the pack is absent. It has no PE-loader boundary and does not open either proprietary Dolby userspace binary.

The SP11 UbiG engine is now the production Linux userspace identity on Golden v33. Profile/Custom switching, endpoint postgain, mute/unmute, idle/wake, deterministic seek, repeated lifecycle, >8-hour stability, physical protection telemetry, and fresh matched Windows-vs-UbiG acoustic gates are closed. The visible PipeWire sink is `effect_input.sp11_ubig`; no active Linux sink/service is branded with the Windows vendor name. See `docs/STATUS.md` for the evidence and exact closure boundary.

## Engine versus promotion status

The shipped SP11 userspace engine is complete for its declared 48 kHz stereo
contract: both source-owned DSP stages, seven profiles, live Custom/GEQ and
endpoint postgain are connected in the running candidate without either Dolby
DLL. The long soak and fresh physical Windows comparison are complete. Golden v32 remains an explicit kernel rollback, while the active userspace graph is UbiG.

The endpoint-specific corrected-v4 owner data pack is still required at
construction time. It contains no executable code and is not part of the GEQ
package; replacing it with a publicly generated/minimized tuning pack remains
a distribution/reproducibility task rather than missing realtime DSP logic.

The small `ubig-control` Debian package provides the application-menu entry
**UbiG Equalizer**, 20 recovered SP11 bands on a friendly `-12..+12 dB` scale,
profile selection, request/ack status and saved per-user state. See
`docs/CONTROL-APP.md`.
