# SP11 Windows render-start owner FINAL4 checkpoint

Date: 2026-08-19 (Europe/London)
Branch: `agent/psycho-bass-20260818`
Status: decisive Windows-oracle render-start localization; Golden v31 remains protected

## Purpose

This checkpoint records the first fully valid synchronized capture in which:

1. qcaucd/qcaudminiport were already loaded and their current relocation bases were known;
2. KD tracepoints were bound to those exact live addresses;
3. no qcadcm/PnP restart occurred after breakpoint binding;
4. a fresh LPASS DATA_LOGGING collector remained active through the render;
5. native Windows tap3 produced real 24 kHz CPS PCM and tap2 produced real 8 kHz VI PCM in the same run;
6. the exact qcaudminiport/qcaucd render-start lifecycle was recorded before those samples appeared.

This removes the breakpoint-rebase ambiguity that invalidated earlier `final3` kernel-negative interpretations.

## Safety baseline

Golden Linux remains untouched.

- GRUB id: `sp11-audio-golden-v31`
- topology SHA-256: `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`
- no latest Windows finding has yet been promoted to a Linux candidate

Previous checkpoint: `54a7a93c2a849be688a0b6f5dfe045d220c96025` (`audio: checkpoint op4 retained state and render-start dataplane`).

## FINAL4 evidence locations

Windows LPASS evidence:

`C:\Users\Geoca\Documents\SP11-Audio-Audit-20260812\windows-protection-deconstruct-20260819\render-start-kd-tap-sync-final4-20260819-235112`

Isolated KD log:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11-Feedback-Boundary-20260819\render-start-sync-final4_2aac_2026-08-19_23-51-06-850.log`

Static decompile generated after the capture:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11-Feedback-Boundary-20260819\qcaucd-render-start-chain-final4-decompile-20260819.txt`

## Live driver bases and exact tracepoints

Immediately before FINAL4 render:

- `qcaucd8380`: `fffff802`bda30000`
- `qcaudminiport8380`: `fffff802`bdad0000`

Absolute KD tracepoints:

- qcaudminiport `+0x6d8d0` -> `fffff802`bdb3d8d0` (`MINI_HWIF`)
- qcaucd `+0x36510` -> `fffff802`bda66510` (`WSA_START_OWNER`)
- qcaucd `+0x31d28` -> `fffff802`bda61d28` (`SWR_START_A`)
- qcaucd `+0x31d54` -> `fffff802`bda61d54` (`SWR_START_B`)
- qcaucd `+0x32e10` -> `fffff802`bda62e10` (`WSA_LIFECYCLE`)

No device restart or module reload occurred between this binding and the render.

## FINAL4 Windows render markers

- WASAPI invoke begin:
  - UTC `2026-08-19T22:51:50.1205207Z`
  - KUSER shared interrupt time `0x0000000d69440391`
- WASAPI READY observed:
  - observer UTC `2026-08-19T22:51:50.9919528Z`
  - KUSER interrupt time `0x0000000d6992a385`
  - renderer ready-file UTC `2026-08-19T22:51:50.9660898Z`
  - format: 48 kHz float32 shared-mode render
- WASAPI end:
  - UTC `2026-08-19T22:52:03.2993730Z`
  - KUSER interrupt time `0x0000000d70d4a94a`

Renderer uses the known delayed 997 Hz marker, so initial render PCM is deliberate digital silence.

## FINAL4 LPASS data plane

Collector mask sent: `2026-08-19T22:51:20.9018076Z`.

32 DATA_LOGGING cmd16 packets were captured.

### tap1 render

- frames: 19
- rate: 48 kHz
- payload: 192 bytes
- first packet: `2026-08-19T22:51:51.0105078Z`, zero payload
- first nonzero render PCM: `2026-08-19T22:51:54.8512639Z`

### tap3 CPS

- frames: 7
- rate: 24 kHz
- payload: 1920 bytes
- all seven frames nonzero
- first real CPS frame: `2026-08-19T22:51:51.3098921Z`
- first frame nonzero bytes: 1440

### tap2 VI

- frames: 6
- rate: 8 kHz
- payload: 640 bytes
- all six frames nonzero
- first real VI frame: `2026-08-19T22:51:51.8365074Z`
- first frame nonzero bytes: 320

Therefore FINAL4 definitively reproduces the Windows oracle: CPS/VI carry real samples before the delayed 997 Hz render PCM becomes nonzero.

## FINAL4 exact kernel lifecycle

KD recorded:

```text
MINI_HWIF itime=69743471 op=4 x0=ffffbd0e06dbc4f0 x2=fffffa85209a89b0 x3=0000000000000040
WSA_START_OWNER itime=6975e6ac x0=ffffbd0e051dde80 x1=0000000000000002 x2=ffffbd0e05be9bd0 x3=fffff802bda436a8
SWR_START_A itime=69776142 x0=0000000000000000 x1=0000000000000000 x2=0000000000000010 x3=fffff802bda43988
SWR_START_A itime=69776aea x0=0000000000000000 x1=0000000000000000 x2=0000000000000010 x3=fffff802bda43988
SWR_START_B itime=6980b371 x0=0000000000000000 x1=0000000000000000 x2=0000000000000001 x3=0000000000000001
WSA_LIFECYCLE itime=69827476 mode=1 x1=fffffa85209a7d50 x2=0000000000000004 x3=fffff802bda43978
WSA_LIFECYCLE itime=69840aef mode=1 x1=fffffa85209a7d50 x2=0000000000000010 x3=fffff802bda43978
MINI_HWIF itime=70c836d0 op=5 x0=ffffbd0e06dbc4f0 x2=fffffa8524d78e90 x3=0000000000000040
WSA_LIFECYCLE itime=70c85523 mode=2 x1=fffffa8524d784c0 x2=0000000000000001 x3=0000000000000001
WSA_LIFECYCLE itime=70c99dba mode=2 x1=fffffa8524d784c0 x2=0000000000000001 x3=0000000000000001
```

The printed `itime` values are the low 32 bits of KUSER_SHARED_DATA InterruptTime. Relative timing among the tracepoints is valid. Approximate active-time deltas:

- op4 -> WSA_START_OWNER: ~11.1 ms
- WSA_START_OWNER -> first SWR_START_A: ~9.7 ms
- first SWR_START_A -> second SWR_START_A: ~0.25 ms
- second SWR_START_A -> SWR_START_B: ~60.8 ms
- SWR_START_B -> first WSA_LIFECYCLE mode=1: ~11.5 ms
- first -> second lifecycle mode=1: ~10.4 ms
- op4 -> second lifecycle mode=1: ~103.8 ms

All create/start/lifecycle events occur before WASAPI READY. After that, tap3 and tap2 become real PCM while render tap1 remains digital silence.

## Decisive interpretation

The missing Linux sample-feed activation is now localized to the Windows render-start transaction:

`qcaudminiport op4 hardware-device create/configure`
` -> qcaucd WSA_START_OWNER +0x36510`
` -> qcaucd SoundWire resource start (+0x31d28/+0x31d54)`
` -> WSA lifecycle mode=1 (+0x32e10)`
` -> CPS tap3 real 24 kHz PCM`
` -> VI tap2 real 8 kHz PCM`

This is not an audiodg/APO effect, not op8/event0x4a ADC state, not a later response to acoustic energy, and not merely qcadcm graph birth.

The strongest remaining suspect is a resource/client action inside `FUN_140036510` or the immediate `FUN_140031bd0` resource-start children that Windows performs during op4 before/while SoundWire starts.

## New static breakthrough in `FUN_140036510`

Ghidra exact decompile shows qcaucd `+0x36510` is `FUN_140036510`.

Important behavior:

- retrieves event/config type `0x22` through `FUN_140028a80(0x22, ...)`
- maps state with `FUN_140028b48(&DAT_140013480, 0xd, ...)`
- obtains four class/bus-2 resource objects:
  - `FUN_140022e80(2,5)`
  - `FUN_140022e80(2,6)`
  - `FUN_140022e80(2,7)`
  - `FUN_140022e80(2,8)`
- configures controller fields at WSA-relative offsets including:
  - `0x2408`, `0x2400`
  - `0x2488`, `0x2480`
  - `0x4408`, `0x4400`
  - `0x4488`, `0x4480`
- invokes `FUN_140031b48(...)`
- then invokes `FUN_140031bd0(...)` on each of the four resource objects

The four `FUN_140022e80(2,5..8)` resources are the highest-priority objects to identify next. They may distinguish ordinary speaker playback resources from the VI/CPS feedback client resources.

`+0x31d28` and `+0x31d54` are both inside `FUN_140031bd0`.

`FUN_140031bd0` calls known WSA reset/clock setup `FUN_14003b9b0`, uses resource-specific tables around `0x1400588e0/890/840` and `0x1400585f0/5c0/590`, performs `FUN_14003ec58` applications, may call `FUN_14003f5c8`, and ends through `FUN_14003b678` zero-user/clock-stop handling.

Generic WSA lifecycle at `FUN_140032e10` is already substantially reconstructed and should not be transplanted wholesale absent a newly isolated missing operation.

## Invalid intermediate runs that must not be used as negative evidence

- `final3`: LPASS data was valid, but KD breakpoints referenced `<Unloaded_qcaucd8380.sys>` / `<Unloaded_qcaudminiport8380.sys>` after qcadcm rebased the modules. Kernel silence was invalid.
- earlier final/final2 attempts had collector teardown/expiry ordering issues.

FINAL4 is the first fully valid synchronized kernel + LPASS capture.

## Next action

1. Decompile and identify `FUN_140022e80` and the resource table entries for class/bus 2 IDs 5, 6, 7, 8.
2. Decompile `FUN_140031b48` and full `FUN_140031bd0` with those resource objects distinguished.
3. Instrument only the resource-specific branches/side effects inside `FUN_140036510` / `FUN_140031bd0` if static identification is insufficient.
4. Compare the resulting operation against Golden Linux's qcom SoundWire / LPASS / AudioReach backend start semantics.
5. Port only the first demonstrably missing operation into a disposable v31-derived candidate.

Promotion gate remains unchanged: real nonzero Linux tap2 8 kHz VI PCM and tap3 24 kHz CPS PCM during an acoustically proven render, with no faults. Never overwrite Golden v31.
