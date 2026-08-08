# SP11 Dolby continuation checkpoint — 2026-08-08

## Scope / hard constraint

- Work on **SP11 Windows only**.
- Do **not** use or modify SP7 for this investigation unless the user explicitly changes that instruction.
- Repository: `geocausa/SP11X1e-audio`.
- Working branch at this checkpoint: `agent/aug8-asar-dual-lane-checkpoint`.

## Why this checkpoint exists

This file is a compact-resilient continuation record. It captures the state required to resume the SP11 Dolby/ASAR investigation even if chat history is compacted or lost.

## Current proven architecture

The remaining missing Windows spatial stage is no longer speculative reverse engineering. The original SP11 Dolby WinRT components can be activated out-of-process and their interface topology matches the live AudioEng path.

The two original classes involved are:

- `Dolby.DolbyAtmosForSpeakersEncoder` from `DolbyHrtfEnc.dll`
- `Dolby.DolbyAudioProcessingModule` from `DolbyAudioProcessing.dll`

The live Windows shared-spatial chain is centered on `ASAR::MainPluginRenderer` / `AsarEncoderWrapper<IAsarEncoder2>` calling the Dolby HRTF encoder, which in turn consumes a Dolby DAP module and produces the pre-VLLDP material we need to reproduce offline.

## Fresh encoder activation / interface closure

Fresh `Dolby.DolbyAtmosForSpeakersEncoder` activation succeeds.

Relevant QIs on this SP11 build:

```text
IID deff1192-f581-4d77-9c1b-3e596b0ca989
  object offset +0x10
  vtable RVA 0x19E88
  slot 3 / +0x18 -> DolbyHrtfEnc.dll+0x5110
  = CDolbyHrtfEncoderPlugin::Initialize

IID dc57ddb4-e086-49ec-b13d-eccdd512990c
IID 54151d15-066e-441c-81e7-d894d8a0abc7
  aliases at object offset +0x18
  vtable RVA 0x199D8
  slot 3 / +0x18 -> DolbyHrtfEnc.dll+0x4DE0
  = CDolbyHrtfEncoderPlugin::SetEncoderEngine
  slot 4 / +0x20 -> DolbyHrtfEnc.dll+0x4F60
  = copies/caches the APO init context used by Initialize

IID 98f37dac-d0b6-49f5-896a-aa4d169a4c48
  object offset +0x28
  vtable RVA 0x19AC8
  slot 3 / +0x18 -> DolbyHrtfEnc.dll+0x4D60
  = stores an additional IUnknown dependency
```

`SetEncoderEngine` QIs the supplied engine object for two DAP-module interface IIDs embedded in `DolbyHrtfEnc.dll`:

```text
ed52ac8d-2018-4d17-9fb2-e7eb4870ee4e
2b2e53bc-651b-4a90-8f64-38531c154fda
```

The fresh original `Dolby.DolbyAudioProcessingModule` rejects the first IID but accepts:

```text
2b2e53bc-651b-4a90-8f64-38531c154fda
```

at object offset `+0x08`, proving the original DAP object can be passed directly to the original encoder's `SetEncoderEngine` call. No surrogate encoder-engine interface is required.

The DAP object also accepts the previously closed stereo-bypass interface:

```text
b5bb3cae-fd91-497d-8b83-2eddce0808db
  object offset +0x10
  vtable RVA 0x3128D0
  slot 3 / +0x18 -> DolbyAudioProcessing.dll+0x1A160
  = CDolbyAudioProcessingModule::GetStereoBypassAllowed
```

## Exact AudioEng initialization wrapper

Microsoft public symbols expose the exact wrapper signature:

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

Recovered call order before the vendor `IAsarEncoder2::Initialize` call:

1. QI encoder for `98f37dac-d0b6-49f5-896a-aa4d169a4c48` and pass wrapper arg 3 through that interface slot 3.
2. Select the encoder setup interface family in the observed preference order `fa1d346f...`, then `dc57ddb4...`, then `54151d15...`; the SP11 Dolby speaker encoder selects the `dc57ddb4...` family.
3. Pass the APO initialization context through setup-interface slot 4 when available.
4. Pass wrapper arg 2 through setup-interface slot 3 = `CDolbyHrtfEncoderPlugin::SetEncoderEngine`.
5. Invoke `IAsarEncoder2::Initialize` at vtable slot 3.

Argument projection from the 13-argument AudioEng wrapper into Dolby's eight explicit `Initialize` arguments is now recovered:

```text
Dolby Initialize arg1 <- wrapper arg5
Dolby Initialize arg2 <- wrapper arg6
Dolby Initialize arg3 <- wrapper arg7
Dolby Initialize arg4 <- wrapper arg9
Dolby Initialize arg5 <- wrapper arg10
Dolby Initialize arg6 <- wrapper arg11
Dolby Initialize arg7 <- wrapper arg12
Dolby Initialize arg8 <- wrapper arg13
```

`MainPluginRenderer::Initialize` also exposes the live origins for these values. In particular wrapper arg6 is derived from the active format/sample-rate field at `status+0x108`; the same path constructs the live object-type mask and forwards the live GUID/runtime-parameter fields rather than fabricated constants.

## Prior signal-path closure retained

Earlier work established that the live shared-mode Windows graph contains direct stacks joining `AudioEng!ASAR::MainPluginRenderer` / `AsarEncoderWrapper<IAsarEncoder2>` to `DolbyHrtfEnc.dll`, while RAW mode leaves ASAR present but suppresses the Dolby HRTF execution path. Matching stereo may take Dolby's dry unity-gain bypass path, so plugin presence alone is not proof of coloration.

The measured transfer-curve work also showed the Dolby behavior under test is broadband and level-dependent rather than merely a bass boost; for example the measured 997 Hz result at the referenced low-level point was essentially the same as the 75 Hz result.

## Tooling changes made

`tools/windows/Probe-DolbyActivationFactory.ps1` was extended so an activation probe can:

- accept arbitrary interface IID(s),
- call `QueryInterface`,
- print HRESULT, returned subobject offset, vtable address and module RVA,
- retain the existing object/vtable scan.

This was used to prove the interface mappings above on fresh SP11 objects.

## Key commits

```text
045c4a3  Document Aug8 ASAR dual-lane closure
bcb16ce  Map Dolby encoder initialization ABI
```

This checkpoint file is the next commit after those.

## Exact next engineering step

Build a small isolated **SP11 user-mode harness** that:

1. activates the original `Dolby.DolbyAtmosForSpeakersEncoder`;
2. activates the original `Dolby.DolbyAudioProcessingModule`;
3. reproduces AudioEng's exact initialization order and argument projection above;
4. feeds the already mapped bed/object data through the real `MixChannelBed`, `SetAudioObject` / metadata path as appropriate, and `IAsarEncoder2::Process`;
5. captures the produced PCM/blocks;
6. compares that output deterministically against the already measured **pre-VLLDP oracle**.

If that matches, the missing Windows stage is reproduced with the original Dolby implementation rather than approximated DSP.

## Operational note

No reboot, custom driver, kernel work, or PPL modification is required for the next step. Continue in SP11 userland from the saved dumps, private Dolby DLLs, public AudioEng symbols, and this branch.
