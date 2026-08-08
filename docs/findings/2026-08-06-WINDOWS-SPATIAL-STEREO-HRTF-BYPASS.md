# Windows spatial Dolby HRTF is live, but DAX speaker stereo explicitly bypasses HRTF bed virtualization — 2026-08-06

## Result

The SP11 Windows shared-mode spatial graph really loads and executes
`DolbyHrtfEnc.dll` and `DolbyAudioProcessing.dll` during preserved Music
playback. This is not an install-only or registration-only feature.

However, the internal DAX speaker endpoint explicitly configures the spatial
DAP engine to **allow/bypass ordinary matching stereo content around the HRTF
channel-bed virtualization path**. The full chain is now proved statically and
dynamically:

```text
DAX3API content-processing policy
  stereo_cp_bypass_mode = 2
  stereo_bypass_dap_dll = IsDaxEndpoint() = 1 on the SP11 DAX endpoint
        |
        v
DolbyAudioProcessing / DAPVR ConfigureRunTime
  active_config + 0x9C = 1
        |
        v
ISpatialAudioEncoderProperties::GetStereoBypassAllowed
  GUID b5bb3cae-fd91-497d-8b83-2eddce0808db
  returns active_config[0x9C] = 1
        |
        v
DolbyHrtfEnc Initialize
  m_StereoBypassMode = 1
        |
        v
CDolbyHrtfEncoderEngine::MixChannelBed
  matching stereo -> short bypass branch
  normal HRTF bed-mixing/virtualization body is skipped
```

Therefore the presence and execution of the Windows HRTF DLLs must **not** be
interpreted as proof that normal 2-channel music is being fully HRTF-virtualized
on the internal speakers. The remaining Windows stereo psychoacoustic search
belongs primarily in the stereo-hot VR surround/virtualizer path,
`VirtualSurroundApo`, `AdaptiveSpatialAudioRenderer`, and any mode-specific
Surface/Microsoft graph behavior.

Personalized HRTF (`PHRTF`) exposed by Dolby Access is a separate
headphone-oriented subsystem and should not be grafted onto the internal
speaker parity chain.

## Exact binaries

HRTF encoder:

```text
SOURCE/UbuntuConceptEliteX/dolby-qualcomm-dissection-local/runtime-live/DolbyHrtfEnc.dll
SHA256 b1ad1fa8ed747ca1fb58125fb0d1819c38a2ac644a60d1d5f392ea46d5463038
```

Spatial DAP engine:

```text
SOURCE/UbuntuConceptEliteX/dolby-qualcomm-dissection-local/runtime-live/DolbyAudioProcessing.dll
SHA256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
```

DAX3 API service executable:

```text
00-RE-archive/sp11-driverdump/
  dax3_swc_aposvc_sdw_arm64.inf_arm64_d6b97a780ee18ae9/DAX3API.exe
SHA256 e77f87dd29275a6f814352494fe019c7a742a1a4ab0fa7911550d15586dda19c
```

Dolby Access OEM binary used only to classify the separate PHRTF model:

```text
SOURCE/Dolby/RPC_Reversal/source/DolbyAccessOEM.dll
SHA256 be4fce2a9cc2b24849ba6b2f8558f2c7d652df543eba7da882f58c59ae88ef19
```

## Runtime proof that HRTF / spatial DAP really execute

The preserved active-stream ETL

```text
20260612_165053_dolby_access_music_ieq_off_to_detailed_active_tone
```

was reparsed with the already-existing project `dissect.etl` environment. For
`audiodg.exe` PID 10260 it contains:

```text
DolbyHrtfEnc.dll          12 stack hits / 26 frames
DolbyAudioProcessing.dll   5 stack hits / 26 frames
VirtualSurroundApo.dll    14 stack hits
DolbyAPOvlldp150.dll     145 stack hits / 997 frames
DolbyApoVr.dll           282 stack hits / 3054 frames
DolbyDax3Apo.dll         430 stack hits / 861 frames
```

Representative active audio-thread stacks include:

```text
DolbyHrtfEnc.dll+0x6640
  -> audioeng.dll+0x13AE0
  -> audioeng.dll+0x1DB9C4
  -> audioeng.dll+0x1DADF8
```

and a separate spatial/DAP stack:

```text
DolbyAudioProcessing.dll+0x11300
  -> +0xBCE8
  -> +0x10208
  -> +0xE9A8
  -> +0x17400
  -> DolbyHrtfEnc.dll+0x50B8
  -> audioeng.dll...
```

Other retained samples enter HRTF RVAs such as `0x15AC0`, `0x15BE8`,
`0x15B74`, and `0x5C00` throughout the active trace. This closes the old gap
where the DLLs were known to be loaded but their actual execution had not been
mapped.

The HRTF binary itself contains and executes code for:

```text
CDolbyHrtfEncoderPlugin::Initialize
CDolbyHrtfEncoderPlugin::Process
CDolbyHrtfEncoderEngine::Process
CDolbyHrtfEncoderEngine::EncodeBlocksForDAPModule1
CDolbyHrtfEncoderEngine::EncodeBlocksForDAPModule2
CDolbyHrtfEncoderEngine::MixChannelBed
CDolbyHrtfEncoderEngine::FoldDownAndMixChannelBed
CDolbyHrtfEncoderEngine::GetStereoBypassMode
```

so it is a real audio engine, not a metadata shim.

## HRTF initialization asks the spatial DAP engine for stereo-bypass policy

`CDolbyHrtfEncoderPlugin::Initialize` queries the exact interface GUID:

```text
b5bb3cae-fd91-497d-8b83-2eddce0808db
```

and calls vtable slot `+0x18`, writing the returned byte to HRTF plugin
`+0x48`. The function logs both:

```text
CDolbyHrtfEncoderEngine::GetStereoBypassMode
StereoBypassMode=%d
m_StereoBypassMode=%d
```

If the interface query fails it explicitly falls back to zero.

The matching `DolbyAudioProcessing.dll` vtable is present beside the same GUID.
At the relevant interface table:

```text
vtable + 0x00  0x180006550
vtable + 0x08  0x180006540
vtable + 0x10  0x180006500
vtable + 0x18  0x18001A160
```

`0x18001A160` decompiles as
`dolby::CDolbyAudioProcessingModule::GetStereoBypassAllowed`.

This closes the interface identity end-to-end: HRTF's cached
`StereoBypassMode` is the spatial DAP engine's **boolean stereo-bypass-allowed
result**, not the raw DAX CP-bypass enum.

## `GetStereoBypassAllowed` reads the active DAPVR configuration byte

The exact function at `0x18001A160` behaves as follows:

```text
if output pointer is null:
    return error

if module is not configured OR active config pointer is null:
    output = 1
else:
    output = *(byte *)(active_config + 0x9C)
```

The active config pointer is held at module `+0x530`.

Therefore the decisive state is the byte at:

```text
active DAP spatial configuration + 0x9C
```

## DAX3API emits an explicit SP11 DAX-speaker stereo policy

`DolbyEndpointControl::GetContentProcessingParamsString` in the exact shipped
`DAX3API.exe` serializes:

```text
"stereo_cp_bypass_mode":%d,
"stereo_bypass_dap_dll":%d,
"eyes-tracking-enable":%d
```

with the arguments:

```text
stereo_cp_bypass_mode  = literal 2
stereo_bypass_dap_dll = DolbyEndpointControl::IsDaxEndpoint()
eyes-tracking-enable  = independently queried runtime value
```

The `IsDaxEndpoint()` function is separately decompiled at `0x140083CC8`. It
returns zero only when both endpoint-type flags at object `+0x1E0/+0x1E1` are
zero; otherwise it returns one. The built-in SP11 Dolby speaker endpoint is the
DAX endpoint, so its content-processing contract supplies:

```text
stereo_cp_bypass_mode  = 2
stereo_bypass_dap_dll = 1
```

This is a shipped-code contract, not an inferred Dolby Access UI state.

## DAPVR `ConfigureRunTime` turns that policy into the exact HRTF bypass byte

The write to active-config byte `+0x9C` was located directly in
`DolbyAudioProcessing.dll`:

```text
0x180010234  strb ..., [config,#0x9C]
```

It belongs to:

```text
dolby::dapvr::CDapVRModule::ConfigureRunTime
DAPVRModule.cpp
```

The parser immediately upstream distinguishes two optional runtime fields:

```text
stereo_cp_bypass_mode
stereo_bypass_dap_dll
```

and the final rule is:

```text
if stereo_bypass_dap_dll is explicitly present:
    config[0x9C] = stereo_bypass_dap_dll
else if stereo_cp_bypass_mode is present and equals 2:
    config[0x9C] = 1
else:
    config[0x9C] = 0
```

Because DAX3API supplies the explicit DAX-endpoint boolean as one, the SP11 DAX
speaker path reaches:

```text
config[0x9C] = 1
```

without depending on enum-name ordering.

The Dolby Access OEM NativeAOT metadata independently contains the CP-bypass
enum family:

```text
DAP_CP_BYPASS_MODE_DISABLED
DAP_CP_BYPASS_MODE_SKIP_VIRTUALIZATION
DAP_CP_BYPASS_MODE_SKIP_CONTENT_PROCESSING
```

The behavioral proof above is stronger than relying on reflection-metadata
ordering: integer mode `2` is the DAPVR fallback value that enables the final
stereo-bypass byte.

## HRTF consumes that byte as a real processing short circuit

`CDolbyHrtfEncoderEngine::MixChannelBed` checks HRTF plugin `+0x48`. For a
matching stream configuration, a nonzero value takes an early-return branch:

```text
if (StereoBypassMode != 0 && stream_config_matches) {
    stage input pointer / sample count / mix scale;
    return success;
}
```

The normal body that analyzes channel masks and performs HRTF channel-bed
mixing/fold-down is skipped on that branch.

Thus this is not merely a UI capability bit; it changes the PCM-processing path.

## Personalized HRTF is a different, headphone-oriented subsystem

`DolbyAccessOEM.dll` exposes a substantial PHRTF object model:

```text
PersonalizedHrtf
PhrtfConfiguration
PhrtfVirtualizerParams
PhrtfSettingsMetadata
PhrtfAdjustment
PhrtfOemAdjustment
PhrtfProvider
StereoBypassAdjustment
phrtf.json
```

but retained type/license/path names also identify:

```text
PhrtfOEMHeadphoneLicense
PhrtfOEMHeadphone
GetActivePhrtfOemAudioRendererId
DolbyPhrtf -> DolbyAtmosForHeadphones
```

So personalized PHRTF must not be treated as an internal-speaker feature merely
because the same Dolby Access package contains it.

A full NativeAOT Ghidra pass was also attempted. It confirmed that these names
live in NativeAOT reflection metadata and do not have normal PE string xrefs;
zero ordinary xrefs are therefore a tooling-format limitation, not evidence
that PHRTF does not exist.

## Conditionality

Older June 6/8 full `audiodg.exe` process dumps do not contain
`DolbyHrtfEnc.dll` or `DolbyAudioProcessing.dll`, whereas later June 12/15/16
spatial/active captures do. The spatial branch is therefore conditional rather
than permanently resident.

This is consistent with the older shared/RAW/exclusive model:

```text
normal shared:
  VirtualSurround + Dolby SFX side graph
  Surface MFX + Dolby MFX + AdaptiveSpatialAudioRenderer recurring graph

shared RAW:
  Surface MFX + Dolby MFX + AdaptiveSpatialAudioRenderer

exclusive:
  no ETW-visible shared APO process loop;
  DolbyAudioProcessing/HrtfEnc absent from the active snapshot
```

## Consequence for parity and psychoacoustics

Do **not** add a generic HRTF convolution or personalized-HRTF stage to the
Linux internal-speaker chain merely to make music sound wider. That would not
match the proved DAX-speaker stereo policy.

For strict ordinary-stereo parity, the HRTF encoder is now a **conditional
Windows spatial actor whose normal matching stereo bed path is explicitly
bypassed**.

For the user-requested psychoacoustic-quality investigation, the next actors
with genuine remaining value are:

1. the original VR surround/virtualizer controls and their Windows live state;
2. `VirtualSurroundApo.dll`, which has real stack hits but unresolved sample vs
   control-plane role;
3. `AdaptiveSpatialAudioRenderer` in `audioeng.dll`;
4. SurfaceAPO EFX/SFX behavior specifically in MEDIA/Spatial/Atmos modes;
5. spatial graph state changes that alter DAX/VR tuning even when the HRTF
   stereo bed itself bypasses.

Those should be exhausted before inventing a non-OEM stereo HRTF stage.
