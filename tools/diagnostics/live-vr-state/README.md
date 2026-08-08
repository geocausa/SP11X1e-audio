# Live VR state diagnostics

These small analysis-only harnesses preserve the 2026-08-05 isolation work on
the June-8 Windows `audiodg.exe` Music-state `DolbyApoVr` object. They are not
production code and must not be installed into the PipeWire host.

Key fixed identities used by the experiments:

- Windows `DolbyApoVr.dll` runtime base: `0x00007FFD07A60000`
- captured outer allocation base: `0x0000024539010000`
- outer allocation size: `0x3C0430`
- `LibWrapperVr`: `0x000002453913C2F0`
- VR core: `0x00000245391DD808`
- dependent scalar isolated at absolute VA `0x0000024539201768`
  (`outer+0x1F1768`)

The binary snapshot consumed by the captured-state harnesses is intentionally
not committed. Re-extract it from the June-8 full minidump documented in
`docs/findings/2026-08-05-VR-LIVE-STATE-FOUR-BYTE-ISOLATION.md`.

Files:

- `sp11_vr_fixed_hybrid.c`: constructs fresh Music VR at the exact Windows
  addresses and supports fresh/core/inner/arena/full hybrid comparisons.
- `sp11_vr_chunk_hybrid.c`: copies selected arena ranges from the captured
  object into a fresh object to localize the acoustic delta.
- `sp11_vr_pair_hybrid.c`: tests a captured core plus one additional arena
  range, used to identify the dependent subobject.
- `sp11_vr_watch.c`: fixed-address replay harness used with GDB hardware
  read-watchpoints on the isolated scalar.
- `sp11_vr_fresh_cont_dump.c`: long continuous-phase fresh-Music VR run.
- `sp11_vr_june_cont_dump.c`: long continuous-phase captured Windows-Music VR
  replay.

Do not infer semantics solely from these temporary experiment names. The
semantic owner of the final scalar is still under reverse engineering.
