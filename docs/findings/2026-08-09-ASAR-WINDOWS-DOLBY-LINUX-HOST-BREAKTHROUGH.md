# 2026-08-09 — Original Windows Dolby ASAR/HRTF + DAP successfully hosted on SP11 Linux

## Status

This checkpoint records the August 9 breakthrough obtained in the isolated SP11 Linux lab.
It is intentionally written as a durable restart point after chat/context loss.

The result is stronger than a PE-loader smoke test:

> The original ARM64 Windows `DolbyHrtfEnc.dll` and `DolbyAudioProcessing.dll`
> can be activated, wired together, initialized, configured, and used to process
> real audio on Linux.

No replacement psychoacoustic DSP was used for these proofs. The vendor DLLs execute their
original DSP paths. Linux supplies only the minimum Windows/COM/CRT host facade needed by
those binaries.

## Exact vendor binaries

Recovered/reference copies in the local RE archive:

- `DolbyAudioProcessing.dll`
  - SHA-256: `900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3`
- `DolbyHrtfEnc.dll`
  - SHA-256: `b1ad1fa8ed747ca1fb58125fb0d1819c38a2ac644a60d1d5f392ea46d5463038`

Current Microsoft graph binaries copied read-only from the Windows partition for analysis:

- `VirtualSurroundApo.dll`
  - SHA-256: `8ae87e55f69eae4c1edda186c3f91ad55b4f4177ba3a3752b1acea91e255b739`
- `AudioEng.dll`
  - SHA-256: `1e2cc764cae6ebfb6985d8503bb83a36022852fbbf1841c377c5ad2fa2d6795b`

The Windows partition was mounted read-only for recovery and unmounted immediately after the
needed artifacts were copied.

## Windows oracle dumps recovered

The original Windows oracle workspace survived on NTFS. The two dumps used heavily during
this Linux reconstruction are:

- normal/shared steady dump:
  - SHA-256 `855c7560492ee7fccf680125a15373e54bdd8ce8438fec2ef8a0781dece43888`
- 997 Hz / 0.25 steady dump:
  - SHA-256 `35524bfd32ccea5bea26fb87d0b11c57b990badc16687cc51f20b88440fa3194`

The dumps and proprietary DLLs are deliberately not committed to Git. They remain in the local
RE archive / recovered Windows oracle workspace.

## HRTF activation proof

`DolbyHrtfEnc.dll` activates on Linux and reproduces the interface layout seen in the Windows
checkpoint:

- `IAsarEncoder2` at object `+0x10`, vtable RVA `0x19E88`
- setup interface at object `+0x18`, vtable RVA `0x199D8`
- dependency interface at object `+0x28`, vtable RVA `0x19AC8`

The real HRTF factory/object construction path runs successfully with a small Linux host shim.

## DAP activation/configuration proof

The original DAP object also constructs on Linux.

The critical missing readiness step was recovered from the original HRTF call sequence:

1. DAP `Initialize`
2. query DAP interface `67b16434...` at object `+0x18`
3. call its slot 3 / RVA `0x15E80` (`SetAPOInitParameters` path)
4. then call `ConfigureEncoder`

A small COM facade for the APO initialization context is sufficient. The real DAP code walks a
fake device collection and endpoint property store, obtains endpoint properties, and establishes
its valid context state.

With the preserved SP11 Dolby property blobs and a minimal reconstruction of one otherwise
uninitialized C++ format-map entry (`key 0x200`), the original DAP reaches:

```text
CONFIG_LEGIT hr=0
phase=1
ready=1
```

and creates real internal module objects for:

- DAP-VR
- AIDE
- OAR
- VLLDP

## Full HRTF -> DAP initialization succeeds

The decisive end-to-end initialization test uses the original HRTF encoder as the orchestrator.
The sequence succeeds on Linux:

- `SetEncoderEngine(original DAP)`
- copy/pass a real-style `APOInitSystemEffects3` context
- DAP initialization and APO parameter handoff
- DAP encoder configuration
- `IAsarEncoder2::Initialize(256, 48000, ..., stereo)`

Result:

```text
HRTF_FULL_INIT_RESULT PASS
DAP phase=1 ready=1
StereoBypass=1
```

## Real audio processing proof

A 75 Hz, amplitude 0.25 stereo block was sent through the original HRTF bed path using
`MixChannelBed` + `Process`.

The output is bit-for-bit identical to the input:

```text
maxdiff = 0
2048 / 2048 bytes produced
```

This independently reproduces the Windows finding that the matching stereo bed follows the
HRTF stereo-bypass identity path.

## VirtualSurround / AudioEng topology recovered

The exact current `VirtualSurroundApo.dll` and `AudioEng.dll` were recovered and decompiled.
The full-memory Windows dump was then used to identify live objects rather than relying only on
pseudocode field names.

Recovered live chain:

```text
AudioEng ASAR::MainPluginRenderer
  -> NonBlockingStreamList
    -> current stream node
      -> VirtualSurround CASARSampleBuffer
  -> Dolby HRTF wrapper
    -> IAsarEncoder2
```

Important corrections:

- the embedded `CASARSampleBuffer` seen between processing passes can look reset/unconfigured;
  AudioEng re-arms it per pass;
- VirtualSurround exposes **19 static ASAR object slots**, not merely one object;
- the static object type mapping covers the complete Windows spatial speaker/object set;
- stereo media is deinterleaved into the appropriate static object buffers while AudioEng also
  carries the unity stereo bed;
- AudioEng sanitizes/copies the object properties before calling the Dolby encoder.

The HRTF static-object setter chooses internal state from the descriptor `type`; the exploratory
"slot index" argument does not cause FL/FR static objects to overwrite each other.

## Pre-VLLDP oracle comparison

Windows normal/shared pre-VLLDP oracle values from the August 8 checkpoint:

```text
75 Hz  input 0.10 -> ~0.320
75 Hz  input 0.25 -> ~0.528
75 Hz  input 0.50 -> ~0.9999
75 Hz  input 0.70 -> ~0.9999
997 Hz input 0.25 -> ~0.5229
```

With the original HRTF/DAP on Linux and a 19-static-object replay (FL/FR carrying samples, the
other static objects silent), before profile correction:

```text
75 Hz  0.10 -> 0.286209
75 Hz  0.25 -> 0.524896
75 Hz  0.50 -> 1.017814
75 Hz  0.70 -> 1.386833
997 Hz 0.25 -> 0.295857
```

The 75 Hz / 0.25 point is within about 0.6% of the Windows oracle, strongly supporting the
recovered bed + static-object topology. The whole curve does not yet match, so no fitted gain or
manual limiter should be introduced.

## Concrete live-state mismatch found

The actual DAP-VR wrapper and underlying DSP core were located in the Windows dump.
The wrapper contains the real core pointer at `wrapper + 0x50`.

Using the vendor setter decompilation as an offset oracle, the live Windows Dynamic core has:

```text
volume leveler amount = 5
volume leveler enable = 1
volume leveler DRC    = 1
regulator enable      = 1
regulator timbre      = 12
regulator relaxation  = 96
speaker distance      = 0
```

The Linux-hosted original DAP core initially had the same basic state except:

```text
volume leveler amount = 0
```

The real vendor setter at VA `0x1800450A0` was then called against the Linux-hosted original
DAP-VR core to set amount `0 -> 5` (not a raw memory patch).

Resulting curve:

```text
75 Hz  0.10 -> 0.345976
75 Hz  0.25 -> 0.533044
75 Hz  0.50 -> 1.017814
75 Hz  0.70 -> 1.386833
997 Hz 0.25 -> 0.307061
```

This proves the profile mismatch is real and materially affects the path, but leveler amount is
not the last missing state.

## Remaining wall / next RE target

The problem is no longer PE loading, Windows execution, COM, WinRT, APO context construction,
HRTF activation, DAP activation, DAP `ConfigureEncoder`, HRTF initialization, static object
plumbing, or stereo-bed processing.

The remaining mismatch is the complete **live SP11 Dynamic DAP content-processing state**.
The authoritative Surface tuning indicates additional important controls including:

```text
surround-decoder-enable = 1
surround-boost          = 96
volmax-boost            = 96
```

`DolbyAudioProcessing.dll` contains generated named setter machinery for at least:

```text
dap_vr_surround_decoder_enable_set
dap_vr_surround_boost_set
dap_vr_volmax_boost_set
```

The next task is to recover their exact callable functions/state offsets from the generated
content-tuning dispatcher, apply them through the original vendor API where possible, and rerun
the five-point oracle.

Do **not** replace the remaining transfer with a fitted gain or an artificial clamp. The Windows
oracle is level dependent and the goal is execution/state parity with the original vendor DSP.

## Private fixture hashes

The small captured tuning-property blobs used by the Linux lab are kept out of Git. Current
lab copies hash to:

```text
dahp_1 / dahp_4: b6be9fd91f09bab641f99db05d02b8f19fd93f64e5c0e6c425048c0357c5b168
dahp_2 / dahp_5: ca80a9c4106b694cb741aa9033074acbd14d8dc7595108d49e0c9af23f606fcd
```

## Reproducible source saved with this checkpoint

See `dolby-port/linux-harness/` for the Linux activation/configuration/process probes and
`tools/ghidra/` for helper scripts used to recover call sites and live object structure.

The most important restart points are:

- `sp11_hrtf_full_init_smoke.c`
- `sp11_hrtf_bed_process_smoke.c`
- `sp11_hrtf_19object_curve.c`
- `sp11_hrtf_19object_level5_curve.c`
- `sp11_hrtf_linux_core_state.c`
- `sp11_dap_configure_replay_fmtmap.c`
