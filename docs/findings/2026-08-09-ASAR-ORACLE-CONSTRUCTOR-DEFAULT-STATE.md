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
