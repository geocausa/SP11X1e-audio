# ASAR oracle constructor-default state closure — 2026-08-09

## Result

The Aug-8 normal/shared pre-VLLDP oracle was produced from the original Dolby DAP-VR **constructor/default effective state**, not from the later Dynamic-like values visible in the frozen dump's requested fields.

This resolves the earlier apparent mixed-profile contradiction.

## Timing proof

The exact original-Dolby control path is now closed:

```text
DAX shared-memory receive
  -> AsarParamsReaderController::ParamsReceived
  -> registered CDolbyAudioProcessingModule lambda
  -> DAP-VR SetParams(payload, size, Runtime=1)
  -> setters update requested fields + dirty flags

next audio block:
  CDolbyAudioProcessingModule::EncodeAudioData
    -> OAR
    -> AIDE
    -> DAP-VR Prepare
       -> FUN_180046FA0
       -> FUN_180047F08
       -> FUN_1800484F0  // commit requested -> effective
    -> DAP-VR Process
```

`FUN_1800484F0` has only two callers in this build: constructor/setup and the live DAP-VR Prepare path. The DAX receive lambda calls DAP-VR SetParams with Runtime mode `1`; it does not call the commit routine.

Therefore a DAX update that arrives after the last processed block and before the next Prepare leaves exactly the state seen in the Windows dump: newer requested values/dirty flags together with the older effective values that generated the retained PCM.

## Constructor-default hypothesis tested directly

A controlled Linux-hosted original-DLL run kept the normal DAP-VR init property but deliberately made the DAHP PID-5 runtime property unavailable. No raw DSP replacement was introduced.

The resulting original DAP-VR core immediately exposed the same values seen in the Windows dump's older effective/current fields:

```text
volmax                  144-equivalent  (scaled 0.06923077)
surround boost           96-equivalent  (scaled 0.04615385)
MI steering active       0
Dialog enhancer active   0
Dialog amount active     0
Volume leveler active    0
Volume leveler amount    7
IEQ active               0
IEQ amount              10 / 16 = 0.625
Regulator timbre        16 / 16 = 1.0
Regulator relaxation    96
Regulator distortion     1
```

These are not a stock Dynamic/Movie/Music XML profile. They are the original DAP-VR constructor/default processing state.

The later requested fields in the Windows dump remain consistent with a Dynamic DAX update waiting for the next Prepare/commit.

## Transfer consequence

Removing PID-5 runtime profile application also removes the artificial low-frequency/high-frequency split seen in the earlier Linux replay.

A later cleanup found the first transfer probe in this section still contained exploratory post-initialize Dynamic setters. With those setters removed, the authentic no-PID-5 state (PID-4 init present, exact FL/FR static objects, unity bed, original Dolby DLLs) gives approximately:

```text
75 Hz  / 0.10 : tail peak ~0.1708
75 Hz  / 0.25 : tail peak ~0.4270
75 Hz  / 0.50 : tail peak ~0.8540
75 Hz  / 0.70 : tail peak ~0.999869
997 Hz / 0.25 : tail peak ~0.4273
```

The clean result is still broadband and, importantly, reproduces the original Dolby near-full-scale ceiling. The remaining low/mid-level magnitude gap is larger than the earlier impure probe suggested.

The Windows pre-VLLDP oracle is approximately:

```text
75 Hz  : ~0.528
997 Hz : ~0.5229
```

So the original Dolby no-runtime-profile state is correctly **broadband**, matching the qualitative Windows oracle behavior. Exact Windows stimulus history (three seconds silence followed by nine seconds tone) leaves the 0.25 transfer essentially unchanged at ~0.427 for both 75 Hz and 997 Hz, so simple warm-up/history is ruled out. The remaining discrepancy is a broadband magnitude/staging or resolved-state problem, not a missing frequency-selective profile/EQ stage.

## Important correction to earlier interpretation

The preserved local DAHP PID-5 blob is a valid Spatial/Movie-like runtime property capture, but applying it during the Linux-hosted ConfigureEncoder path does **not** reproduce the effective state that generated the Aug-8 oracle PCM.

The frozen Windows dump proves a later asynchronous DAX update was pending. The timing closure above proves requested fields in that dump must not be treated as the state used by the preceding audio block.

Do not use the later Dynamic requested values as the oracle DSP recipe.

## Exact next step

Replay the actual Windows stimulus history against the constructor/default state:

1. 48 kHz / 256-frame ASAR blocks;
2. three seconds of silence before tone onset;
3. steady tone for the same capture interval;
4. no PID-5 runtime profile commit before the measured block;
5. compare 75-Hz and 997-Hz steady transfer against the ~0.528 / ~0.5229 oracle.

If the remaining magnitude does not close through authentic history, compare the constructor/default DAP-VR core and HRTF object engine against Windows before introducing any new parameter hypothesis.


## PID-4 init payload closed byte-for-byte

The frozen Windows DAP-VR wrapper retains its raw init-time payload at wrapper `+0x80` with the byte count at `+0x88`. Both the 75-Hz and 997-Hz steady dumps retain the same 13-byte payload:

```text
01 00 01 00 01 00 00 00 00 00 00 00 00
```

SHA-256:

`b6be9fd91f09bab641f99db05d02b8f19fd93f64e5c0e6c425048c0357c5b168`

This is byte-for-byte identical to the local DAHP PID-4 evidence blob used by the Linux-hosted original-DLL harness. Therefore the remaining parity gap is **not** caused by using the wrong PID-4 init bytes.

Removing PID-4 entirely causes HRTF/DAP-VR initialization to fail (`0x80004005`), so a valid init-time payload is required. The Windows-only `IAudioDeviceModulesManager` is retained by `CDolbyAudioProcessingModule::Initialize`, but no reads of the retained manager pointer were found in `ConfigureEncoder` or `EncodeAudioData`; the ASAR shared-memory reader is initialized independently. This lowers the manager/CAPX path as the current audio-magnitude suspect.

## Object gain chain closed

`DolbyHrtfEnc!SetAudioObject` reads the static object type, flags and descriptor gain, then multiplies by an internal per-object factor at record `+0xC4`, the API scale and Dolby's internal `0.707945764` factor. In both Windows steady dumps the FL and FR records have descriptor gain `1.0`, internal factor `1.0`, static types `2` and `4`, and no positional mode. The Linux-hosted original DLL reports the same values. The remaining broadband lift is therefore not an unmodeled FL/FR object gain field.

## Post-a099b75 closures

### Media Intelligence lane is not the missing broadband lift

A controlled original-DLL Linux A/B nulled the live AIDE/OAR lane after successful HRTF/DAP initialization, leaving the same FL/FR static-object input and unity bed. The resulting transfer was bit-for-bit unchanged from the clean no-PID-5 baseline:

```text
75 Hz  / 0.10 : tail peak ~0.170807
75 Hz  / 0.25 : tail peak ~0.427018
75 Hz  / 0.50 : tail peak ~0.854035
997 Hz / 0.25 : tail peak ~0.427293
```

Therefore the missing ~0.427 -> ~0.523 broadband lift is not produced by the OAR/AIDE Media Intelligence lane in this stereo path.

### Microsoft ASAR adds no post-HRTF gain

The preserved public-symbol/disassembly closure remains valid: `ASAR::MainPluginRenderer::Process` delegates the channel bed through `MixChannelBed` and the final block through `IAsarEncoder2::Process`. At 48 kHz -> 48 kHz the bed scale remains unity. No Microsoft sample-domain gain stage exists between Dolby HRTF output and the ASAR output buffer.

Together with the proved HRTF object-gain chain, the remaining broadband lift must be created inside the original Dolby encoded/object-processing state or by the exact scheduling/state history feeding that state, not by an outer ASAR scalar.

### Live HRTF block-domain state pinned

The live Windows HRTF engine in both the 75-Hz and 997-Hz steady dumps retains the same structural values:

```text
engine +0x5C58 = 480   // outer/live input-domain frames
engine +0x5C5C = 256   // inner Dolby processing quantum
engine +0x2C8C8 = 448  // retained carry/fill state at dump time
engine +0x2C8D8 = 2    // input channels
engine +0x2C8DC = 2    // output channels
```

This does **not** invalidate the recovered `IAsarEncoder2::Initialize(256, 48000, ...)` contract. Instead it proves two simultaneous domains: a 480-frame Windows Audio/ASAR host cadence around a 256-frame Dolby inner quantum, with persistent carry state.

The frozen object/encoded staging buffers themselves are cleared after consumption, so the last object PCM cannot be recovered directly from the dumps. Their retained scheduler metadata is still sufficient to establish the 480 -> 256 adaptation boundary.

### Remaining target

The strongest remaining structural difference is now the host scheduling history rather than a missing tuning scalar. The standalone Linux harness currently submits successive 256-frame object/bed blocks directly, while the live Windows path presents 480-frame ASAR host work around the 256-frame Dolby engine and retains carry state.

Next experiment: reproduce the exact 480-frame host cadence around the original 256-frame HRTF/DAP engine, preserving the same carry/accumulator semantics, then rerun the five-point pre-VLLDP oracle. Do not fit an external gain or clamp.

## Quantum grouping and AudioEng Process ABI closed

A direct capacity sweep against the original HRTF encoder proves that its callable `Process` boundary is quantized in whole 256-frame engine blocks. Repeated 256- and 512-frame calls are valid; arbitrary 128/192/224/288/320/384/448/480-frame calls are not a stable consecutive-call contract. In particular, a direct 480-frame experiment can complete the first call but fails on the next call, so the live `480`/`448` state seen inside HRTF must not be interpreted as AudioEng handing arbitrary 480-frame blocks directly to `IAsarEncoder2`.

The exact Windows `AudioEng.dll` wrapper was then read directly from the frozen dump at `AsarEncoderWrapper<IAsarEncoder2>::Process` RVA `0x13AB0`. Its ARM64 code performs:

```text
ldr x0, [x0,#8]
mov w1, w2
mov x2, x3
mov x3, x4
... call IAsarEncoder2 vtable +0x38
```

Therefore the public wrapper's `(frames, byte_capacity, output, produced)` signature deliberately discards `frames` and shifts the remaining arguments before entering Dolby. The Linux harness's direct Dolby call `(byte_capacity, output, produced)` is ABI-correct.

An explicit no-PID-5 A/B also compares equal-duration 256-frame and 512-frame Dolby calls. Their steady transfer converges to float-noise equivalence:

```text
                 256-frame             512-frame
75 Hz / 0.10     ~0.17080705           ~0.17080706
75 Hz / 0.25     ~0.42701766           ~0.42701766
75 Hz / 0.50     ~0.85403532           ~0.85403532
75 Hz / 0.70     ~0.99986947           ~0.99986947
997 Hz / 0.25    ~0.42729273           ~0.42729670
```

Thus whole-quantum grouping does not create the missing Windows low/mid-level lift. The 480->256 scheduler is real structural state, but it is not the present amplitude root cause.

## Effective output mode also matches

The original DAP-VR output-mode setter `FUN_180046DD0` writes requested state at:

```text
+0x118 requested processing mode
+0x120 requested output-channel count
+0x128 requested matrix-present flag
+0x130... requested matrix
+0x13B0 global dirty flag
```

The commit pass `FUN_1800484F0` proves the corresponding current/effective fields are:

```text
+0x11C effective processing mode
+0x124 effective output-channel count
+0x12C effective matrix-present flag
```

Both frozen Windows oracle dumps have:

```text
requested: mode=11, channels=2, matrix=present, dirty=1
effective: mode=1,  channels=0, matrix=absent
```

The clean Linux no-PID-5 engine has and retains:

```text
requested: mode=1, channels=0, matrix=absent, dirty=0
effective: mode=1, channels=0, matrix=absent
```

So the Windows mode-11 matrix is another part of the later asynchronous DAX update waiting for the next `Prepare`; it did **not** generate the retained oracle PCM. The effective output mode used by the preceding Windows block is mode 1, matching Linux. Do not apply the pending mode-11 state as an oracle recipe.

## HRTF Initialize contract correction: Windows passes 480, inner Dolby quantum remains 256

A direct decompilation of the original `DolbyHrtfEnc.dll` `IAsarEncoder2::Initialize` path and comparison against both frozen Windows HRTF engine instances correct an earlier interpretation of the first Initialize argument.

The live Windows engine state in both the 75-Hz and 997-Hz oracle dumps is:

```text
engine +0x5C58 = 480   // input sample count per object supplied at Initialize
engine +0x5C5C = 256   // normalized inner Dolby processing quantum
engine +0x64   = 8     // derived ConfigureEncoder value for a non-integral 480/256 relation
```

The original engine constructor stores the first explicit Initialize argument at `+0x5C58`. It then normalizes the inner processing quantum to 256. Assembly around `0x180008B98..0x180008BC0` tests the low byte of the 480-sample outer count and writes derived value `8`; an exact 256-sample outer count instead derives `1`.

Therefore the correct Windows contract is:

```text
IAsarEncoder2::Initialize(480, 48000, ...)
outer/object host domain = 480 frames
inner Dolby engine quantum = 256 frames
```

The remaining topology is also byte/field exact between Windows and the corrected Linux-hosted original DLL:

```text
output channels             = 2
internal output channels    = 2
static object mask          = 0x6   // FL + FR
object-derived mask         = 0x3
```

There is no hidden wider static-object initialization mask in the live stereo path.

### Corrected 480/480 host contract does not close the amplitude gap

When the Linux harness is corrected to Initialize with 480 and then submit successive 480-frame FL/FR object + unity-bed host blocks, the original HRTF encoder runs stably. Its internal quantum remains 256 and derived field `+0x64` becomes the Windows value 8.

The steady no-runtime-profile transfer remains:

```text
75 Hz  / 0.10 -> ~0.170807
75 Hz  / 0.25 -> ~0.427018
75 Hz  / 0.50 -> ~0.854035
75 Hz  / 0.70 -> ~0.999869
997 Hz / 0.25 -> ~0.427297
```

So the host/inner quantum contract is now correct but is not the missing Windows ~0.523 low/mid-level lift.

## Exact Aug-8 runtime blob recovered from the frozen Windows DAP-VR wrapper

The DAP-VR wrapper caches its runtime property payload at wrapper `+0x90`, with the byte count in the low 32 bits of `+0x98`. The 75-Hz and 997-Hz oracle dumps contain the same 2641-byte payload.

Private fixture identity:

```text
bytes   2641
SHA256  92936e727fe85b0fd37bf3ef515a7496851eea311342e7e8e10f0431264c1b89
```

This is **not** the earlier lab `dahp_5.blob` (`ca80a9c4...`), so that older local blob must not be treated as the Aug-8 oracle runtime fixture.

The first 2624 bytes of the actual Windows blob differ from the regenerated OEM Dynamic serializer output in only seven bytes; its final 17 bytes are zero padding. It differs materially more from the regenerated Movie payload. Therefore the runtime update cached in both frozen oracle instances is Dynamic-family state, with a small set of live policy/stereo overrides.

Do not commit the proprietary/raw blob; retain only its hash and derived schema facts.

## Exact asynchronous runtime-race replay

The Windows dump timing model was reproduced directly on Linux using the original DLLs:

1. construct the no-runtime-profile engine with the corrected 480/480 contract;
2. process the steady tone;
3. after the final audio `Process`, deliver the exact 2641-byte Windows runtime blob through the original DAP-VR Runtime `SetParams` entry;
4. do **not** call `Prepare` or process another audio block;
5. dump the DAP-VR state.

The runtime callback succeeds while the already-produced PCM is unchanged:

```text
75 Hz  / 0.25 -> ~0.427018
997 Hz / 0.25 -> ~0.427297
```

This reproduces the proven Windows ordering:

```text
last audio block uses old effective state
        -> asynchronous Dynamic SetParams arrives
        -> requested fields + dirty bits change
        -> no following Prepare yet
```

Comparing Windows to Linux before this race left roughly 467 stable, plausibly meaningful 32-bit differences in the first 16 KiB of the processing state. Replaying the exact pending Dynamic update after audio reduces that set to roughly 241 while leaving the PCM unchanged. This is strong confirmation that a large fraction of the earlier core diff was merely the pending update footprint rather than the state that generated the oracle audio.

## Effective scalar controls used by the oracle block match the neutral Linux state

The original setter/commit code separates requested and effective fields. Re-reading those pairs after the race closure shows the controls that actually processed the retained Windows block match the no-runtime-profile Linux engine:

```text
output processing mode       effective 1     on both
output matrix                effectively absent on both
VolMax boost                 effective 0.06923077 on both
volume leveler enable        effective 0     on both
volume leveler amount        effective 7     on both
IEQ enable                   effective 0     on both
IEQ amount                   effective 0.625 on both
dialog enable/amount         effective 0 / 0 on both
surround boost               effective 0.04615385 on both
```

The Windows-visible Dynamic values such as mode 11, leveler enable/amount 1/5, IEQ enable, and VolMax 96-equivalent belong to the newer requested state and were waiting for the next commit.

The residual ~0.427 -> ~0.523 difference is therefore no longer credibly explained by one of these obvious scalar controls. The remaining target is persistent/structural algorithm state that survives while those effective controls remain equal.

## Exact HRTF -> DAP object identity is now proved

The live HRTF engine retains the actual DAP Module2 interface at `engine + 0x2C9A0`.
Following that pointer in both frozen Windows oracle dumps leads to the exact DAP-VR wrapper/core already under analysis; this is not a stray Dolby instance found by heap scanning.

```text
75-Hz dump:
HRTF engine              0x1ec9270e000
stored DAP Module2       0x1ec92687758
DAP +0x538 wrapper       0x1ec926761e0
wrapper +0x60 core       0x1ec946e0000

997-Hz dump:
HRTF engine              0x2161b722000
stored DAP Module2       0x2161b713398
DAP +0x538 wrapper       0x2161b675420
wrapper +0x60 core       0x2161d6e0000
```

On Linux the DAP-VR wrapper's `+0x50` and `+0x60` fields alias the same processing-core pointer, matching Windows. The object comparison is therefore valid end-to-end from the live HRTF encoder.

## Linked Windows DAP core remains constructor-pristine at the per-block descriptor fields

The exact HRTF-linked Windows DAP core has, in both steady dumps:

```text
core +0x80 = 0
core +0x84 = 288
core +0x88 = 384
```

Those are the literal constants written by the DAP-VR core constructor `FUN_180046020`:

```text
+0x80/+0x84 <- 0x12000000000  // 0, 288 in the relevant 32-bit views
+0x88       <- 0x180          // 384
```

No second writer of that exact tuple was found. The normal DAP-VR `Prepare` path `FUN_180046FA0` instead leaves Linux with the per-block descriptor state `1 / 128 / 192`; the downstream DAP-VR `Process` wrapper reads those fields but does not restore them to the constructor tuple.

This discrepancy is therefore real on the exact DAP instance bound to HRTF. Merely enabling HRTF stereo-bypass does not explain it: Linux variants with bypass disabled or enabled all execute `Prepare` and retain `1 / 128 / 192` after processing.

## Dolby final Initialize dependency identified: Spatial Audio License Server runtime parameters

The final explicit Dolby `IAsarEncoder2::Initialize` argument was previously passed as `NULL` by the Linux harness. Microsoft public symbols and the exact frozen `AudioEng.dll` call setup now identify its source.

`ASAR::MainPluginRenderer::Initialize` calls:

```cpp
ASAR::MainPluginRenderer::GetSpatialAudioLicenseServerRuntimeParameters(
    IUnknown *,
    APOInitSystemEffects3 const *,
    GUID const &,
    PCWSTR,
    IUnknown **out);
```

The returned `IUnknown*` is the value forwarded as wrapper argument 13 and therefore as Dolby Initialize argument 8. This is distinct from `MainPluginRenderer::Initialize`'s own incoming final `IUnknown*`, which is forwarded through the separate HRTF dependency interface (`98f37dac...`).

Thus the Linux harness is still missing a genuine Windows object: the **Spatial Audio License Server runtime-parameters object**.

## DAP Module2 consumes that object during ConfigureEncoder

The live DAP Module2 interface vtable maps slot 4 to `DolbyAudioProcessing.dll` RVA `0x17DF0`, `CDolbyAudioProcessingModule::ConfigureEncoder`.

That function forwards its final argument into `ApplyModuleParams` twice for DAP-VR:

```text
ApplyModuleParams(DAP_VR, Inittime, runtime_params_object)
ApplyModuleParams(DAP_VR, Runtime,  runtime_params_object)
```

Decompilation of `ApplyModuleParams` (`FUN_1800175B8`) proves this object is an actual parameter source, not a logging or license-check ornament. For DAP-VR it queries the object for interface GUID `DAT_180324678`, then asks that interface for named runtime parameters:

```text
dahp.v2.1.1.init_time_params
dahp.v2.1.1.run_time_params
```

The returned parameter object exposes a method at vtable `+0xD0` which yields a raw byte buffer and length; those bytes are passed directly into the original DAP-VR `SetParams` implementation and then freed with `CoTaskMemFree`.

This is now the highest-value unclosed Windows/Linux initialization difference. The next step is to resolve the queried interface GUID and Microsoft provider object, recover what parameter source Windows supplies, and emulate only the actually used contract on Linux before drawing further conclusions from DAP-VR state.

## Spatial Audio License Server runtime-map contract executed on Linux

The missing final HRTF Initialize dependency has now been exercised through the original Dolby code rather than approximated after initialization.

Microsoft public symbols identify the provider family as `ISpatialAudioLicenseServer` and the interactive-user class as:

```text
CLSID_SpatialAudioLicenseServerInteractiveUser
354ff91b-5e49-4bdc-a8e6-1cb6c6877182

IID used by AudioEng for ISpatialAudioLicenseServer
cdc99663-5f31-45ee-89fa-a4a0d64f6d1c
```

The object returned to Dolby is queried for WinRT `IMap<String,Object>` IID:

```text
1b0d3570-0877-5ec2-8a2c-3b9539506aca
```

The raw DAP assembly confirms a normal COM/WinRT call chain:

```text
runtime->QueryInterface(IMap<String,Object>)
map->Lookup(HSTRING key)
property_value->GetUInt8Array(...)
DAP-VR SetParams(byte_array, length, phase)
```

The exact UTF-16 key names in the shipped binary are underscore-separated, not the dotted form suggested by the decompiler labels:

```text
dahp_v2_1_1_init_time_params
dahp_v2_1_1_run_time_params
```

A small private Linux shim was built with only the WinRT pieces Dolby actually calls:

- `WindowsCreateStringReference` / raw HSTRING access;
- `IMap<String,Object>::QueryInterface` and `Lookup`;
- `IPropertyValue::GetUInt8Array` at vtable `+0xD0`;
- `CoTaskMemFree` ownership handoff.

The HRTF `ConfigureEncoder` vtable was instrumented and proves the runtime-map object arrives unchanged at the original DAP Module2 call.

### Minimal license-runtime stereo policy fixture

Using the original `DAX3API.exe` serializer, a runtime payload containing only the two already-observed stereo policy fields was generated:

```text
stereo_cp_bypass_mode = 2
stereo_bypass_dap_dll = 1
```

Private serialized fixture identity:

```text
bytes   60
SHA256  503179df262071fda176b4a1345510abc377dafa24037e56f5ea62e5fe9a582f
```

The raw fixture remains private and is not committed.

The runtime-map test supplies:

```text
init_time_params -> exact 13-byte Windows PID-4 bytes
run_time_params  -> 60-byte minimal stereo-policy payload
```

The original Dolby code successfully fetches both arrays through the WinRT map and initializes HRTF with the Windows-observed stereo bypass mode enabled.

One private diagnostic branch patch is currently required to enter the DAP-VR map fallback after the fake property-store miss; the null-object guard remains intact so AIDE's legitimate null runtime object still follows its normal path. This gate discrepancy is a harness/control-state issue still under investigation and must not be promoted to production behavior.

### Minimal license policy is not the missing oracle transfer

With the corrected 480-frame HRTF host contract and the license-server map active, the original DLLs produce approximately:

```text
75 Hz  / 0.10 -> 0.152766
75 Hz  / 0.25 -> 0.381914
75 Hz  / 0.50 -> 0.763828
75 Hz  / 0.70 -> 1.069359
997 Hz / 0.25 -> 0.270804
```

This is clearly not the Windows pre-VLLDP oracle (`~0.320 / ~0.528 / ~1.0 / ~1.0 / ~0.523`). Therefore the real Windows Spatial Audio License Server runtime map contains additional state beyond the two stereo-bypass fields, or an equivalent earlier policy transaction establishes additional DAP state before the frozen oracle block.

Do not fit the remaining difference. The next task is to recover or reconstruct the **actual initial license-server runtime byte array** from preserved Windows/OEM spatial-policy evidence and feed it through this now-working map boundary.

## License-runtime candidate comparison at the authentic ConfigureEncoder boundary

The SP11 OEM Microsoft Atmos operator policy independently constrains the expected license-runtime state family:

```text
internal_speaker + spatial_audio=false -> default profile Music
internal_speaker + spatial_audio=true  -> default profile Movie
bypass_stereo_virtualizer             -> true for all listed profiles
full_dsp_support                       -> false
audio_director_mode                    -> redirected
dahp_auto_enablement                   -> true
```

Source identity is the preserved Surface SP11 `operator_settings_msft_atmos.json`; no vendor payload is copied into Git.

Four private runtime payload candidates were then supplied **through the working Spatial Audio License Server IMap boundary during original Dolby ConfigureEncoder**, using the exact 13-byte init payload and corrected 480-frame HRTF contract. The only private diagnostic remains the narrowly scoped DAP-VR fallback-gate patch documented above.

### 1. Preserved older Movie-family / spatial-policy blob

```text
bytes   2641
SHA256  ca80a9c4106b694cb741aa9033074acbd14d8dc7595108d49e0c9af23f606fcd
HRTF stereo bypass = 1
```

Two-second steady results:

```text
75 Hz  / 0.25  last ~0.577453   max ~0.649368
997 Hz / 0.25  last ~0.413158   max ~0.500921
```

This is substantially closer to the Aug-8 oracle than the minimal two-field stereo-policy map, especially because the 997-Hz transient reaches ~0.501 before decaying.

### 2. Regenerated OEM Movie payload

```text
bytes   2624
SHA256  b4c72f73e3e874e7c709f620504f269cff2f4fd377c490d57bb77a4284553088
HRTF stereo bypass = 0 in this serialized candidate
```

```text
75 Hz  / 0.25  last ~0.969796   max ~0.999869
997 Hz / 0.25  last ~0.959614   max ~0.961940
```

### 3. Regenerated OEM Dynamic payload

```text
bytes   2624
SHA256  35bce8df538d1932da548f494ea74f974b5552059f8fe13adf58f89f2aaf3a99
HRTF stereo bypass = 0 in this serialized candidate
```

```text
75 Hz  / 0.25  last ~0.981941   max ~0.999869
997 Hz / 0.25  last ~0.984895   max ~0.999869
```

### 4. Exact later Dynamic blob cached in the Aug-8 Windows dump

```text
bytes   2641
SHA256  92936e727fe85b0fd37bf3ef515a7496851eea311342e7e8e10f0431264c1b89
HRTF stereo bypass = 1
```

```text
75 Hz  / 0.25  last ~0.593719   max ~0.727569
997 Hz / 0.25  last ~0.432413   max ~0.618467
```

None is yet the Windows steady oracle (`~0.528` / `~0.523`), so no candidate is promoted as the recovered license-server runtime array.

The useful narrowing is that the **older Movie-family spatial-policy state is the closest authentic-boundary candidate so far** and agrees with the independent OEM rule that internal-speaker Spatial ON defaults to Movie. Its 997-Hz path also shows history dependence: it passes near the Windows target before settling lower. The next test therefore uses the exact Windows probe history rather than another static tuning guess: 3 seconds silence followed by 9 seconds of tone at 48 kHz / 480-frame host blocks.

## Post-0eb0ff0 closure: Movie-family history rejected; OEM ASAR writer located

The closest preserved Movie-family runtime candidate was rerun with the same host-history shape as the frozen Windows oracle: 1200 x 480-frame calls at 48 kHz, with the first 300 blocks (3 s) silent and the following 900 blocks (9 s) carrying the test tone.

Steady results after the full 12-second history were:

```text
75 Hz / 0.10  last ~0.301042
75 Hz / 0.25  last ~0.584278
75 Hz / 0.50  last ~1.115344
75 Hz / 0.70  last ~1.521108
997 Hz / 0.25 last ~0.400485
```

These do not reproduce the Windows pre-VLLDP oracle (`~0.320 / ~0.528 / ~1.0 / ~1.0 / ~0.523`). The earlier transient near 0.50 was therefore not the steady Windows state.

A second diagnostic reproduced the Aug-8 race shape: start from that Movie-family license state, process the 3 s silence + 9 s tone, deliver the exact later Aug-8 Dynamic runtime blob after the final audio block, then dump DAP-VR memory without another Prepare. This made the stable core-state comparison worse: roughly 362 meaningful stable 32-bit differences versus roughly 241 from the neutral-start race replay. This rejects the preserved Movie-family runtime blob as the hidden Aug-8 initial state despite the independent OEM default-profile rule.

The provider registration was also recovered from the preserved Windows registry:

```text
CLSID {354ff91b-5e49-4bdc-a8e6-1cb6c6877182}
SpatialAudioLicenseServerInteractiveUser
C:\Windows\System32\SpatialAudioLicenseSrv.exe SpatialAudioLicenseServerInteractiveUser
```

The preserved Surface Dolby OEM client then exposed the more important implementation trail. `DolbyAccessOEM.dll` contains explicit native/.NET-Native metadata strings for:

```text
WriteAsarInitParametersForAllEndpoints
WriteAsarInitParametersForEndpoint
WriteAsarRuntimeParametersForAllEndpoints
WriteAllAsarParametersForEndpoint
WriteAllAsarParametersForAllEndpoints
GetBaseInitTimeParams
_movieRunTimeParams
_musicRunTimeParams
_dynamicRunTimeParams
```

and runtime-parameter names for headphone, home-theater, and built-in-speaker Spatial Audio paths. This is direct evidence that the Dolby OEM service writes dedicated **ASAR runtime parameters** into the Windows property/runtime-parameter path; they are not necessarily identical to the ordinary full DAX profile blobs.

The exact OEM binary used for this evidence is private and not published. Public identity only:

```text
DolbyAccessOEM.dll SHA256 be4fce2a9cc2b24849ba6b2f8558f2c7d652df543eba7da882f58c59ae88ef19
ARM64 PE32+
CodeView PDB: DolbyAccessOEM.pdb
PDB GUID: 9B8926F2-367B-4B84-BDA0-A3F5E0C977F0
PDB age: 1
```

The matching OEM PDB is not present on Microsoft's public symbol server (404 for the exact CodeView identity), so the continuation must recover the writer from the preserved native image.

Unique UTF-16 anchor recovered in `DolbyAccessOEM.dll`:

```text
"WriteAsarRuntimeParametersForAllEndpoints dahpKey:{0}, dabsKey:{1} isMonitor:{2} audioendpoint{3}"
file offset 0x1b394fc
RVA         0x1b3b6fc
VA          0x181b3b6fc
```

Additional unique anchors:

```text
"Runtime parameters for ASAR written to Property Store for endpoint "
RVA 0x1b15484

"Writing runtime parameters for ASAR to Property Store for all endpoints"
RVA 0x1b3beec
```

Full serialized candidate blobs are not embedded verbatim in the OEM DLL; the ASAR payload is built at runtime. Therefore the next engineering step is to use these string anchors to recover the native ARM64 writer/state-machine path and determine exactly how the built-in-speaker ASAR runtime byte array is constructed. Feed that recovered array through the already-working Linux Spatial Audio License Server `IMap<String,Object>` harness rather than curve-fitting DAP controls.

## Native DolbyAccessOEM ASAR writer localized

The preserved `DolbyAccessOEM.dll` is a native ARM64 .NET-Native image. PE section-name truncation matters: the managed native code section is `.textMan`, not `.textManaged`. Direct ARM64 disassembly of that section resolved the previously found managed string objects into executable xrefs.

The async state-machine body for the all-endpoint ASAR runtime writer begins at:

```text
0x180581738
```

The exact method-name/log anchors are referenced inside that function:

```text
0x1805817A8 -> "WriteAsarRuntimeParametersForAllEndpoints"
0x1805817BC -> "Writing runtime parameters for ASAR to Property Store for all endpoints"
0x180581D48 -> "Runtime parameters for ASAR written to Property Store for endpoint "
0x180581D6C -> "WriteAsarRuntimeParametersForAllEndpoints dahpKey:{0}, dabsKey:{1} isMonitor:{2} audioendpoint{3}"
```

The same async state machine repeats its post-await logging material around `0x180581FC4`, `0x180582258`, and `0x18058240C`; these are continuation states, not separate writer implementations.

Most important, the frozen managed strings loaded by the writer explicitly distinguish four runtime objects:

```text
dahpRuntimeParams
 dabsRuntimeParams
 dafm dahpVlldpRuntimeParams
 dafm dabsVlldpRuntimeParams
```

and the combined init+runtime writer similarly distinguishes:

```text
dahpRuntimeParams
 dahpInitParams
 dabsRuntimeParams
 dabsInitParams
 dafm dahpVlldpRuntimeParams
 dafm dahpVlldpInitParams
 dafm dabsVlldpRuntimeParams
 dafm dabsVlldpInitParams
```

This proves that the ASAR DAHP runtime payload consumed through the Spatial Audio runtime-parameter service is **not the same object as the downstream DAFM/VLLDP runtime payload**. Treating the normal VLLDP/DAX profile blob as the ASAR license-server payload is therefore structurally wrong unless an explicit equality is later proved.

A second native cluster around `0x18057C000..0x18057E4xx` is the corresponding `WriteAllAsarParametersForAllEndpoints` / per-endpoint path. It contains the same DAHP/DABS/DAFM object split and is the current upstream lead for recovering the producer of `dahpRuntimeParams`.

Next: trace where the state-machine fields holding `dahpRuntimeParams` are populated before the per-endpoint property-store loop, then recover the parameter-builder/profile object that supplies those bytes.

## OEM DAHP ASAR producer and base-profile factory table recovered

Ghidra analysis of the preserved OEM native image resolved the async wrapper/caller chain around the ASAR writer.

The combined per-endpoint writer state machine `FUN_18057daf8` is started by `FUN_18046a810`. The wrapper itself receives only two top-level objects; `dahpRuntimeParams` is **not** a direct wrapper argument. It is produced inside the async body.

Inside `FUN_18057daf8`, the DAHP path awaits a service operation, then `FUN_1808a4c30` unpacks the returned four-item bundle as:

```text
state byte +0x38  DAHP init params
state byte +0x40  DAFM/DAHP VLLDP init params
state byte +0x48  DAHP runtime params
state byte +0x50  DAFM/DAHP VLLDP runtime params
```

A parallel four-item bundle supplies DABS and DAFM/DABS objects at `+0x58..+0x70`.

The pair writer `FUN_18046be58(parent, endpoint, key, initObj, runtimeObj)` does not construct the parameter contents. It converts the already-created init/runtime objects into the endpoint/property-store representation, batches the non-null values, and writes them. Therefore the real payload producer is the DAHP configuration service upstream of this function.

Frozen OEM strings and native xrefs identify that producer family as:

```text
Dolby.Common\AudioConfiguration\Managers\DahpConfigurationParameters.cs
GetBaseInitTimeParams
GetBaseRunTimeParams
Base DAHP init-time parameters for {0} profile requested
Base DAHP runtime parameters for {0} profile requested
```

Native functions:

```text
FUN_18044b778(profile)  base DAHP init-time parameters
FUN_18044b8fc(profile)  base DAHP runtime parameters
```

`FUN_18044b8fc` performs a lookup in a static profile->factory dictionary and invokes the selected factory delegate. Its static constructor is `FUN_18044bba8`, which installs the following mapping:

```text
profile 5      -> FUN_180448f20
profile 0      -> FUN_1804495c8
profile 1      -> FUN_180449f00
profile 2      -> FUN_18044a500
profile 3      -> FUN_18044aba8
profile 0x2c   -> FUN_18044b190
profile 0x2d   -> FUN_18044b190
profile 0x2e   -> FUN_18044b190
```

Profile ID 5 is positively identified as the SP11 **Dynamic** base DAHP runtime factory. `FUN_180448f20` creates a dedicated DAHP parameter object whose constants match the recovered SP11 Dynamic settings, including:

```text
volume-leveler amount          5
volume-leveler in target      -320
volume-leveler out target     -320
regulator relaxation           96
regulator timbre preservation  12
surround boost                 96
bass cutoff                   200
bass width                     16
regulator enable                1
```

along with the corresponding steering/enable flags and other profile fields.

This is important because the OEM ASAR service starts from a dedicated `DahpConfigurationParameters` object and only later serializes/adjusts it. The previously generated "full Dynamic" DAX blob was assembled from the normal DAX transaction surface and included large nested tables. It must not be assumed byte-identical to the ASAR service serialization merely because the visible scalar tuning values match.

The OEM runtime adjustment pipeline is also now localized by exact native log xrefs:

```text
FUN_180484128  -> "DAX RPC params loaded"
FUN_180487c98  -> bypass mode disabled / skip virtualization
FUN_180489e98  -> adjusted for current profile settings
FUN_180488150  -> stereo processing bypass disabled
FUN_18048ae1c  -> test params loaded
```

Next: recover the serialization of the `DahpConfigurationParameters` object (especially Dynamic ID 5), determine the exact sparse ASAR byte array after the applicable runtime adjustments, and feed that exact array through the already-working Linux Spatial Audio License Server map.

## Exact sparse OEM ASAR Dynamic serialization and Aug-8 Dynamic delta decoded

The recovered `FUN_180448f20` Dynamic `DahpConfigurationParameters` object maps one-for-one onto the private DAX3API serializer state layout. Reconstructing exactly that object — preserving the distinction between nullable-zero and truly absent nested objects — produces a **348-byte** DAHP runtime payload:

```text
bytes   348
SHA256  7f4bd94703ca47d774925fa9749ce3242160f16bf5550351629463acbee4ddad
```

This is dramatically smaller than the previously generated 2624-byte normal/full Dynamic DAX transaction. The real base ASAR Dynamic object does not contain the output matrix, process-optimizer table, audio-optimizer table, regulator tuning table, or GEQ table that were present in the earlier full-profile experiment.

Private Linux license-map replay of this 348-byte payload through the original Windows DLLs succeeds end-to-end (`IMap -> IPropertyValue -> GetUInt8Array -> DAP SetParams`) but is not the final Windows state:

```text
HRTF StereoBypassMode = 0
75 Hz  / 0.25 -> ~0.921603
997 Hz / 0.25 -> ~0.649384
```

Therefore at least one OEM runtime adjustment follows the base profile factory before the Windows license-server payload is finalized.

The exact 2641-byte Aug-8 pending Dynamic blob was then compared against controlled DAX3API serializations. Its previously unexplained seven-byte delta from the regenerated normal Dynamic transaction is now fully decoded semantically.

Controlled serializer experiments prove:

```text
serialized offset 0x3FB -> mi2dialog_enhancer_steering_enable = false
serialized tail          -> stereo_cp_bypass_mode = 2
                            stereo_bypass_dap_dll = true
                            volume_leveler_drc_enable = unset/absent
```

Using normal Dynamic state plus:

```text
MI->dialog steering  false
stereo CP bypass     2
stereo DAP-DLL bypass true
leveler DRC          unset
```

produces a 2628-byte meaningful serialization that matches the first 2628 bytes of the 2641-byte Windows blob **byte-for-byte**. The remaining 13 Windows bytes are all zero padding.

This also explains the frozen Windows HRTF `StereoBypassMode=1`: the later pending Dynamic array contains the authentic stereo-policy combination `CP mode 2 + DAP-DLL bypass true`.

The relevant OEM adjustment implementations are now localized:

```text
FUN_180484128  DaxRpcParametersAdjustment
FUN_180487c98  SkipVirtualizationAdjustment
FUN_180489e98  ProfileParametersAdjustment
FUN_180488150  StereoBypassAdjustment
FUN_18048ae1c  TestParametersAdjustment
```

Important semantics from the original code:

- DAX-RPC adjustment preserves already-present stereo fields and only supplies defaults when absent;
- SkipVirtualization selects CP bypass mode 0 or 1 based on policy state;
- StereoBypassAdjustment explicitly sets CP bypass mode 2 and DAP-DLL bypass false;
- ProfileParametersAdjustment modifies current-profile fields including MI/IEQ/GEQ-related state;
- TestParametersAdjustment replaces parameters only when test parameters are configured.

The exact pending Dynamic result therefore comes from the broader OEM adjustment pipeline, not merely one scalar stereo setter.

### Important correction to the earlier Movie negative

The earlier Movie-family negative replay used a normal/full DAX runtime blob, not the newly recovered sparse `DahpConfigurationParameters` ASAR representation. It rejects that old full-profile fixture as the hidden Aug-8 initial state, but it does **not** reject the authentic OEM ASAR Movie object. Because SP11 operator policy states `internal_speaker + spatial_audio=true -> default profile Movie`, the real sparse/adjusted ASAR Movie path must now be reconstructed and tested before Movie can be closed.

## OEM-adjusted Movie serialization proved; Movie rejection is now legitimate

The base-profile factory mapping can be identified by its control fingerprints as:

```text
profile 0  Movie
profile 1  Music
profile 2  Game
profile 3  Voice
profile 5  Dynamic
0x2c..2e   custom family
```

Direct decompile diff between the sparse Movie factory `FUN_1804495c8` and sparse Dynamic `FUN_180448f20` shows Movie changes only five booleans from the base Dynamic object:

```text
MI->IEQ steering                 off
MI->DV-leveler steering          off
MI->surround-compressor steering off
IEQ enable                       off
regulator enable                 off
```

The exact sparse Movie object serializes to:

```text
bytes   348
SHA256  3cdc60a6dbc7a04f6a26c8a48334f0f9c68e4fde764a9c2e1c0804a2e28b7214
```

Private original-DLL license-map replay succeeds but is still not the Windows oracle:

```text
HRTF StereoBypassMode = 0
75 Hz  / 0.25 -> ~0.870967
997 Hz / 0.25 -> ~0.608100
```

The preserved 2641-byte `ca80a9c4...` Movie-family blob was then compared to a regenerated normal Movie DAX serialization. Its seven meaningful byte differences have now been decoded exactly:

```text
serialized offset 0x4C3 -> dialog enhancer enable = false
serialized tail          -> stereo CP bypass mode = 2
                            stereo DAP-DLL bypass = true
                            volume-leveler DRC = unset
```

Regenerating normal Movie with precisely those four policy changes produces a 2628-byte payload:

```text
SHA256 d0266e59b86a32a702b23b1e712bc4d5c3850976092d1295d0acf60b80d53a6f
```

Its **entire 2628-byte meaningful prefix matches `ca80a9c4...` byte-for-byte**. The remaining 13 bytes in the preserved 2641-byte fixture are all zero padding.

Therefore the earlier `ca80a9c4...` replay is now proved to represent the fully OEM-adjusted Movie-family DAHP runtime serialization, not an arbitrary generic Movie fixture. Its prior 3-second-silence + 9-second-tone result:

```text
75 Hz  / 0.25 -> ~0.584278
997 Hz / 0.25 -> ~0.400485
```

legitimately rejects the fully adjusted OEM Movie ASAR runtime state as the state that generated the Aug-8 Windows pre-VLLDP oracle (`~0.528 / ~0.523`).

The important remaining question is therefore which runtime state actually preceded the later pending Dynamic update; this should be resolved from the Aug-8 profile/update chronology or by testing only the remaining chronology-supported OEM factories, not by broad parameter fitting.
