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

## SetDapVariantParam ABI partially decoded live

A hardware breakpoint on `Dax3DapControl!SetDapVariantParam` was left armed
while the system ran normally. It fired spontaneously before any requested UI
click, proving that the DAX service emits variant updates autonomously as well
as in response to visible Dolby Access controls.

At the live stop:

```text
x0 = 0x000002153a1b8d80
x1 = 0x838
x2 = 0x000002153ab5a728
x3 = 1
lr = 0x00007ff70f9b264c   (DAX3API.exe caller)
```

Fresh entry disassembly corrects the first interpretation of `x0`. The setter
immediately scans `x0` with `ldrsh` as a UTF-16 string; it is **not** a DAP
handle. Live `du @x0` returned:

```text
{0.0.0.00000000}.{5bb689e6-2c6b-4357-b4c1-beb815638f88}
```

That GUID is the known active internal-speaker render endpoint from the July
capture (Qualcomm Aqstic / AUCD REV_0D).

The DAX3API caller is iterating a tree/map record. At the call site:

```text
ldr w1,[x19,#0x40]     ; parameter id
mov w3,w21
mov x2,x0              ; value object
ldr x0,[x20,#8]        ; endpoint-id string
bl  ...SetDapVariantParam
```

For the stopped record:

```text
x19 = 0x000002153ab5a6e0
node+0x40 = 0x838
node+0x48 = value object / x2
```

The value object has the standard COM VARIANT-looking prefix:

```text
03 00 ...              ; VARTYPE 3 = VT_I4
```

and the 32-bit integer payload at the VARIANT union slot is:

```text
0xfffffe2c = -468
```

So the observed call is approximately:

```text
SetDapVariantParam(
    L"{0.0.0.00000000}.{5bb689e6-2c6b-4357-b4c1-beb815638f88}",
    0x838,
    VARIANT(VT_I4, -468),
    1)
```

The semantic meaning of parameter `0x838` is not yet proven. `-468` is
suspiciously compatible with a fixed-point / centi-dB-like volume quantity,
but that must be confirmed by a controlled one-variable test before assigning
a name. Do not label `0x838` as volume yet.

## Dormant/silent-state -> active wake capture (2026-08-04 late session)

A controlled long-pause/resume experiment captured the Dolby endpoint graph entering a dormant state and then being reinstantiated when meaningful playback resumed. Device Default Effects remained enabled throughout; no Dolby Access or Spatial Sound UI setting was changed during the test.

### Dormant state while playback was paused

The active audio-engine host before the long pause was:

```text
audiodg.exe PID 0x1650
EPROCESS ffffae8a`db3ea100
```

After roughly a minute paused, the same `audiodg.exe` process remained alive. Its handle count fell from 623 to 516. `DolbyDax3Apo.dll` remained resident, while `DolbyAudioProcessing.dll` and `DolbyHrtfEnc.dll` were absent from the process module list. A hardware execution breakpoint on the known-hot DAX3 wrapper `APOProcess` (`DolbyDax3Apo + 0xcd000`) produced no hit while paused.

The same PID showed a different VLLDP150 mapping from the earlier active snapshot, consistent with inner-module teardown/remap while the audio-engine host itself survived. Do not treat module residency alone as proof of active PCM processing.

### Resume caused a fresh audiodg and full Dolby stack reinstantiation

On playback resume, the previous `audiodg.exe` was replaced by a fresh process:

```text
new audiodg.exe PID 0x0d30
EPROCESS ffffae8a`dbc95100
```

The new process immediately contained the full active stack:

```text
DolbyAudioProcessing.dll  0x00007ffa`0d1c0000
DolbyApoVr.dll            0x00007ffa`106c0000
DolbyAPOvlldp150.dll      0x00007ffa`244b0000
DolbyDax3Apo.dll          0x00007ffa`24680000
DolbyHrtfEnc.dll          0x00007ffa`3dba0000
SurfaceAPO.dll            0x00007ffa`10410000
audioeng.dll              0x00007ffa`109e0000
```

This directly supports the observed Windows behavior: prolonged silence can leave an audio-engine host alive in a dormant state, while resume can replace/rebuild the active `audiodg` graph and reload the modern adaptive Dolby components.

### First active DAX3 wake callback carries VALID PCM

After rebinding the hardware breakpoint to the fresh process, `DolbyDax3Apo + 0xcd000` fired immediately.

Representative entry state:

```text
x0 = 0x0000020343192840   wrapper object
x1 = 1                    input connection count
x2 = 0x000000d43457ef58   input connection-property array
x3 = 1                    output connection count
x4 = 0x000000d43457ef50   output connection-property array
```

The first input connection property contained:

```text
buffer = 0x0000020344b16140
frames = 480 (0x1e0)
flag   = 1   (VALID)
sig    = 0x41435053
```

The corresponding output property at callback entry contained:

```text
buffer = 0x0000020344b18180
capacity / frame field = 480
flag   = 0   (INVALID at entry)
sig    = 0x41435053
```

The input buffer contained nonzero float PCM immediately. Thus the first trapped post-wake callback was already a real VALID audio block, not a SILENT placeholder.

### Fresh DAX3 wrappers again resolve to VR and VLLDP150

Two consecutive DAX3 wrapper invocations resolved through `this+0xc0` to the two inner APOs:

```text
wrapper 0x0000020343192840
  +0xc0 -> 0x0000020345010008
  vtable -> 0x00007ffa10895a18 (DolbyApoVr.dll)

wrapper 0x000002034319ba10
  +0xc0 -> 0x0000020345560008
  vtable -> 0x00007ffa245b93a0 (DolbyAPOvlldp150.dll)
  vtable slot +0x18 -> 0x00007ffa245b5050
```

Because the debugger was rebound mid-render cycle, this pair of hits must not be used to overturn the earlier stable VLLDP -> VR per-cycle ordering result.

### Fresh VLLDP object takes the deep path

The newly instantiated VLLDP150 outer callback at `0x00007ffa245b5050` fired immediately. At entry:

```text
this = 0x0000020345560008
[this+0x70] = 0x00
input  = 480 frames, VALID
output = 480-frame buffer, INVALID at entry
LR     = 0x00007ffa2474d664 (inside DolbyDax3Apo wrapper)
```

The zero gate byte confirms the newly created object takes the deeper VLLDP processing path rather than the short copy/pass-through branch.

The hardware breakpoint on the byte-locked inner orchestrator then fired immediately:

```text
DolbyAPOvlldp150 + 0x1f7a8
runtime 0x00007ffa244cf7a8
```

This proves the dormant->active wake transition feeds directly into the same `FUN_18001f7a8` snapshot/crossfade/state-history orchestrator already proven hot in steady-state music playback.

### Exact external/internal block scheduling: 480 -> 256 + 224

The wake capture exposed a critical scheduling detail for native parity.

The Windows APO callback receives a 480-frame stereo block, but one outer VLLDP150 callback invokes `FUN_18001f7a8` twice before the next outer callback:

```text
480 frames = 256 frames + 224 frames
             0x100        0x0e0
```

The first orchestrator state header included:

```text
state+0x08 = 0x0000bb80`00000100
             48000 Hz     256 frames
state header also contains 2 / 2, consistent with stereo
```

The live descriptor construction at the immediate caller is:

```text
channels = 2
stride   = 2
format   = 7
planes   = { base, base + 4 }
```

This represents interleaved float stereo as two channel pointers one float apart with stride 2.

On the second orchestrator call, source and destination pointers advanced by exactly `0x800` bytes:

```text
0x800 = 256 frames * 2 channels * 4 bytes
```

Live registers on that second call explicitly contained both:

```text
0x100 = 256
0x0e0 = 224
```

Execution ordering was observed directly as:

```text
VLLDP150 outer APO callback (480 frames)
  -> orchestrator call #1 (first 256 frames)
  -> orchestrator call #2 (remaining 224 frames, pointers +0x800)
-> next VLLDP150 outer APO callback
```

This is a first-class Linux parity requirement. A free-running native VLLDP150 harness that assumes one external callback equals one internal processing quantum will not reproduce Windows state/history timing exactly.

### Revised parity priority

Before fitting any residual EQ or assuming AIDE is the missing steady-state speaker processor, reproduce the live VLLDP150 wrapper cadence exactly:

```text
480-frame Windows-style external callback
  -> persistent VLLDP150 object/state
  -> 256-frame orchestrator call
  -> 224-frame orchestrator call
  -> preserve all state/history into the next 480-frame callback
```

The modern ASAR DLL is clearly reloaded on wake, but its previously trapped low-level AIDE/DAPVR/embedded-VLLDP blocks remain unproven as the hot steady-state stereo speaker path. The driver DAX3 -> VLLDP150 path is directly proven hot and now has its external/internal scheduling decoded live.
