# Windows normal-shared ASAR dual-lane pre-VLLDP closure — 2026-08-08

## Status

This is the durable post-18:00 checkpoint for the August 8 Windows oracle work.
It records evidence obtained after repository commit `849350d` and is intended to
survive chat/context loss.

The key correction is narrow but important:

- the 2026-08-06 finding that the **matching stereo channel bed itself** follows
  the HRTF stereo-bypass identity path remains correct;
- the stronger conclusion that the **whole ASAR output** is therefore identity
  is no longer correct for the current normal-shared SP11 speaker graph;
- the current graph simultaneously carries a VirtualSurround spatial-object
  contribution, and `DolbyHrtfEnc::Process` merges the unity bed into already
  produced encoded/object output.

Do not implement a fitted fixed gain from these measurements. The measured
pre-VLLDP transfer is level dependent.

---

## 1. Current Windows boundary measurement

A controlled WASAPI float stereo probe was run on the SP11 with normal shared
rendering. Full-memory `audiodg.exe` dumps were taken during steady 75-Hz tones
at multiple amplitudes and during a 997-Hz control tone.

Measured source amplitude -> live VLLDP input-staging peak:

```text
75 Hz input 0.10 -> ~0.320
75 Hz input 0.25 -> ~0.528
75 Hz input 0.50 -> ~0.99987
75 Hz input 0.70 -> ~0.99987

997 Hz input 0.25 -> ~0.5229
```

The same 0.25 input therefore receives almost the same pre-VLLDP lift at 75 Hz
and 997 Hz. This is not explained by a simple bass shelf or EQ stage. The
amplitude dependence also disproves a single fixed `+6.5 dB` replacement gain.

The 0.50/0.70 cases already drive the VLLDP input to essentially full scale;
VLLDP's own final limiter can then become active downstream. This is consistent
with the separately recovered Aug-7 live VLLDP state and does not require a
secret positive VLLDP system gain or a forced negative peak-level setting.

---

## 2. The channel bed is still identity

The existing exact stereo-bypass work remains valid for the ordinary channel
bed:

```text
CAdaptiveSpatialAudioRenderer::APOProcess
  -> ASAR::MainPluginRenderer::Process
  -> AsarEncoderWrapper<IAsarEncoder2>::MixChannelBed
  -> DolbyHrtfEnc::MixChannelBed
```

For the matching stereo DAX-speaker path:

- `StereoBypassMode` is enabled;
- input/output are 48 kHz;
- `MainPluginRenderer` supplies bed scale `1.0`;
- `DolbyHrtfEnc::MixChannelBed` stages the original bed pointer/sample count and
  scale without applying HRTF/fold-down processing.

So there is still no evidence for an invented stereo widening matrix or generic
HRTF convolution on the ordinary bed.

---

## 3. VirtualSurround carries a second copy of the same media into ASAR

Fresh current-build disassembly of `VirtualSurroundApo.dll` closes the source of
the spatial-object lane.

On the live branch, `CVirtualSurround::APOProcess` obtains an ASAR object for
writing and calls its sample writer with the **input APO connection pBuffer**.
The writer deinterleaves/copies those floats into the ASAR object buffer with
plain float load/store operations; no gain multiply is applied in that copy.

Therefore the VirtualSurround object is not an unrelated sound source. It is
fed from the same normal-shared media samples that also remain available as the
ordinary channel bed.

The steady full-memory dump also contains exactly one active non-blocking stream
node in `MainPluginRenderer`; that node resolves to the live VirtualSurround COM
stream object. This rejects a naive explanation involving a list containing two
accidental duplicate stream nodes.

---

## 4. `DolbyHrtfEnc::Process` merges object output and the bed

The old ASAR identity note implicitly treated the HRTF staged-bed fallback as if
no competing encoded output existed. The current process-path disassembly closes
that assumption.

The encoder has a scalar vector primitive with exact semantics:

```c
dst[i] += src[i] * scale;
```

The current `DolbyHrtfEnc::Process` control flow distinguishes two cases:

```text
no encoded/object output:
    copy or scaled-copy staged bed to output

encoded/object output already produced:
    preserve that output
    merge the staged bed into it
```

For the current stereo bed scale of `1.0`, the merge uses the unity/add path.
For a non-unity positive bed scale, the scaled-add helper is selected.

The normal-shared equation is therefore structurally:

```text
ASAR output = encoded/spatial-object contribution + unity stereo bed
```

This is now a code-path result, not an amplitude-fit hypothesis.

The exact magnitude/transfer law of the encoded spatial-object contribution is
still the remaining DSP question.

---

## 5. Object-lane scale and state

The recovered object path includes a deliberate float constant:

```text
0.707945764
```

which is approximately -3 dB. Live per-object descriptor gain fields checked in
the steady dump are `1.0`.

This rules out a hidden per-object `~1.12x` volume field as the explanation for
the low-level residual. Do not reduce the object lane to a fitted scalar: the
measured end-to-end transfer is level dependent.

---

## 6. Authoritative SP11 Dynamic content-processing tuning

The active internal speaker hardware is:

```text
AUCD\VEN_QCOM&DEV_0C29&SUBSYS_MSHW0486&REV_0D
```

The authoritative Surface Dolby tuning is therefore the MSHW0486 REV_0D file.
Its Dynamic profile has separate content-processing and VLLDP/device sections.

Relevant `tuning-cp` values:

```text
volume-leveler-enable       = 1
volume-leveler-amount       = 5
volume-leveler-drc-enable   = 1
volume-leveler-in-target    = -320
volume-leveler-out-target   = -320
regulator-enable            = 1
regulator-relaxation-amount = 96
regulator-timbre-preservation = 12
surround-decoder-enable     = 1
surround-boost              = 96
volmax-boost                = 96
pregain                     = 0
postgain                    = 0
system-gain                 = 0
```

Relevant `tuning-vlldp` values are a separate downstream/device domain and
include Audio Optimizer/device regulation while multiband compressor is off.

This separation matters: the new pre-VLLDP measurement is occurring on the
content/spatial side of the graph, not because VLLDP has acquired an unexplained
positive system gain.

---

## 7. Direct modern-DAP research harness is not the production answer

The repository already contains a native research harness for internal APIs in
`DolbyAudioProcessing.dll` (`sp11_dolby_native_known_input.c` and
`sp11_dapvr_native_measure.c`). The same APIs were invoked directly on native
ARM64 Windows as an oracle during this investigation.

A partial Dynamic-like configuration produced valid deterministic DSP output,
but its transfer is too aggressive at low/medium level:

```text
input 0.10 -> native direct-DAP last peak ~0.4516
input 0.25 -> native direct-DAP last peak ~0.7617
input 0.50 -> native direct-DAP last peak ~0.9605
input 0.70 -> native direct-DAP last peak ~0.9684
997 Hz 0.25 -> native direct-DAP last peak ~0.7856
```

The same path can reach the familiar ~0.99987 ceiling, proving that real vendor
level-management machinery is active, but the transfer does **not** match the
current normal-shared Windows pre-VLLDP oracle.

This is consistent with the earlier KDNET result that the low-level DABS speaker
wrapper used by the research harness was cold in the tested persistent speaker
condition. Do not integrate this direct DAPVR harness into production as a
replacement for the ASAR route.

---

## 8. Original Dolby ASAR components can now be activated offline

A major new closure is that the original vendor ASAR components can be created
successfully in an isolated ordinary ARM64 Windows process.

Using the copied, hash-pinned private vendor DLLs and their own
`DllGetActivationFactory`:

```text
Dolby.DolbyAtmosForSpeakersEncoder
    activation: success

Dolby.DolbyAudioProcessingModule
    activation: success
```

The fresh `Dolby.DolbyAtmosForSpeakersEncoder` object contains the exact
`IAsarEncoder2` interface used by the live AudioEng graph:

```text
IAsarEncoder2 subobject offset = +0x10
vtable RVA                     = 0x19E88
```

This matches the live encoder layout rather than being an unrelated public
wrapper object.

The practical consequence is important: the missing normal-shared stage can be
recovered by hosting the **original vendor encoder + original vendor DAP module**
in an isolated process and driving the same bed/object interfaces, instead of
reimplementing the level-dependent object lane by curve fitting.

The activation method is now preserved as a public, parameterized probe:

```text
tools/windows/Probe-DolbyActivationFactory.ps1
```

It accepts a private local DLL path and activatable class name, calls that
DLL's own `DllGetActivationFactory`, activates the class, and reports object
subobject/vtable RVAs. It embeds no vendor binary and was re-run successfully
against both hash-pinned private DLLs before this checkpoint was committed.

Immediate plumbing target:

1. identify/call the plugin `SetEncoderEngine` method;
2. identify/call plugin/engine `Initialize` with the same stereo/48-kHz contract;
3. connect a fresh `Dolby.DolbyAudioProcessingModule` instance;
4. feed controlled VirtualSurround object samples plus the unity channel bed;
5. call the exact `IAsarEncoder2::Process` path;
6. compare output against the state-pinned VLLDP-input oracle at 0.10/0.25/0.50/0.70 and 997 Hz;
7. only after a match, decide how to host the stage on Linux.

---

## 9. Private evidence manifest

The raw dumps, disassembly captures and proprietary Dolby binaries remain local
and must not be committed. Their hashes are recorded here so conclusions can be
revalidated against exact bytes.

Private workspace:

```text
C:\Users\Geoca\Documents\SP11-Dolby-Windows-Oracle-20260807
```

Key artifacts:

```text
asar-normal-unity-steady.dmp
  bytes  118163184
  SHA256 855C7560492EE7FCCF680125A15373E54BDD8CE8438FEC2EF8A0781DECE43888

amp010-steady.dmp
  bytes  118212208
  SHA256 D667E117C2F9F51B3F27903DAA78A4725BE6CDC1D98996E409557B27B79ED119

amp050-steady.dmp
  bytes  118220464
  SHA256 0810827827D6374876316797E4DD2A8C85034751A84EFAE8F4AC4BDF53D6C590

amp070-steady.dmp
  bytes  118080176
  SHA256 A3E28F5DBDD646E15B9A45617828CF4E4615DE508BC0DAE0887C996FE74E0440

amp025-997hz-steady.dmp
  bytes  118204112
  SHA256 35524BFD32CCEA5BEA26FB87D0B11C57B990BADC16687CC51F20B88440FA3194

dolby-hrtf-process-disasm.txt
  SHA256 E06E2E49D5E70E799100C5A713619BEC9B4C020B78C7A0A4EFC6225AD66E675E

virtualsurround-apoprocess-disasm.txt
  SHA256 31BE874F7BA7054E15E77E11A1873B869066F428495C94323F2C2284FA7E77C6

Run-DapCpNativeOracle.ps1
  SHA256 86549D96FCB71DE2BD4A475249CEBF3B21ECB5DB37B59EF42FF251AA468AB29C

private-dll\DolbyAudioProcessing.dll
  SHA256 900944A1F96292813FF5C56D30D49663851FE368E709F53681EE7A0C0A84D0D3

private-dll\DolbyHrtfEnc.dll
  SHA256 B1AD1FA8ED747CA1FB58125FB0D1819C38A2AC644A60D1D5F392EA46D5463038
```

The binaries and dumps are provenance/evidence only. Public Git should contain
hashes, analysis tools, small reviewed derived fixtures and documentation, not
vendor DLL payloads or process dumps.

---

## 10. What is proved vs still open

### Proved

- normal shared and RAW do not have the same pre-VLLDP sample path;
- the ordinary stereo ASAR bed is unity under the SP11 stereo-bypass policy;
- VirtualSurround submits the same input media into a separate ASAR object lane;
- the HRTF encoder merges already-produced encoded/object output with the staged
  unity bed;
- the normal-shared pre-VLLDP transfer is wideband and level dependent;
- the real vendor speaker encoder and companion Dolby audio-processing module
  can be activated offline in an isolated process;
- the fresh encoder contains the same `IAsarEncoder2` subobject/vtable family as
  the live AudioEng graph.

### Still open

- the exact initialization/engine-binding ABI between the fresh HRTF plugin and
  `Dolby.DolbyAudioProcessingModule`;
- the exact object-lane transfer law/state that produces the measured 0.320 /
  0.528 / near-1.0 boundary points;
- whether the final Linux production solution can host this ASAR pair directly
  with a small Windows-runtime shim, or whether a smaller original-code DSP
  subset is preferable after the API contract is recovered.

### Explicitly rejected for production

- a fixed +3 dB or +6.5 dB pre-gain;
- forced VLLDP `peak=-48`;
- positive fake VLLDP system gain;
- a generic HRTF convolution/widening effect;
- wholesale integration of the low-level modern-DAP research harness just
  because it produces similar limiting behavior.

The next engineering work should continue from the isolated original encoder
activation, not from fitted gain experiments.

## Offline activation ABI closure after tool-session recovery

The remaining encoder-plumbing calls are now identified directly from the SP11
ARM64 binaries plus Microsoft `AudioEng.dll` public symbols.

### Encoder interfaces

A fresh `Dolby.DolbyAtmosForSpeakersEncoder` object answers these relevant QIs:

```text
IID deff1192-f581-4d77-9c1b-3e596b0ca989
  object offset +0x10
  vtable RVA 0x19E88
  slot +0x18 / index 3 -> DolbyHrtfEnc.dll+0x5110
  = CDolbyHrtfEncoderPlugin::Initialize

IID dc57ddb4-e086-49ec-b13d-eccdd512990c
IID 54151d15-066e-441c-81e7-d894d8a0abc7
  both alias object offset +0x18
  vtable RVA 0x199D8
  slot +0x18 / index 3 -> DolbyHrtfEnc.dll+0x4DE0
  = CDolbyHrtfEncoderPlugin::SetEncoderEngine
  slot +0x20 / index 4 -> DolbyHrtfEnc.dll+0x4F60
  = copies/caches the APO initialization context used by Initialize

IID 98f37dac-d0b6-49f5-896a-aa4d169a4c48
  object offset +0x28
  vtable RVA 0x19AC8
  slot +0x18 / index 3 -> DolbyHrtfEnc.dll+0x4D60
  = stores an additional IUnknown dependency
```

`SetEncoderEngine` takes an IUnknown-like engine object and QIs it for two DAP
module interfaces. On this SP11 build the embedded IIDs are:

```text
ed52ac8d-2018-4d17-9fb2-e7eb4870ee4e
2b2e53bc-651b-4a90-8f64-38531c154fda
```

The fresh `Dolby.DolbyAudioProcessingModule` object rejects the first IID but
accepts `2b2e53bc-651b-4a90-8f64-38531c154fda` at object offset `+0x08`, so the
original HRTF `SetEncoderEngine` call can bind the fresh original DAP module
without a surrogate interface.

The DAP object also accepts the previously closed stereo-bypass interface:

```text
b5bb3cae-fd91-497d-8b83-2eddce0808db
  object offset +0x10
  vtable RVA 0x3128D0
  slot +0x18 / index 3 -> DolbyAudioProcessing.dll+0x1A160
  = CDolbyAudioProcessingModule::GetStereoBypassAllowed
```

### Exact AudioEng wrapper contract

Microsoft symbols expose the full wrapper signature:

```cpp
ASAR::AsarEncoderWrapper<IAsarEncoder2>::Initialize(
    IUnknown *,
    IUnknown *,
    IUnknown *,
    APOInitSystemEffects3 const *,
    unsigned int,
    unsigned int,
    AudioObjectType,
    GUID const &,
    unsigned int,
    unsigned long,
    ISpatialAudioPositionMapper *,
    GUID const &,
    IUnknown *);
```

The wrapper performs the original plumbing before invoking the vendor
`IAsarEncoder2::Initialize`:

1. QI the encoder for `98f37dac-d0b6-49f5-896a-aa4d169a4c48` and pass the
   wrapper's third IUnknown to that interface's slot 3;
2. prefer the encoder interface family `fa1d346f...`, then `dc57ddb4...`, then
   `54151d15...`; the SP11 Dolby speaker encoder selects `dc57ddb4...`;
3. on that selected interface, pass the APO initialization context through slot
   4 when present;
4. pass the wrapper's second IUnknown through slot 3, which is the recovered
   `CDolbyHrtfEncoderPlugin::SetEncoderEngine` call;
5. call `IAsarEncoder2::Initialize` at vtable slot 3.

The ARM64 call setup closes the argument projection from the 13-argument
AudioEng wrapper to Dolby's eight explicit `Initialize` arguments. With wrapper
arguments numbered 1..13 after `this`, Dolby receives:

```text
Dolby Initialize arg1 <- wrapper arg5   (unsigned int)
Dolby Initialize arg2 <- wrapper arg6   (unsigned int)
Dolby Initialize arg3 <- wrapper arg7   (AudioObjectType)
Dolby Initialize arg4 <- wrapper arg9   (unsigned int)
Dolby Initialize arg5 <- wrapper arg10  (unsigned long)
Dolby Initialize arg6 <- wrapper arg11  (ISpatialAudioPositionMapper *)
Dolby Initialize arg7 <- wrapper arg12  (GUID const &)
Dolby Initialize arg8 <- wrapper arg13  (IUnknown *)
```

`MainPluginRenderer::Initialize` also exposes the live sources for those wrapper
arguments. In particular wrapper arg6 is obtained by converting the active
format/sample-rate field at `status+0x108`, and the same call constructs the
object-type mask and forwards the live GUID/runtime-parameter fields rather than
inventing replacement values.

This removes the previous ABI guesswork. The next isolated harness should copy
this wrapper call order exactly, then submit the already mapped bed/object calls
and compare `IAsarEncoder2::Process` output against the measured pre-VLLDP
oracle.
