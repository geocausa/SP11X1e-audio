# ASAR DAX lifecycle and core-state closure — 2026-08-09

## Scope

This checkpoint continues the SP11 original-Dolby ASAR reproduction work on the isolated Linux host. It uses hash-pinned private vendor binaries and private dumps locally, but publishes no vendor binary, dump, waveform, PDB, or captured property blob.

## DAX ASAR runtime transport is reproduced

The DAX producer path is now recovered end-to-end:

`DAX3API AsarParamsController::Update`
→ `AsarParamsResolver`
→ packed presence/value serializer
→ shared-memory writer
→ `DolbyAudioProcessing!AsarParamsReaderController`
→ DAP-VR `SetParams`.

DAX3API frames ASAR messages as five 32-bit words followed by the payload:

`8, 0, 2, 0, payload_size, payload...`

The payload serializer is a fixed presence/value schema. A complete SP11 Dynamic tuning-cp payload generated with the original DAX3API serializer is 2624 bytes and is accepted by the original Dolby DAP-VR parser.

The accompanying research harness `dolby-port/linux-harness/sp11_hrtf_dax_runtime_fullpayload_curve.c` generates the nested IEQ/GEQ/optimizer/regulator/output-mode structures and sends them through the original parser. Private DLLs and local evidence blobs are not part of the repository.

## Important lifecycle correction

A prior exploratory experiment called DAP-VR `SetParams(..., Inittime)` manually *after* the HRTF/DAP engine had already been created. It produced an interesting transfer change, but this is not equivalent to Windows initialization and must not be treated as parity evidence.

`CDolbyAudioProcessingModule::ConfigureEncoder` proves the authentic order:

1. create DAP-VR;
2. apply DAP-VR init-time module params;
3. validate the module is initialized/ready;
4. apply DAP-VR runtime module params.

The active SP11 property-key selection on this path is the DAHP family `{1b4dab55-b1fb-4d8c-8317-f2d4a96efbb8}`:

- PID 4 = DAP-VR init-time;
- PID 5 = DAP-VR runtime.

The local evidence bundle already supplies these during the real ConfigureEncoder sequence. Replacing the authentic small PID-4 init blob with the later 2624-byte ASAR payload before HRTF initialization produced no transfer change. Therefore the post-init `Inittime` result is retired as a lifecycle-misuse experiment.

## PID-5 state is Spatial/Movie-like, but Windows final core is later overridden

The preserved PID-5 runtime blob is 2641 bytes. A regenerated OEM Movie/Spatial payload is 2624 bytes and matches its meaningful schema almost completely. The remaining semantic differences are accounted for by live stereo policy state:

- Movie disables MI→adaptive-virtualizer steering;
- `stereo_cp_bypass_mode = 2` is explicitly present in the preserved live blob;
- `stereo_bypass_dap_dll = 1` is explicitly present;
- the final 13 bytes are zero padding.

This closes the earlier ambiguity that the preserved runtime blob was a generic Dynamic XML dump: it reflects the Spatial/Movie policy family.

However, the frozen Aug-8 Windows DAP-VR core contains a later Dynamic-like requested state (MI steering enabled, dialog amount 5, leveler amount 5, IEQ enabled/amount 10, etc.). Therefore a live update occurs after the property-store Movie-like state. This is consistent with the recovered DAX-RPC/shared-memory update architecture.

## VirtualSurround routing/metadata is closed

The exact Microsoft `VirtualSurroundApo.pdb` for the binary was recovered from the public symbol server and used only as local evidence.

The Microsoft symbols and code close the steady stereo object contract:

- `CASARSampleBuffer` maps FL/FR to static object types `0x2` and `0x4`;
- `CBaseASARClientAPO::UpdateSpatialObjectProperties` writes flags, frame count, and unity gain;
- the VirtualSurround override adds positional data only for the position-update flag, which is not present on the steady FL/FR path;
- `WriteSamplesToASARBuffer` performs the already-proven plain stereo deinterleave.

`DolbyHrtfEnc!SetAudioObject` confirms static FL/FR consume the object type, flags, and gain fields. The external object-slot argument is not used to key static FL/FR objects. The `ISpatialAudioPositionMapper` branch is only relevant to positional/non-static objects and cannot explain the current steady stereo mismatch.

## AIDE/OAR is real but its stable setup already matches Windows

The DAP encode path explicitly runs OAR and AIDE before DAP-VR processing. This is a real Media Intelligence lane, not a naming-only feature.

The current fake property store returns `NAME NOT FOUND` for the normal AIDE property key `{bd4e102e-f1d9-481d-9261-fdd83364d731}, PID 1`. Historical real-Windows ETW evidence also recorded this key as `NAME NOT FOUND` on SP11.

Stable AIDE state read from both Aug-8 Windows steady dumps matches the original-DLL Linux host field-for-field for the inspected controls: enabled state, 12-channel configuration, 960-frame block size, status, mode, and preset. The missing AIDE property is therefore not promoted as the current root cause.

## Authentic transfer status

Using the original vendor HRTF + DAP engine, the exact FL/FR object contract, unity stereo bed, and the untouched preserved Spatial/Movie runtime state gives approximately:

- 75 Hz / 0.10: 0.2862
- 75 Hz / 0.25: 0.5249
- 75 Hz / 0.50: 1.0178
- 75 Hz / 0.70: 1.3868
- 997 Hz / 0.25: 0.2959

The corresponding Windows pre-VLLDP oracle is approximately:

- 75 Hz / 0.10: 0.320
- 75 Hz / 0.25: 0.528
- 75 Hz / 0.50: 0.99987
- 75 Hz / 0.70: 0.99987
- 997 Hz / 0.25: 0.5229

The 75-Hz mid-level point is already very close. The major unresolved problem remains the broadband/997-Hz object contribution and the Windows near-full-scale staging ceiling.

## DAP-VR requested vs effective state

A full 0x4000-byte DAP-VR core comparison was made locally between the two Windows steady dumps and the original-DLL Linux host. Windows 75-Hz and 997-Hz dumps have the same stable control values at the mapped locations, while Linux 75-Hz and 997-Hz likewise agree with each other. The important difference is Windows-vs-Linux state, not tone-dependent corruption.

`FUN_1800484F0` is now identified as the DAP-VR commit/update pass. It:

- copies requested controls into current/effective controls;
- commits the volmax/sub-control block;
- rebuilds IEQ state when its dirty flag is set;
- commits GEQ and regulator structures;
- clears the global dirty flag at `+0x13B0`.

The Windows snapshot contains requested Dynamic-like fields together with a distinct previously effective state. One especially clear regulator example is:

- regulator dirty flag `+0xF04 = 1`;
- active timbre scale `+0xF08 = 1.0`;
- active timbre raw `+0xF0C = 16`;
- newly requested timbre `+0xF10 = 12`.

The regulator update function proves the `16 -> 12` relation and derives the active scale from the committed raw timbre. Relaxation, overdrive, and distortion values already match; this is a specific pending timbre transition, not a wholesale regulator mismatch.

Similar requested/current pairs exist for MI steering, dialog, leveler, IEQ, and volmax. The Windows snapshot is therefore not representable as simply “use the Dynamic profile” or “use the Movie profile.” It is resolved runtime state around a live update.

## Negative localization result

As an evidence-only experiment, the visible Windows *current/effective scalar fields* were injected into the Linux DAP-VR core while dirty flags were cleared. This made the transfer substantially worse (for example 997 Hz / 0.25 fell to about 0.188). Therefore those scalar snapshots are not a standalone DSP recipe. They depend on associated coefficient/filter objects and transition history.

Do not turn these memory values into production constants.

## Exact next step

Trace the vendor operation that creates the resolved current/effective state, rather than copying snapshot values. Highest-value targets are:

1. the callers and state-transition timing around `FUN_1800484F0`;
2. the volmax sub-commit `FUN_180048FB8`;
3. IEQ/regulator coefficient-object creation and the pending/current transition observed in the Windows dump;
4. the DAX-RPC update timing relative to the audio block that produced the pre-VLLDP oracle.

The goal remains deterministic reproduction through original vendor operations, not a fitted EQ/gain replacement.
