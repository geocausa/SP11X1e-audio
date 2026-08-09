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
