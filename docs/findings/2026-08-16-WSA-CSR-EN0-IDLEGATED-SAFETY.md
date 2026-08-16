# WSA8845 CSR-EN0 v5 under corrected idle lifecycle

Date: 2026-08-16
Status: **bounded safety PASS / H03 AMBER; exact acoustic verdict pending byte-identical Windows-oracle rerun**

## Why v5 exists

The earlier one-variable `csren0-v4` boot changed only the SP11 2S WSA8845 playback-unmute write from `CSR_GAIN_EN=1` to `CSR_GAIN_EN=0`. It could carry controlled short 1% program audio, but the user later reported prolonged crackling while that boot remained active.

Subsequent safe-CPS-v3 tracing proved that the August-14 split Dolby topology had been holding the physical ALSA speaker PCM and WSA8845 PA continuously RUNNING at desktop idle, even with no application streams. Windows does not do this: its captured normal playback lifecycle disables the PA after playback.

Commit `1523015` makes the hidden Dolby engine input passive so the complete speaker graph suspends when no application demands it. Commit `2643e44` additionally pins the actual validated Dolby bridge path (`/home/geoca/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so`); the older tracked `/usr/lib/ladspa/...` path did not exist and could create a visible control sink without a real hidden render engine.

v5 therefore intentionally reuses the **same one-bit WSA8845 module bytes as v4** while changing the host lifecycle around it. This makes the experiment answer one question: whether CSR-off remains unsafe when Linux uses the Windows-like demand-driven PA lifecycle.

## Candidate identity

GRUB one-shot ID:

```text
sp11-audio-rpv4-macro84-winproducer-nohd2-csren0-v5-idlegated
```

Kernel marker:

```text
sp11_wsa_csren0_v5_idlegated=1
```

Saved fallback remained:

```text
sp11-audio-cps-v3
```

The one-shot `next_entry` was consumed after boot. The candidate module in the v5 initramfs was extracted and hashed directly; it matches the staged candidate module:

```text
feea9384643d37e041d86d63f84ca814d7f06a6e72036a5254cf5a8d8d646e76
```

The exact one-variable patch SHA-256 is:

```text
ffd7ecaca3681b24035cdcbda79729905c926bff9c881eb181ae469d088eb02c
```

The deployed passive/correct-bridge Dolby config SHA-256 is:

```text
5c7333076ce895621d36710f1e8e6c2f5050746e52df6cfe1a2916e11f0406d7
```

## Cold-idle gate

Before intentional playback after v5 boot:

- visible endpoint: 1%, then explicitly muted;
- physical speaker sink: suspended;
- visible Dolby sink: suspended;
- hidden Dolby engine input/output: suspended/idle;
- ALSA speaker PCM: `closed`;
- no WSA/PA/SoundWire/XRUN/ADSP fault evidence.

The hidden engine was explicitly checked to exist before counting any playback result. An earlier v5 attempt in which only the visible control sink existed is invalid and is not included as a CSR safety result.

## 1% real-MP3 hardware pass

The fixed local MP3 was played for six seconds through `effect_input.sp11_windows_dolby` at 1% visible endpoint.

One second into playback:

```text
/proc/asound/card0/pcm0p/sub0/status
state: RUNNING
owner_pid: <PipeWire PID>
```

All visible/hidden Dolby nodes and the physical speaker sink were RUNNING, proving the stimulus reached the actual speaker graph.

Read-only lifecycle trace ordering:

1. SoundWire runtime resume/hw_params/prepare/enable;
2. WSA macro interpolator/producer POST_PMU;
3. WSA8845 speaker POST_PMU;
4. first WSA8845 playback unmute roughly 82 ms later;
5. after playback/suspend delay, WSA8845 playback mute;
6. speaker PRE_PMD;
7. WSA macro POST_PMD;
8. SoundWire disable/deprepare.

Final ALSA PCM returned to `closed`. No new runtime fault was found.

Evidence:

```text
artifacts/reviewed/2026-08-16-csren0-v5-idlegated-real-1pct.trace
```

## 90-second idle stress gate

After the real 1% pass, a 90-second watcher sampled the physical ALSA PCM once per second with no intentional playback.

Result:

```text
90 / 90 samples: closed
0 / 90 samples: any other state
```

This directly exercises the condition absent from v4: the corrected host topology actually removes the PA/PCM demand at idle instead of holding the CSR-off amp state indefinitely.

Evidence:

```text
artifacts/reviewed/2026-08-16-csren0-v5-idlegated-idle90.log
```

## 5% real-MP3 pass

The fixed MP3 was repeated at 5% endpoint.

- PCM was RUNNING during program audio;
- 44 lifecycle events captured;
- producer was fully established before PA unmute;
- PA mute preceded speaker/producer teardown;
- SoundWire deprepared afterward;
- PCM returned `closed`;
- no WSA/PA/SoundWire/XRUN/ADSP fault appeared.

Evidence:

```text
artifacts/reviewed/2026-08-16-csren0-v5-idlegated-real-5pct.trace
```

## 12% bounded sweep passes

Three full 12% exponential sweeps were then played through the real hardware path. Every run:

- completed with physical PCM RUNNING during the sweep;
- produced 44 read-only lifecycle events;
- preserved producer-before-PA and PA-before-teardown ordering;
- returned physical PCM to `closed`;
- produced no new runtime speaker/SoundWire/XRUN/ADSP fault.

Evidence:

```text
artifacts/reviewed/2026-08-16-csren0-v5-idlegated-chirp12-run1.trace
artifacts/reviewed/2026-08-16-csren0-v5-idlegated-chirp12-run2.trace
artifacts/reviewed/2026-08-16-csren0-v5-idlegated-chirp12-run3.trace
```

## Acoustic-analysis rejection and exact oracle recovery

The first three 12% v5 sweeps used a newly regenerated WAV that had the intended 2 s silence + 15 s exponential 40 Hz -> 16 kHz + 2 s silence shape, but its WAV SHA-256 was:

```text
8f30f68b28debacb0ea6e4d0acf5a92b8fd6f8ef2db9920303b657d0799b5020
```

The validated Windows/Linux synchronized oracle used:

```text
c8782c7741b3ece628362e785d2ec91b990ef453befa4a3e7d6c3e1cd1f8a208
```

The regenerated-source captures aligned very poorly to the old reference (correlation-quality values about 0.04 versus roughly 0.26--0.29 on prior accepted takes) and showed huge run-to-run ridge spread. Their apparent multi-dB Windows residual is therefore **rejected as an analysis failure, not a v5 acoustic verdict**.

The Windows NTFS partition was subsequently mounted strictly read-only. Five copies of the original chirp were found under the Windows audio-audit directory, all with the exact expected `c878...208` SHA. The canonical Linux stimulus has now been restored byte-for-byte from the Windows copy; the regenerated WAV was retained separately as `chirp-40-16000Hz-24dBFS.regenerated-invalid.wav` to prevent provenance confusion.

## Decision

The old statement “CSR-off is unsafe on Linux” is now too broad. What is established is:

- raw forced `DRE_CTL_1=0x00` remains rejected and must not be retried;
- `csren0-v4` remains a real delayed-crackle failure under the old pinned-open host lifecycle;
- the same isolated CSR-enable-bit change in v5 has passed bounded 1%, 5%, 12%, repeated wake/teardown, and 90-second idle gates under the corrected Windows-like host lifecycle;
- **v5 is not yet promoted to production**: longer-duration confidence and a valid exact-stimulus Windows acoustic comparison are still required;
- H03 should move from RED to AMBER because the enabling lifecycle problem is now understood and the isolated CSR-off state is no longer failing its bounded safety gate.

Next: repeat synchronized v5 acoustic sweeps with the byte-identical `c878...208` source and compare against the retained Windows 12% oracle. Only after that should the stored CSR gain field (`DRE_CTL_1` non-enable bits) be considered as a separate final exact-state variable.
