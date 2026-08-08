# Exact June AudioEng ASAR path reduces matching SP11 stereo to unity copy — 2026-08-06

## Result

The conditional Windows Spatial graph is now closed far enough to exclude
`AdaptiveSpatialAudioRenderer` (ASAR) as a hidden ordinary-stereo widening or
HRTF stage on the captured SP11 DAX-speaker path.

For the exact June 12 AudioEng generation used by the preserved active Music
trace, the real-time path is:

```text
CAdaptiveSpatialAudioRenderer::APOProcess
  -> ASAR::MainPluginRenderer::Process
  -> AsarEncoderWrapper<IAsarEncoder2>::MixChannelBed
  -> DolbyHrtfEnc::MixChannelBed
       matching stereo + StereoBypassMode=1:
       stage original input pointer / sample count / scale, skip HRTF body
  -> AsarEncoderWrapper<IAsarEncoder2>::Process
  -> DolbyHrtfEnc::Process
       scale == 1.0:
       copy staged input to output
```

The AudioEng wrapper performs no additional stereo matrix, crossfeed, widening,
or psychoacoustic sample transform around that bypassed bed. At 48 kHz input
and 48 kHz output, the one scalar supplied by `MainPluginRenderer` remains
unity. The surviving HRTF process code then takes its direct copy branch.

Consequently, ordinary matching stereo passing through the active ASAR/Dolby
spatial graph is **sample-path identity at this boundary**, apart from ordinary
buffer/status handling. The subjective Spatial-ON difference must be sought in
other state changes, especially the Windows-triggered Dolby profile/policy
retune and any genuinely mode-dependent Surface APO behavior.

## Exact June AudioEng recovery

The June 12 ETL identifies the active `AudioEng.dll` image used by
`audiodg.exe` PID 10260 as:

```text
ImageSize     0x306000
ImageChecksum 0x2FA3E3
```

The August System32 copy has already advanced to a different PE checksum and
was rejected.

The exact June-generation ARM64 image was recovered from Microsoft's public
symbol-file service after using the ARM64 Winbindex metadata to identify the
matching Windows 11 26100.8521 generation:

```text
AudioEng.dll
SHA256 944456ceb4cd7653afcee06e4c5a1bcf9dec9a2cf710f4f387adb19d3a5f8894
PE machine      0xAA64 (ARM64)
PE timestamp    0x71212144
SizeOfImage     0x306000
PE checksum     0x2FA3E3
```

That PE checksum and image size exactly match the June ETL image record.
Winbindex metadata places this native ARM64 generation in the May 26 preview
and June 9 cumulative update; the next native AudioEng generation is later in
June.

The image's CodeView record identifies the matching Microsoft PDB. It was also
recovered from Microsoft's symbol service:

```text
AUDIOENG.pdb
GUID {DDE5CF55-C01D-FBE2-F3E3-CA104B95B33A}
SHA256 f1f388ffa25884aa598c26651bfb12bf3dcd5a50eb8447dae78b38cac67d47d4
```

This PDB supplies the exact ASAR public function identities below; no nearby or
current Windows binary is being substituted.

## June StackWalks resolve to the ASAR real-time chain

A fresh census of the preserved active traces resolves the recurring AudioEng
RVAs against the exact PDB.

For the June 12 Music IEQ Off -> Detailed active-tone trace, representative
render-thread stacks are:

```text
DolbyHrtfEnc.dll+0x6640
  -> audioeng.dll+0x13AE0
  -> audioeng.dll+0x1DB9C4
  -> audioeng.dll+0x1DADF8
```

The exact PDB maps those sites to:

```text
0x1DAD80  CAdaptiveSpatialAudioRenderer::APOProcess
0x1DAEE0  ASAR::MainPluginRenderer::Process
0x13AB0   ASAR::AsarEncoderWrapper<IAsarEncoder2>::Process
```

The sampled return PCs have exact call-site meaning:

```text
0x1DADF4  indirect renderer Process call
0x1DADF8  return site in CAdaptiveSpatialAudioRenderer::APOProcess

0x1DB9BC  guard/icall thunk
0x1DB9C0  indirect main encoder Process call
0x1DB9C4  return site in MainPluginRenderer::Process

0x13ADC   indirect IAsarEncoder2::Process call
0x13AE0   return site in AsarEncoderWrapper<IAsarEncoder2>::Process
```

Thus the HRTF stack is not merely an initialization or ActivityID association:
it is the real-time ASAR render callback delegating to the Dolby encoder.

Separate DolbyAudioProcessing/HRTF stacks resolve to ASAR notification methods
rather than the per-buffer path:

```text
0x112E0  CAdaptiveSpatialAudioRenderer::HandleNotification
0x130B0  ASAR::MainPluginRenderer::HandleNotification
0x4F490  AsarEncoderWrapper<...>::HandleNotification
```

This separates spatial control-plane activity from the render callback.

## `CAdaptiveSpatialAudioRenderer::APOProcess` has an explicit identity fallback

The exact `APOProcess` at `0x1801DAD80` first checks its active render-engine
interface. If active, it calls that engine's `Process` method. If no active
renderer owns the block, valid PCM is copied directly:

```text
if active_renderer && active_renderer->IsActive():
    active_renderer->Process(...)
    return

if input.BufferFlags == VALID:
    memcpy(output, input,
           frames * cached_bytes_per_frame_components)
    output.ValidFrameCount = input.ValidFrameCount
    output.BufferFlags = VALID
else if input.BufferFlags == SILENT:
    propagate frame count / SILENT
```

There is no hidden fallback spatial effect in AudioEng itself.

## MainPluginRenderer delegates the channel bed and final block to the encoder

The exact `MainPluginRenderer::Process` performs object/stream bookkeeping, then
handles the ordinary channel bed through its encoder interface.

For a valid input bed it calls encoder vtable slot `+0x20`:

```text
encoder->MixChannelBed(
    input,
    input_frames,
    input_channel_count,
    bed_scale,
    silent_flag)
```

and later calls encoder slot `+0x28` to produce the output block:

```text
encoder->Process(
    input_frames,
    output_byte_capacity,
    output,
    &produced_bytes)
```

The exact wrapper methods prove these slots are thin delegates:

```text
0x13A80 AsarEncoderWrapper<IAsarEncoder2>::MixChannelBed
        -> IAsarEncoder2 vtable +0x30

0x13AB0 AsarEncoderWrapper<IAsarEncoder2>::Process
        -> IAsarEncoder2 vtable +0x38
```

No AudioEng psychoacoustic PCM routine sits between these calls.

## The previously proved Dolby stereo-bypass branch stages the original bed

The exact shipped `DolbyHrtfEnc.dll` `MixChannelBed` branch is:

```text
if StereoBypassMode != 0 && input_channel_count == configured_channel_count:
    staged_input_ptr   = input
    staged_sample_count = configured_channel_count * input_frames
    staged_scale       = silent ? 0.0 : bed_scale
    return success
```

It skips the normal channel-mask/HRTF/fold-down body.

The DAX speaker policy feeding this is already closed in
`2026-08-06-WINDOWS-SPATIAL-STEREO-HRTF-BYPASS.md`:

```text
stereo_bypass_dap_dll = 1
  -> GetStereoBypassAllowed() = true
  -> HRTF StereoBypassMode = 1
```

## DolbyHrtfEnc `Process` copies the staged bed when the scale is unity

Later in the same exact HRTF engine `Process` function, the staged fields are
consumed directly. With no competing encoded output, the branch is:

```text
if staged_input_ptr != NULL:
    count = min(output_capacity_float_count, staged_sample_count)

    if staged_scale == 1.0:
        copy(output, staged_input_ptr, count * sizeof(float))
    else if staged_scale > 0.0:
        scalar_multiply_copy(output, staged_input_ptr, count, staged_scale)
    else:
        zero/silence according to stream state
```

The staging fields are then cleared for the next pass.

Therefore the only remaining sample-domain question on the bypass path is the
single `bed_scale` supplied by `MainPluginRenderer`.

## The SP11 48 kHz -> 48 kHz path makes `bed_scale` unity

`MainPluginRenderer::Process` initializes `bed_scale` to `1.0`. It only replaces
that value when both:

1. its cached rate ratio is not one, and
2. a renderer state flag requests the alternate scale.

`MainPluginRenderer::Initialize` computes that cached ratio as:

```text
(int)(cached_output_rate_float / cached_input_rate_float)
```

The fields come from the input/output format structures validated by
`CAdaptiveSpatialAudioRenderer::LockForProcess`. The preserved SP11 shared-mode
capture evidence gives a two-channel 32-bit-float mix format at 48000 Hz, and
retained Microsoft-Windows-Audio state rows record `48000, 48000` for the
corresponding input/output-rate pair. Hence:

```text
output_rate / input_rate = 48000 / 48000 = 1
```

so the alternate scaling condition is false and:

```text
bed_scale = 1.0
```

The HRTF bypass `Process` consequently takes its direct copy branch.

## VirtualSurroundApo is not a second hidden stereo spatial kernel

The matching June `VirtualSurroundApo.dll` generation was separately recovered
and decompiled. Its dominant sampled child calls resolve to standard Windows:

```text
CLSID 3dc09436-7d83-4ba0-addc-cd47f996c5ba -> AudioMeter
CLSID 06587e71-f043-403a-bf49-cb591ba6e103 -> AudioVolume
```

The `CVirtualSurround` derived callback reached by its ASAR sample-buffer helper
only updates sample-buffer metadata/status and unity-gain flags. It does not
contain the missing stereo psychoacoustic math. The actual renderer ownership
belongs to AudioEng ASAR; the exact ASAR analysis above then closes matching
stereo to unity copy through Dolby's configured bypass.

## Consequence

For ordinary matching stereo on the captured SP11 DAX-speaker spatial graph:

- do **not** add generic HRTF convolution;
- do **not** add personalized HRTF;
- do **not** port `VirtualSurroundApo.dll` as a widening DSP;
- do **not** add an invented ASAR stereo matrix/crossfeed stage.

The remaining high-value psychoacoustic difference is now elsewhere. In
particular, the exact SP11 `msft_atmos` operator policy changes the Dolby state
with Windows Spatial mode (including the recovered Spatial OFF -> Music and
Spatial ON -> Movie policy). That profile/policy retune and any independently
proven Surface APO mode delta should be isolated next.

This finding supersedes the older statement that the ordinary-stereo ASAR role
was still open. ASAR remains important for spatial/object/multichannel modes;
the closure here is specifically the captured matching two-channel DAX-speaker
path.
