# Windows shared ASAR / MainPluginRenderer path — 2026-08-06

## Scope

This finding resolves where Windows' `VirtualSurroundApo.dll` and Adaptive Spatial
Audio Renderer (ASAR) sit relative to the persistent SP11 Dolby endpoint chain.
It uses the returned 2026-07-26 Windows ETL scenario matrix plus exact target
Windows binaries recovered read-only from the installed Windows partition after
the archive/ISO copies were shown to be absent or version-mismatched.

No Windows file was modified. `/dev/nvme0n1p3` was mounted read-only only long
enough to copy the two exact target DLLs, then unmounted.

## Exact target binaries

### VirtualSurroundApo.dll

- Windows target version: `10.0.26100.8115`
- SHA-256: `8ae87e55f69eae4c1edda186c3f91ad55b4f4177ba3a3752b1acea91e255b739`
- July target module path: `C:\Windows\System32\VirtualSurroundApo.dll`
- the local 25H2 ARM64 ISO contained an older `10.0.26100.7309` binary and was
  therefore not used for address-level conclusions.

### AudioEng.dll

- Windows target version: `10.0.26100.8875`
- SHA-256: `1e2cc764cae6ebfb6985d8503bb83a36022852fbbf1841c377c5ad2fa2d6795b`
- July target module path: `C:\Windows\System32\AudioEng.dll`

## July StackWalk scenario matrix

The main returned ETL is:

`artifacts/raw/windows-target-20260726/SP11-PARITY-OUTPUT/sp11_audio_parity.etl`

The analyzed `audiodg.exe` PID is `12172`. Raw StackWalk payloads were decoded as
QWORD event timestamp, DWORD PID, DWORD TID, followed by QWORD return addresses.

Observed `VirtualSurroundApo.dll` stack counts:

| scenario | shared mode | VirtualSurround stacks |
|---|---:|---:|
| Dolby UI bypass | yes | 23 |
| Audio Enhancements off | yes | 12 |
| shared RAW | RAW | 0 |
| exclusive PCM16 | no | 0 |
| shared volume-step run | yes | 42 |
| Dolby active | yes | 16 |

Therefore `VirtualSurroundApo.dll` is a **normal Windows shared-mode actor**, not a
Dolby UI on/off discriminator. It disappears from the sampled path in shared RAW
and exclusive mode.

## VirtualSurroundApo is an ASAR client/stager, not the psychoacoustic engine

The exact target DLL's recurrent July PCs (`+0x11440`, `+0x11478`, `+0x11660`)
fall inside its real-time processing routine in source areas named
`avcore\xaudio\hrtf\virtualsurround\lib\baseasarclientapo.cpp` and
`asarsamplebuffer.cpp`.

The function:

- accepts the real-time APO buffers;
- normalizes/stages 48-kHz floating-point channel data;
- extracts/copies channels into ASAR sample buffers;
- tracks ASAR sample-buffer state;
- only enters its local SRC helpers for non-48-kHz cases.

The apparent per-buffer virtual callback at vtable slot `+0x78` resolves to
`FUN_180003E20`. Decompilation shows that this function only writes ASAR sample
buffer metadata: validity/status flags, sample count, gain `1.0`, and related
bookkeeping. It does not perform HRTF convolution, matrixing, filtering, or other
sample-domain psychoacoustic DSP.

The embedded `ISpatialAudioProcessBlockRT` object is likewise control/transport
machinery rather than a hidden convolver.

**Conclusion:** `VirtualSurroundApo.dll` feeds Windows ASAR. It is not itself the
missing psychoacoustic transform.

## AudioEng contains the real ASAR renderer implementation

The exact July `AudioEng.dll` contains the real renderer family, including strings
and code for:

- `CAdaptiveSpatialAudioRenderer::APOProcess`
- `ASAR::MultiSourceHrtfRenderer::ProcessStream`
- `ProcessDynamicObject`
- `ProcessStaticObject`
- `MixSpatial`
- `Process2DBed`
- `TryStartAsyncHrtfProcessing`
- `ApplySrcAndMixSpatial`
- `RegisterSpatialAudioProcessBlock`
- `ASAR::MainPluginRenderer::Process`
- `weakfolddownmixer.cpp`
- `hrtfdownmixer.cpp`
- `vacconnection.cpp`

### Active shared-engine dispatch

Recurring July PC `AudioEng+0x1DADF8` lies in
`CAdaptiveSpatialAudioRenderer::APOProcess` (`FUN_1801DAD80`). The exact branch is:

1. load the current inner renderer pointer from `this+0x130`;
2. call its active/status vtable slot `+0x28`;
3. when active, call the inner renderer's virtual slot `0`;
4. return to `AudioEng+0x1DADF8`.

The fallback copy path is later (`+0x1DAE10`). Thus the recurring `+0x1DADF8`
sample is evidence of a **real active inner renderer call**, not a memcpy fallback.

That dispatch is sampled in every analyzed shared scenario, including shared RAW,
and absent in the exclusive scenario. ASAR itself is therefore a Windows shared
engine baseline.

## The July inner renderer is MainPluginRenderer

The second recurrent AudioEng cluster around `+0x1DB8xx..+0x1DBBxx` maps to
`ASAR::MainPluginRenderer::Process` (`FUN_1801DAEE0`), not
`MultiSourceHrtfRenderer::Process2DBed`.

The function contains the main spatial-plugin calls and diagnostics, including:

- `Main Engine MixChannelBed returned failure`
- `Main Engine Process failed`

and performs plugin bed preparation, `MixChannelBed`, plugin `Process`, metadata,
and status/counter calls.

## Direct ASAR → Dolby HRTF proof

In the July normal shared scenario, sampled stacks directly join the two engines.
Representative stacks include:

`DolbyHrtfEnc.dll+0x15B7C`
→ `AudioEng.dll+0x13AE0`
→ `AudioEng.dll+0x1DB9C4`
→ `AudioEng.dll+0x1DADF8`
→ `audiodg.exe`

and:

`DolbyHrtfEnc.dll+0x6840`
→ `AudioEng.dll+0x1DB8E0`
→ `AudioEng.dll+0x1DADF8`
→ `audiodg.exe`

This proves `ASAR::MainPluginRenderer` directly invokes the Dolby spatial plugin
in ordinary shared playback.

However, the separately documented stereo-HRTF finding proves that matching
stereo is allowed to take Dolby's dry unity-gain bypass path. Therefore plugin
execution alone does not imply that ordinary two-channel music is HRTF-coloured.

## Shared RAW discriminator

In the July shared-RAW scenario:

- `DolbyHrtfEnc.dll` and `DolbyAudioProcessing.dll` remain resident in the module
  list;
- sampled CPU stacks contain **zero** HRTF/DolbyAudioProcessing frames;
- `CAdaptiveSpatialAudioRenderer::APOProcess` and
  `ASAR::MainPluginRenderer::Process` remain active.

So "module loaded" is not equivalent to "spatial Dolby processing executed."
RAW keeps the Windows shared ASAR machinery alive while suppressing or bypassing
the Dolby HRTF plugin path.

## Parity implications

1. Strict SP11 endpoint-Dolby parity remains centered on the recovered
   `DolbyDax3Apo -> VLLDP -> VR` path and the Qualcomm speaker/protection graph.
2. Windows shared playback additionally contains an ASAR/MainPluginRenderer layer.
3. `VirtualSurroundApo.dll` does not need to be cloned as a DSP algorithm; it is
   host/buffer plumbing.
4. A future "Windows shared music presentation" mode may need to account for the
   ASAR/plugin layer, but the existing stereo dry-bypass proof means we should not
   blindly insert headphone HRTF/PHRTF processing into the internal-speaker path.
5. Shared RAW and exclusive are valuable negative controls for separating Windows
   shared-engine presentation from endpoint Dolby DSP.

## Superseded old interpretation

Older project notes sometimes treated the presence of VirtualSurround or
DolbyAudioProcessing as sufficient proof that spatial DSP was acoustically
colouring the stereo stream. The July stack matrix plus exact binary RE supersede
that shortcut: execution state, selected renderer, and the Dolby stereo-bypass
branch must all be considered separately.
