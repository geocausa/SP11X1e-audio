# Dolby evidence tools

These tools keep the public repository reproducible without publishing Windows
process dumps, captured WAVs or vendor Dolby DLLs.

## State-pinned oracle analyzer

`analyze_state_pinned_oracle.py` reads raw PCM16 stereo WAVs and Windows full
minidumps directly. It does not require WinDbg symbols.

Example:

```sh
python tools/dolby/analyze_state_pinned_oracle.py \
  --source-wav sp11-known-input-stimulus-48k.wav \
  --loopback-wav windows-loopback-20260807-075900.wav \
  --dump audiodg-2800-source27p5.dmp \
  -o oracle.json
```

The dump parser locates `DolbyAPOvlldp150.dll`, finds the unique live inner
wrapper from the relocated primary vtable pointer, and reports:

- VLLDP `system-gain`, applied/staged postgain and `peak-level`;
- the real final-limiter ceiling, envelope and gain state;
- inner 256-frame input/output staging RMS and peaks.

The vtable/offset geometry is tied to the hash-matched SP11 VLLDP binary
recorded in the findings. Do not assume the same RVAs apply to another Dolby
revision.

## Stereo-matrix discriminator

`generate_stereo_matrix_probe.py` creates a 17-second 48-kHz PCM16 stereo WAV
with three 75-Hz cases:

```text
in-phase     L=+x R=+x
left-only    L=+x R=0
anti-phase   L=+x R=-x
```

Those three inputs uniquely distinguish many candidate stereo sums/matrices
that a correlated dual-mono test cannot.

Example:

```sh
python tools/dolby/generate_stereo_matrix_probe.py diagnostic-stereo-matrix-75hz.wav
```

The generated file is intentionally ignored by Git. Record its SHA-256 with the
capture notes.

## Windows loopback capture

`../windows/Record-WindowsLoopback.ps1` records the default multimedia render
endpoint through WASAPI shared loopback and writes PCM16 WAV output. Its default
output directory is relative to the current working directory; it contains no
developer-specific path.

```powershell
powershell -ExecutionPolicy Bypass -File tools\windows\Record-WindowsLoopback.ps1 -Seconds 30
```

A loopback WAV by itself is not enough for a state-pinned parity claim. Record
profile, endpoint volume and the relevant runtime state/dump at the same time.

## Evidence policy

Raw `*.wav`, `*.dmp` and vendor `*.dll` files are ignored. Findings commit only
hashes, reproducible analysis code and derived observations.
