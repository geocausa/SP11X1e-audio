# Windows genuine spatial-object oracle staged

Date: 2026-08-10

## Why this is now the correct next oracle

The Aug-8 full-memory graph closure proves ordinary stereo is not an HRTF/object parity target:

- Microsoft VirtualSurround is byte-transparent for the frozen stereo blocks and its ASAR-object
  output mode is disabled;
- Dolby SFX creates the `0.25 -> ~0.52` transfer before ASAR;
- Adaptive Spatial Audio Renderer h10->h11 is byte-exact unity for all 960 float32 samples in both
  frozen steady-tone dumps;
- the original Dolby HRTF/DAP bed-only host is independently bit-exact unity under the exact
  Windows 480-frame contract.

Therefore any remaining HRTF/object parity question must be tested with a **real Windows Spatial
Audio object stream**, not by duplicating ordinary stereo into FL/FR object slots in the Linux lab.

## Native Windows ARM64 probe

Added:

```text
tools/windows/sp11_spatial_object_oracle.c
tools/windows/build-sp11-spatial-object-oracle-arm64.sh
tools/windows/capture-sp11-spatial-object-oracle.ps1
tools/windows/README-spatial-object-oracle.md
```

The probe is intentionally self-contained.  It carries only the minimal public COM ABI definitions
needed for:

```text
IMMDeviceEnumerator / IMMDevice
IAudioEndpointVolume
ISpatialAudioClient
IAudioFormatEnumerator
ISpatialAudioObjectRenderStream
ISpatialAudioObject
```

It does not depend on a Windows SDK or CRT.  Linux cross-build uses `clang-cl`, `lld-link` and
`llvm-dlltool`; generated import libraries contain only the required `KERNEL32.dll` and `OLE32.dll`
exports.

Probe signal:

```text
object format      mono float32 / 48 kHz
preferred object   Dynamic
fallback object    static FrontCenter if no dynamic object is available
position           x=+1 m, y=0, z=0 for Dynamic
frequency          997 Hz
amplitude          0.05
lifetime           30 s
```

The dynamic position is intentionally lateral so a genuine HRTF/object path is distinguishable from
ordinary bed bypass.

## Mandatory physical-volume safety gate

Before activating `ISpatialAudioClient`, the probe:

1. opens `IAudioEndpointVolume` on the same default render endpoint;
2. saves the current master-volume scalar;
3. sets master volume to **0.01 (1%)**;
4. reads it back;
5. refuses to start any object stream unless the verified value is <= 1.01%;
6. restores the previous master volume on normal exit.

Thus failure after the safety step leaves the endpoint at the safe 1% value rather than at a louder
value.  This makes the capture compatible with the standing silent/night lab constraint.

## Automated full-memory capture

The Windows image already contains native ARM64 Sysinternals `procdump64a.exe`.  The PowerShell
orchestrator waits for the probe's first successful update marker:

```text
READY_FOR_AUDIODG_FULL_DUMP
```

and then:

1. captures `-ma` dumps of every live `audiodg.exe` PID;
2. waits for the probe's normal exit so endpoint volume is restored;
3. requires `SPATIAL_ORACLE_RESULT PASS`;
4. hashes the probe/log/dumps;
5. writes a JSON manifest.

No debugger GUI interaction is required.

## Linux cross-build verification

The probe cross-builds successfully as a native ARM64 PE with no default libraries:

```text
Machine                IMAGE_FILE_MACHINE_ARM64
Subsystem              WINDOWS_CUI
Entry point             0x1000
PE timestamp            0
imports                 KERNEL32.dll, OLE32.dll only
```

The linker timestamp is fixed deliberately.  Two independent builds separated in time are
byte-for-byte identical:

```text
SHA-256 49602dafc20f325f3fd7d5a966fa55e7af586970a551543c7c3d196fc2c6573f
REPRODUCIBLE_BUILD_PASS
```

The PowerShell capture script also passes a parser-only syntax check under PowerShell Core on Linux.

The generated EXE is **not committed**; source/scripts/docs are the repository artifacts.  A private
local compiled copy is retained with the RE evidence so it can be transferred to SP11 Windows on the
next Windows boot without depending on a compiler there.

## Expected closure from the next Windows run

A successful full-memory capture should finally provide the proper object-path oracle for:

- live VirtualSurround ASAR-output mode;
- non-zero `CASARSampleBuffer` descriptors and sample backing;
- `MainPluginRenderer` object calls;
- original Dolby HRTF object input/output buffers;
- the AudioEng CAPONode before/after ASAR;
- exact dynamic-object position/type/gain metadata.

That oracle can then be fed into the already-working Linux original-Dolby HRTF/DAP host without
reopening the now-closed ordinary-stereo gain problem.
