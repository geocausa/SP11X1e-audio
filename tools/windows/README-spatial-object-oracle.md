# SP11 Windows genuine spatial-object oracle

This tool exists to create the **object-stream oracle that the old ordinary-stereo captures could
not provide**.  The Aug-8 graph closure proved ordinary stereo is dry/unity through ASAR, so a
separate `ISpatialAudioClient` object stream is required to validate Dolby HRTF/object parity.

## Probe behavior

`sp11_spatial_object_oracle.c` uses only public Windows COM interfaces:

- default render `IMMDevice`;
- `ISpatialAudioClient`;
- `ISpatialAudioObjectRenderStream`;
- one `ISpatialAudioObject`;
- `IAudioEndpointVolume` as a mandatory physical-volume safety gate.

It requests mono float32 / 48-kHz object audio, activates one **dynamic** object when the endpoint
reports a dynamic slot (otherwise static FrontCenter), and feeds a deterministic 997-Hz sine at
amplitude 0.05.  A dynamic object is held at `(x=+1 m, y=0, z=0)` to create an unambiguous lateral
HRTF case.

Before any stream activation it saves the current endpoint master volume, forces the endpoint to
**1%**, reads it back, and refuses to continue unless the value is <= 1.01%.  On normal exit it
restores the original endpoint volume.  If the process crashes, the safe failure mode is therefore
to leave the endpoint at 1%, not at a louder value.

The stream runs for 30 seconds and prints `READY_FOR_AUDIODG_FULL_DUMP` after the first successful
object update.

## Cross-build from Linux

```sh
tools/windows/build-sp11-spatial-object-oracle-arm64.sh /tmp/sp11-spatial-object-oracle-arm64.exe
```

The build is SDK-free.  It uses `clang-cl`, `lld-link` and `llvm-dlltool`, creates tiny ARM64 import
libraries for the required `KERNEL32.dll` and `OLE32.dll` exports, and links with `/nodefaultlib`.
No CRT is required.

The executable is a native `IMAGE_FILE_MACHINE_ARM64` console PE.  Do not commit the generated EXE;
source/scripts/docs are the repository artifacts.

## Windows capture

Put the built EXE beside `capture-sp11-spatial-object-oracle.ps1` and run the PowerShell script from
an administrative Windows session:

```powershell
powershell -ExecutionPolicy Bypass -File .\capture-sp11-spatial-object-oracle.ps1
```

The script:

1. locates native `procdump64a.exe`;
2. launches the self-volume-capped object probe;
3. waits for `READY_FOR_AUDIODG_FULL_DUMP`;
4. takes `-ma` full-memory dumps of every live `audiodg.exe` PID;
5. waits for the probe to exit normally so its previous endpoint volume is restored;
6. verifies `SPATIAL_ORACLE_RESULT PASS`;
7. writes SHA-256 hashes and a JSON capture manifest.

The resulting dump should be analyzed with the same graph/HRTF tools used for the Aug-8 captures,
with emphasis on:

- VirtualSurround ASAR-output mode becoming active;
- non-zero `CASARSampleBuffer` backing/descriptors;
- `MainPluginRenderer` object calls;
- Dolby HRTF object input/output buffers;
- the AudioEng CAPONode before/after ASAR.
