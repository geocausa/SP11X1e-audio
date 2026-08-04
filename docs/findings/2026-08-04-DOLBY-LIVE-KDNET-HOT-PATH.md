# Dolby live KDNET hot-path correlation — 2026-08-04

This note records live kernel-debugger results obtained on the Surface Pro 11
while normal stereo music was actively rendering. The debugger host was the
Surface Pro 7 over KDNET. These results correct several earlier conclusions
that were based on software breakpoints or static reachability alone.

## Proven live process layout

`audiodg.exe` EPROCESS during this session:

```text
ffff94826e48b080   Cid 152c
```

Two `DAX3API.exe` processes were live:

```text
ffff948269de7080   Cid 1610   parent
ffff94826a9ec100   Cid 1dc0   child
```

The parent DAX3API process loads `Dax3DapControl.dll`. The child loads both
`Dax3DapControl.dll` and `CaptureStreamMonitor.dll`. Neither helper is loaded
inside `audiodg.exe`. `Dax3Ref.dll` was not loaded in either DAX3API process.

### Live helper exports

`Dax3DapControl.dll` (base `0x00007ffd5a5d0000`):

```text
OpenDapControl       0x00007ffd5a5e15e8
CloseDapControl      0x00007ffd5a5e26f0
SetDapParam          0x00007ffd5a5e3600
SetDapVariantParam   0x00007ffd5a5e38a0
GetDapParam          0x00007ffd5a5e48a0
```

`CaptureStreamMonitor.dll` (base `0x00007ffd4c260000`):

```text
StartMonitor         0x00007ffd4c28e838
```

Both are DAX build `3.30704.742.0` from September 2024.

## Modern DolbyAudioProcessing.dll is explicitly ASAR

Current live modern DLL:

```text
base          0x00007ffd53620000
image size    0x00344000
version       7.3.7.0 / 7.3.7.rel
timestamp     2026-04-17 05:00:11 (0x69E1B04B)
package       DolbyAccessOEM_3.27.11070.0_arm64
InternalName  msft-asar-dap
ProductName   Dolby Atmos for Microsoft Spatial Audio
Description   Dolby Audio Processing for Microsoft Spatial Audio
```

This is an important architectural distinction from the persistent 2024 DAX
speaker APO stack.

The live 7.3.7 image preserves the exact symbol-string RVAs used by the prior
reverse engineering, proving that the earlier modern-DLL offsets are not stale:

```text
base + 0x316b10 -> dolby::dapvr::CDapVRModule::Process
base + 0x324d20 -> dolby::dap::VlldpModule::Process
```

The live string inventory also exposes the OAR subsystem more clearly:

```text
dolby::oar::AIDEModule::Initialize
dolby::oar::AIDEModule::Process
dolby::oar::AIDEModule::SetParams
dolby::oar::CrossfadeModule::Process
dolby::oar::OARModule::Initialize
dolby::oar::OARModule::Process
CDolbyAudioProcessingModule::EnsureAideAndOarModules
DefaultDAPModule::EncodeAudioData
```

Therefore OAR/Crossfade must remain in the candidate set; AIDE must not be
considered the only missing modern stage.

## Master effects gate lifetime split

With music playing, disabling the Windows master Dolby/Device Default Effects
gate removed these modules from `audiodg.exe`:

```text
DolbyAudioProcessing.dll
DolbyHrtfEnc.dll
```

while these remained loaded:

```text
DolbyDax3Apo.dll
DolbyAPOvlldp150.dll
DolbyApoVr.dll
SurfaceAPO.dll
```

Re-enabling the gate reloaded the modern ASAR DLL and HRTF DLL, at new bases.
This reproduces the July cold/active/idle module-lifetime snapshots and proves
that the master effects gate does not tear down the persistent DAX/VLLDP150
speaker stack.

## DAX control boundary proven live

Hardware execution breakpoints were placed on both DAP control setters while
profiles/IEQ/spatial controls were changed.

Observed:

```text
SetDapParam          0 hits
SetDapVariantParam   7 hits
```

All seven `SetDapVariantParam` hits shared this stable outer ABI:

```text
x0 = 0x000002153a1b8d80   stable DAP-control object/handle
x1 = 0x838                stable selector/parameter
x2 = changing heap pointer (per-call payload/variant)
x3 = 1
x4 = 3
caller LR = DAX3API.exe + ... -> 0x00007ff70f9b264c
```

The first capture only logged registers; the `x2` allocations had already been
released by the time a later manual break attempted to read them. A revised
hardware trap now dumps `db @x2 L80` at entry before auto-continuing. Future UI
changes should be labelled one at a time so each payload can be mapped exactly.

## Critical debugger-method correction

Software `/p` breakpoints on user-mode APO code produced false-negative
non-hits in this KD setup. A hardware execution breakpoint at the exact same
DAX3 wrapper address immediately fired continuously.

Therefore:

```text
software-breakpoint non-hit != proof of inactive path
hardware execution non-hit, with active audio pump and verified address = useful evidence
```

Old conclusions based only on the software non-hits in this session must be
discarded.

The live audio pump was independently proven active while testing. The thread
`ffff94826e3c0100` (`AUDIODG!CAudioPump::OutputPumpWorkRoutine`) had reached:

```text
Context Switch Count  214306
UserTime               00:00:48.656
KernelTime             00:00:06.453
```

versus roughly 9,238 context switches and 2.6 seconds user time earlier in the
same session.

## Persistent DAX/VLLDP150 hot path proven

Current driver APO identities:

```text
DolbyDax3Apo.dll       version 3.30704.742.0
                       SHA256 from July capture:
                       6EA1702C0F86766E45C2E248E169022E3D71EAA3C655B3FCA159B4DD59F18D87

DolbyAPOvlldp150.dll   version 3.30704.742.0
                       SHA256:
                       A2553FF7B013B5A248E50BDCAE46D08405E393C0085073975214D035CEDF02C1
```

The tracked Git copy of VLLDP150 hashes exactly to the live-July value above,
so its historical function offsets are byte-locked to the current binary.

### DAX3 wrapper

Hardware breakpoint at runtime `0x00007ffd527bd000`, corresponding to the
previously decoded wrapper `APOProcess` RVA `0x1800cd000`, fired continuously
within one second of active music playback.

**Conclusion: the DAX3 wrapper is unquestionably on the live PCM path.**

### VLLDP150 outer APOProcess

Hardware breakpoint at runtime `0x00007ffd30fa5000`, corresponding to
`FUN_180105000`, fired continuously.

**Conclusion: VLLDP150 itself is unquestionably on the live PCM path.**

### VLLDP150 inner orchestrator

Hardware breakpoint at runtime `0x00007ffd30ebf7a8`, corresponding exactly to
`FUN_18001f7a8`, fired repeatedly; a one-second sample counted about 31 hits.

**Conclusion: the exact top-level VLLDP150 orchestrator that contains the
snapshot/crossfade/state machine is actively executed on the real Windows
speaker stream.** This strongly validates the older native VLLDP150 work and
means the unresolved free-running state/history/orchestration problem remains
a first-class parity target.

## Modern ASAR low-level stages are cold in this tested condition

Using hardware execution traps (not software breakpoints) while the same audio
pump was active:

```text
modern DAPVR DABS speaker wrapper  FUN_18004e7b0   0 hits / ~1 s
modern embedded VLLDP process      FUN_1800922f8   0 hits / ~1 s
AIDE adaptive core                 FUN_18003a438   0 hits / ~1 s
```

The earlier control on `FUN_180061698` is not relevant to the DABS speaker
route: the native DABS harness actually uses `FUN_18004e7b0`, which proceeds
through `FUN_180060ce8`. This correction was made before interpreting the
hardware result above.

These non-hits do **not** prove the whole modern ASAR DLL is idle. They prove
that the exact lower-level blocks used by our current Linux DAPVR -> embedded
VLLDP oracle are not being entered continuously for this active stereo/music
condition. Other ASAR/OAR/crossfade paths may still execute, and activation can
be session/content/spatial-mode dependent.

## Revised architecture model

Best evidence after this live session:

```text
DAX3API service/policy side
  CaptureStreamMonitor (child process)
  Dax3DapControl SetDapVariantParam
            |
            v
Windows audio engine / audiodg
  DolbyDax3Apo wrapper                   HOT
       -> DolbyAPOvlldp150 outer APO     HOT
            -> FUN_18001f7a8             HOT
               snapshot/crossfade/state/history

  DolbyAudioProcessing.dll 7.3.7 (ASAR) loaded in active states
       AIDE / OAR / Crossfade / DAPVR / embedded VLLDP available
       tested DAPVR speaker, embedded VLLDP, AIDE core: COLD for current stereo/music
       exact live participation still to be mapped by high-level wrapper traps
```

This supersedes the simplistic assumption that the current Linux
`DolbyAudioProcessing.dll` DAPVR -> embedded VLLDP oracle directly represents
the per-buffer Windows internal-speaker path.

## Immediate next live tests

1. Capture one labelled `SetDapVariantParam` payload at a time with `db @x2`
   before the call returns (profile change, IEQ change, spatial toggle).
2. Resolve and hardware-trap the high-level modern ASAR wrappers for:
   - `dolby::oar::AIDEModule::Process`
   - `dolby::oar::CrossfadeModule::Process`
   - `dolby::oar::OARModule::Process`
   - `dolby::dapvr::CDapVRModule::Process`
   - `dolby::dap::VlldpModule::Process`
3. Hardware-trap `DolbyAudioProcessing!DllGetActivationFactory` during a single
   spatial/effects activation to capture the exact caller stack.
4. Prioritize executing VLLDP150 `FUN_18001f7a8` with relocated real state on
   Linux rather than manually re-orchestrating its internal analyzer/leveler/
   synth stages.
5. Reclassify the previous AIDE conclusion: AIDE is proven present in the
   modern ASAR code path, but is **not yet proven hot on the tested speaker
   stream**.

## Live DAX3 wrapper objects resolve to VR and VLLDP150

A hardware breakpoint on `DolbyDax3Apo` wrapper `APOProcess` revealed two
nearly perfectly balanced live wrapper instances during the same music stream:

```text
this = 0x00000209396cb260   56 sampled calls
this = 0x00000209396c8860   55 sampled calls
```

The two live wrapper vtable pointers differ:

```text
0x396cb260 -> vtable 0x00007ffd52826018
0x396c8860 -> vtable 0x00007ffd52826188
```

The previously decoded inner-interface field at `this+0xc0` resolves the two
wrappers directly to different Dolby modules:

```text
wrapper 0x396cb260 + 0xc0 -> 0x000002093b010008
  first interface vtable -> 0x00007ffd111d5a18 -> DolbyApoVr.dll

wrapper 0x396c8860 + 0xc0 -> 0x000002093b560008
  first interface vtable -> 0x00007ffd30fa93a0 -> DolbyAPOvlldp150.dll
```

Thus the two equally hot DAX3 wrapper objects are not duplicate mystery
instances. One wraps `DolbyApoVr`; the other wraps `DolbyAPOvlldp150`.

### Exact inner process callbacks from the live vtables

The DAX3 wrapper's inner dispatch uses the inner interface vtable slot at
`+0x18`. Live memory gives:

```text
DolbyApoVr inner vtable 0x111d5a18
  slot +0x18 -> 0x111d1220
  0x111d1220 is a branch thunk -> 0x111d10c8

VLLDP150 inner vtable 0x30fa93a0
  slot +0x18 -> 0x30fa5050
```

Both exact targets were hardware-trap HOT. Representative call linkage for
both was:

```text
LR = 0x00007ffd527bd664   (inside DolbyDax3Apo wrapper)
```

Observed short samples:

```text
DolbyApoVr  0x111d10c8   43 hits in about 1 second
VLLDP150    0x30fa5050   32 hits in about 1 second
```

### Both inner APOs are actively processing, not taking the short bypass

The VR and VLLDP interface callbacks have the same structural gate:

```text
ldrb w8, [this,#0x70]
cbz  w8, deeper_processing_path
otherwise: short copy/pass-through path
```

The byte was captured at hardware-breakpoint entry while the object page was
resident:

```text
DolbyApoVr object 0x000002093b010008 + 0x70 = 0x00
VLLDP150   object 0x000002093b560008 + 0x70 = 0x00
```

Therefore both callbacks take their deeper processing paths under the tested
music condition.

The old note that treated wrapper `this+0x5d0` as a universal bypass flag does
not match these live derived wrapper objects: that offset contains UTF-16-like
object data here. Do not apply the old base-class layout blindly to these two
live wrapper variants.

### Stable per-cycle order: VLLDP then VR

A deliberately short dual hardware-marker run trapped the two exact inner
callbacks simultaneously. 84 captured events alternated without reversal:

```text
VLL -> VR -> VLL -> VR -> ...
```

This is strong live evidence that for the current stream Windows invokes the
VLLDP150-wrapping DAX3 instance before the DolbyApoVr-wrapping DAX3 instance in
each repeating audio cycle.

Updated persistent chain model for the tested stream:

```text
audioeng / audiodg
  -> DAX3 wrapper instance (VLLDP)
       -> DolbyAPOvlldp150 process gate [0x70 = 0]
            -> VLLDP processing
            -> FUN_18001f7a8 hot orchestrator
  -> DAX3 wrapper instance (VR)
       -> DolbyApoVr process gate [0x70 = 0]
            -> VR deeper processing path
```

The exact sample-buffer ownership/order around the two wrappers should still be
confirmed from the audio-engine graph objects before claiming that the output
of one is literally the input pointer of the other, but the callback ordering
itself is directly observed.
