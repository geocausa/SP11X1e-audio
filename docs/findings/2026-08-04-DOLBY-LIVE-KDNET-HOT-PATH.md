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

### Corrected block-domain model: 480 host -> 432 outer scheduler -> 256 inner VLLDP engine

The earlier interpretations of either a universal per-callback `480 = 256 + 224` split or a single 480->256 rolling FIFO were too simple. A fresh post-dormancy reinstantiation capture exposed two distinct block domains inside VLLDP150 in addition to the 480-frame Windows Audio Engine callback.

The outer APO connection properties at the scheduler entry were:

```text
input frames = 480 (0x1e0)
input flag   = 1 (VALID)
output frames/capacity = 480
output flag  = 0 (INVALID at entry)
```

The hot scheduler/state-machine function is at module RVA `0xED348` in this build. WinDbg labels the same address as `DolbyAPOvlldp150!DllUnregisterServer+0xDA928`; the `0xDA928` number is symbol-relative, not the module RVA.

At a fresh active-graph scheduler hit its object fields were:

```text
obj+0x00 = 3       // current scheduler/state-machine mode
obj+0x04 = 1024
obj+0x08 = 432     // outer scheduler limit used by the accumulation loop
obj+0x0C = 0       // persistent fill/phase field at this capture
```

The core loop directly uses `obj+0x08` and `obj+0x0C`:

```text
limit          = obj->u32_08       // 432 in this live graph
current_offset = obj->u32_0C       // persistent state
remaining      = input_frames

while (remaining != 0) {
    space = limit - current_offset;
    take  = min(remaining, space);

    helper/process/copy/mix take frames through the outer staging state;

    remaining      -= take;
    input_ptr       += take * input_channels  * sizeof(float);
    output_ptr      += take * output_channels * sizeof(float);
    current_offset += take;

    if (current_offset == limit)
        current_offset = 0;
}
```

The decisive instructions in the live scheduler body are:

```text
ldr  w9,[x20,#0x0c]      // persistent current_offset
sub  w8,w21,w9           // space = obj+0x08 - current_offset
cmp  w23,w8
cselhs w19,w8,w23        // take = min(remaining, space)
...
sub  w23,w23,w19         // remaining -= take
...                        // advance input/output pointers by take*channels*4
ldr  w8,[x20,#0x0c]
add  w8,w19,w8
str  w8,[x20,#0x0c]
cmp  w8,w21
bne  ...
str  wzr,[x20,#0x0c]     // wrap at the outer scheduler limit
cbnz w23,...              // continue until callback input is consumed
```

This is an **outer 432-frame scheduler/state machine**, not the 256-frame VLLDP core itself.

#### Inner VLLDP object proves the separate 256-frame engine

The scheduler subobject at `scheduler+0x12C040` points to the live inner VLLDP object. In the fresh graph:

```text
inner object = 0x0000020bdc68c1f8
inner+0x08 -> header/config
inner+0x20 = 0x100 = 256
inner+0x28 -> main VLLDP state
inner+0x30 -> auxiliary state
```

The referenced header/config contains:

```text
48000 Hz
2 channels
2 channels
```

The main VLLDP state itself begins with:

```text
state+0x08 = 0x0000bb80`00000100
             48000 Hz     256 frames
state+0x10 = 0x00000002`00000002
             stereo / stereo
```

So the 432 and 256 values are both genuine and belong to **different layers**.

The inner object's vtable further anchors the call structure:

```text
vtable +0x60 -> module RVA 0x35160   descriptor/dispatch shim
vtable +0xB0 -> module RVA 0x105000  outer processing-wrapper family
```

A live scheduler hit was followed immediately by the hardware breakpoint at module RVA `0x1F7A8`, proving that the outer scheduler feeds the same hot VLLDP orchestrator in the active graph.

The orchestrator descriptors remain:

```text
channels = 2
stride   = 2
format   = 7
planes   = { base, base + 4 }
```

This is interleaved float stereo represented as two channel pointers one float apart with stride 2.

The current proven layering is therefore:

```text
DAX3 wrapper APOProcess
  -> VLLDP150 outer APO callback       module RVA 0x105050
       [this+0x70 == 0: deep path]
       -> outer scheduler/state machine module RVA 0x0ED348
            [live object limit = 432]
            -> inner VLLDP object
                 [inner quantum = 256]
                 -> descriptor shim     module RVA 0x035160
                 -> orchestrator        module RVA 0x01F7A8
```

Do **not** infer a fixed `256+224` orchestrator pattern from the earlier callee-saved register snapshots. Those register pairs were caller temporaries and are not a formal frame-count ABI. The assembly/object fields above are authoritative.

#### Orchestrator state mutation snapshots

Four narrow binary snapshots were captured around two consecutive steady-state orchestrator calls from one live object:

```text
entry #1 -> return #1 -> entry #2 -> return #2
```

Diff results:

```text
main state: entry1 -> return1   6725 changed bytes
main state: return1 -> entry2      0 changed bytes
main state: entry2 -> return2   4687 changed bytes

aux state:  entry1 -> return1   2887 changed bytes
aux state:  return1 -> entry2      0 changed bytes
aux state:  entry2 -> return2   2781 changed bytes
```

This proves that the large DSP/history mutation occurs **inside `FUN_18001f7a8`**. No captured main/aux state bytes changed between return of call #1 and entry of call #2.

The first mutable region in the main object begins near `state+0xB60`. Three consecutive 20-element runtime arrays are present there:

```text
state+0xB60 .. +0xBAC   20 x int32
state+0xBBC .. +0xC08   20 x float
state+0xC0C .. +0xC58   20 x int32
```

All three mutate on each orchestrator call, strongly matching the 20-band topology, although their exact semantic names are not yet proven. They do not closely match the older exported `FUN_18001de90` vector corpus, so they are a distinct live runtime-state family rather than another copy of the already-known export vectors.

A selector at `state+0x1630` changed:

```text
entry1  1
return1 0
entry2  0
return2 1
```

This is consistent with a ping-pong/history-bank selector, but that label remains provisional until its readers/writers are decoded statically.

#### Fresh post-reinstantiation state vs steady state

A later long-pause/resume cycle replaced the dormant `audiodg` with a fresh active host (PID `0x1764`). The new process loaded the full stack again, including `DolbyAudioProcessing.dll` and `DolbyHrtfEnc.dll`.

The first captured scheduler object in that fresh graph had the outer fields `state=3`, `1024`, `432`, `offset=0` shown above. A matching orchestrator entry/return pair was captured from the same new graph.

Fresh-graph diff:

```text
main state: entry -> return   6595 changed bytes
aux state:  entry -> return   2788 changed bytes
first changed main-state byte: +0xB60
selector +0x1630: 1 -> 0
```

The fresh graph therefore uses the **same state layout and mutation machinery** as the prior steady-state graph, but begins from materially different 20-band/history values. This supports a reset/reinitialization-of-history model rather than a different DSP algorithm after wake.

Example `state+0xB60` 20-band int32 array:

```text
steady entry:
-285,-251,-316,-370,-423,-515,-569,-594,-583,-588,-657,-683,-681,-682,-695,-735,-772,-748,-636,-672

fresh graph entry:
-308,-265,-318,-359,-382,-386,-447,-486,-467,-482,-508,-549,-653,-648,-568,-599,-686,-643,-655,-785
```

Both calls still mutate the same array family and flip the same selector.

#### Local evidence hashes

The raw state snapshots are intentionally retained locally on SP7 rather than committed publicly because they contain live DSP buffers from playback and may include reconstructable audio content.

Local directory:

```text
C:\Users\SurfacePro7\Documents\KDNET\dolby-vlldp-state-20260804
```

Steady-state SHA256:

```text
entry1_aux.bin      917965A2B6BB2340820A7CCB9B65C7BE4E76FC048275FC0368D4BB0DF832685C
entry1_state.bin    93F188D4004C10D85B4F19F6A00291EB9510EBB1BB9B571AE20E6672117F28B9
entry2_aux.bin      C2797462CC94E1A21A4D13556BFBC90F7B3F74D2F469FBB66CBBD9F4C80034C0
entry2_state.bin    B95D0A970D4AB199469494BE1680380109D9733AB3B0B5379F54DD335263CA18
return1_aux.bin     C2797462CC94E1A21A4D13556BFBC90F7B3F74D2F469FBB66CBBD9F4C80034C0
return1_state.bin   B95D0A970D4AB199469494BE1680380109D9733AB3B0B5379F54DD335263CA18
return2_aux.bin     32C4A1E9CCB552B9DF386512B8DC32C493E771715381677618150B58DC68E612
return2_state.bin   8C5775549D175D39FAC9A5BA901B43F1D6C0BAB9811D8E34EFA5CAF79B2A6262
```

Fresh-graph SHA256:

```text
freshgraph_orch_state.bin         7571895A431CC9D203E46BB3F76E8D40D9FC90EFA30153CF82BAA5CCC5445017
freshgraph_orch_return_state.bin  CA32E99A4CB7A0BCEFE09E54C5DF61BD56CC28CA7BC2C82C869F9EE2B9CF63A1
freshgraph_orch_aux.bin           9F7AFF7703EFCC420B7528581725AF67C224F952C55D94F74AFA6E8F59F4C440
freshgraph_orch_return_aux.bin    E4DD1DC96F49A430BF23C1168AD68DFC324F42B00842E990D45C2406C94BB70B
```

### Revised parity priority

Before fitting residual EQ or returning to AIDE as the presumed steady-state missing processor, reproduce the **layered live wrapper behavior**:

```text
480-frame Windows Audio Engine callback
  -> persistent outer scheduler/state machine (live limit 432)
  -> persistent inner VLLDP object (256-frame engine quantum)
  -> exact descriptor shim / orchestrator call sequence
  -> preserve both outer scheduler state and inner DSP/history state across callbacks
```

The remaining unknown is the exact scheduling relationship by which the 432-frame outer state machine feeds one or more 256-frame inner orchestrator invocations. That should be measured directly rather than inferred from temporary registers.

The modern ASAR DLL is clearly reloaded on wake, but its previously trapped low-level AIDE/DAPVR/embedded-VLLDP blocks remain unproven as the hot steady-state stereo speaker path. The driver DAX3 -> VLLDP150 path is directly proven hot and now has its external/internal scheduling decoded live.
