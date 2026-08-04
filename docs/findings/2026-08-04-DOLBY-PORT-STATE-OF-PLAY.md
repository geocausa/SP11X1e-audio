# Dolby port — state of play, 2026-08-04

Read this first. Everything below is committed and pushed to
`github.com/geocausa/SP11X1e-audio`, branch **`dolby-re-decode`**
(six commits, `8a95876` .. `a715962`).

---

## The one thing to know

This is a **decode of the real Dolby implementation**, not an approximation.
Every constant was read at its exact address in `DolbyAudioProcessing.dll`
and every algorithm follows the disassembly. An earlier attempt that wrote
DSP from scratch and tuned it against the measured curve is marked
SUPERSEDED and should not be used.

Binary: `DolbyAudioProcessing.dll`, ARM64 PE, image base `0x180000000`,
`.text` VMA `0x180001000` at file offset `0x400`,
sha256 `900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3`
at `00-RE-archive/.../dolby/dolby-qualcomm-dissection-local/runtime-live/`

---

## The technique that made it possible

**The DLL is not stripped.** It carries 49 C++ symbols in its logging
strings, with source paths:

```text
dolby::dapvr::CDapVRModule::Process        string VA 0x180316b10
dolby::dap::VlldpModule::Process           string VA 0x180324d20
dolby::oar::OARModule::Process
C:\b\0\SampleCodecPack_v2\DolbyAudioProcessing\src\DAPVRModule.cpp
```

To locate a function, find the `adrp`/`add` pair that materialises its name
string. Script it: page = `VA & ~0xfff`, offset = `VA & 0xfff`, then scan the
disassembly for an `adrp` to that page followed by an `add` of that offset in
the same register. That resolved in minutes what call-graph chasing had not.

RTTI is **not** available for the DSP classes — only `std::` exception types
and `Util::` loggers have type descriptors, so vtable recovery is out.

---

## What is decoded and working

| Component | Function(s) | State |
|---|---|---|
| Volume leveler | `0x180051658`, `0x1800518a0`, `0x180051950` | complete |
| Regulator | `0x180051b38`, `0x180051cf0` | complete |
| dB→linear (exp2) | `0x180096c50` | complete, verified |
| Envelope limiter | `0x180097228` | **complete, exact** |
| VLLDP block processor | `FUN_1800922f8` | 14-stage chain mapped |
| Speaker PEQ | from the tuning XML | complete |

### The limiter is exact

```text
input 0.50 -> peak 0.50000 (-6.02 dBFS)   untouched
input 1.50 -> peak 0.98514 (-0.13 dBFS)   LIMITED
input 3.00 -> peak 0.98514 (-0.13 dBFS)   LIMITED
```

`-0.13 dBFS` is the measured Windows ceiling, hit to the digit.

The last bug was instructive: the gain had converged correctly to
`0.65676 = threshold/1.5`, but blocks spanning a zero crossing measured a low
peak, earned a high gain, and the ramp carried it into the next loud block.
Taking `min(gain[b], gain[b+1])` is what makes it look-ahead. Without that it
overshot to 1.19.

---

## Evidence the decode is genuine

* `21.59277344` appears in the VLLDP chain **and** as
  `BASS_CROSSOVER_INPUT_GAIN` in `bass_coefficients.h`, verified bit-exact
  months ago by a completely separate extraction.
* `21.592773 × 0.046312 = 1.000011` — a reciprocal pair for entering and
  leaving the log domain.
* The exp2 cubic fits `2^f` over `[0,1)` to a maximum absolute error of
  `0.00116`, and is exact at integer powers.
* `mov x8, #0x6000000060` decodes to `96, 96`, exactly matching
  `regulator-relaxation-amount` in this device's tuning XML.
* `0.01562452316` — imperceptibly below `1/64`, deliberately. No curve fit
  would ever recover that.
* One coefficient family on a 65 denominator throughout: `6/65` (leveler
  smoothing), `3/65` (regulator smoothing), `64/65` (conversion).

---

## What this device actually runs

From `DAX3_SPEAKER_TUNING_MSHW0486_REV0D.xml`, `<profile type="dynamic">`
(the profile `CaptureStreamMonitor.dll` selects for browser playback):

```text
ENABLED    volume-leveler (amount 5, in/out target -320, drc on)
           regulator (timbre 12, relax 96, slope 14,
                      stress 216,216,0,0,0,0,0,0)
           speaker-peq, ieq (amount 10), dialog-enhancer (amount 5)

DISABLED   virtual_bass_process, bass-extraction, sliding-bass,
           mb-compressor, graphic-equalizer, audio-optimizer,
           volume-modeler                     — in ALL TEN profiles

ZERO       pregain, postgain, system-gain, calibration-boost
```

**Dolby adds no static gain at all.** The loudness difference against a bare
Linux path comes from the leveler and regulator alone.

**The bass chain never runs on this hardware.** The spectral harmonic
synthesiser was fully decoded anyway (see
`2026-08-02-dolby-stage3-re-log.md`) — normalise each bin to unit magnitude,
raise to power `n` for mode `n`, accumulate, rotate. It is correct and it is
unused. Including it would be an addition, not a port.

---

## Where to pick up

### 1. Measure the assembled chain

`sp11_dolby_chain.so` builds clean (71984 bytes) with the real limiter wired
in. The last measurement run was cut short. Rebuild and measure:

```sh
cd 01-audio/dolby-port
gcc -O2 -Wall -fPIC -shared -o sp11_dolby_chain.so \
    sp11_dolby_chain.c sp11_dolby_leveler.c \
    sp11_dolby_regulator.c sp11_dolby_limiter.c -lm
```

Targets, from a Windows loopback measurement:

```text
1 kHz  @ -12 dBFS   +8.01 dB
75 Hz  @ -30 dBFS  +16.82 dB
75 Hz  @ -12 dBFS  +10.25 dB
peak ceiling        -0.13 dBFS   <- already exact
```

Test in **blocks** (1024 frames), reconnecting ports each block, the way a
host calls it. A single `run()` over a large buffer hits the `MAXBLOCK`
clamp and reads uninitialised memory — that cost an hour once already.

### 2. Decode the remaining per-sample stages

Inside `FUN_1800922f8`, still undecoded:

```text
FUN_180095460   per-channel main processing
FUN_1800986a8   per-channel apply
FUN_180097178   pre-processing
FUN_180098290   per-channel analysis
```

Use the symbol technique. These are where the remaining dB live.

### 3. Listen to it

Nothing has ever been played through this. Load as a **separate PipeWire
sink**, not the default, so the working audio path is untouched.

---

## Files

```text
01-audio/dolby-port/
  sp11_dolby_leveler.c/h     decoded leveler
  sp11_dolby_regulator.c/h   decoded regulator
  sp11_dolby_limiter.c/h     decoded limiter, exact
  sp11_dolby_vlldp.c/h       decoded exp2 + chain scalars
  sp11_dolby_chain.c         the assembled LADSPA plugin
  sp11_dolby_dax.c/h         SUPERSEDED (fitted) - PEQ still valid
  sp11_dolby_stage1/2.c/h    crossover + Chebyshev harmonics (unused here)

01-audio/docs/findings/
  2026-08-03-dolby-leveler-regulator-re-log.md   annotated disassembly
  2026-08-02-dolby-stage3-re-log.md              spectral synthesiser
  2026-08-02-dolby-bass-stage3-fft-analysis.md   why the old sketch fails
  2026-08-01-dolby-integration-map.md            binaries, imports, COM chain
```

---

## Still only local, not in any repo

* **`02-kernel/`** — the injection path, PA limit removal, event logging.
  Patch copies exist in `01-audio/deploy/kernel-patches/`, but the source
  trees are not version-controlled. If that directory is lost they must be
  re-applied from the patches.
* Large evidence directories: `SP11-KDNET-HANDOFF/`, `02-windows-re-mining/`,
  `CLAUDE_KD_24-06/`, `03-etw-kd-session-traces/`.

This machine has already lost one NVMe. Worth addressing.

---

## Method note

Three parameter-mapping errors and two test-harness bugs were caught during
this work. Every one surfaced as **degenerate output** — twenty identical
gains, a flat curve, silence, a 1.19 peak — rather than as plausible-sounding
wrong audio. That is the argument for decoding over fitting: a fitted
implementation absorbs its own errors and sounds fine while being wrong.
