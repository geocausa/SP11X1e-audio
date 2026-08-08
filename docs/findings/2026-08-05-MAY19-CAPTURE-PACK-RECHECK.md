# May-19 Windows capture-pack recheck — 2026-08-05

## Purpose

Use the twelve paired May-19 deterministic Windows loopback captures as an
independent transfer-function check of the current original-code
VLLDP -> VR implementation, while avoiding the historical mistake of treating
endpoint registry tuning blobs as a live active-profile indicator.

## Capture provenance

The capture set is:

```text
Research_Hub_Audio/SOURCE/UbuntuConceptEliteX/
  windows-loopback-captures/v6-regulator-20260519/
```

with paired source/Windows WAVs for:

```text
bass_multitone_m18
bass_spikes_m8
dense_music_loud_m12
dense_music_quiet_m22
full_sweep_m18
one_k_reference_m18
pink_noise_loud_m10
pink_noise_medium_m18
pink_noise_quiet_m30
seventyfive_hz_reference_m18
speech_like_m16
treble_sweep_m18
```

`Run-SP11CapturePackLoopback.ps1` proves that the script sorted the files by
name, launched a fresh WASAPI loopback recording, waited about 900 ms, played
the source with `System.Media.SoundPlayer`, and retained about three seconds of
capture padding. It did not itself change Dolby profile, endpoint volume,
spatial mode, or enhancement state.

The original PowerShell/agent material recovered so far does **not** prove which
Dolby profile was selected for this capture. Do not infer the Windows profile
from the later Linux filenames `sp11_dynamic_v*`; those were approximation
profiles fitted to the Windows data after capture.

## Endpoint registry correction

The May-18 endpoint snapshot contains the familiar Dolby property families
`{5510c7ab-dfc2-40d0-a98b-5f77f697005e}` and
`{f112024a-fe30-42a8-80ab-8dd825a06f78}`. Their decoded values include the
Movie/Music-style VLLDP compressor payload (deviation 96, slow enable 1,
slow-mix 103), which initially looked like a possible active-profile marker.

That interpretation is **wrong**.

The controlled June-12 `Dynamic -> Music` experiment contains before/after
endpoint registry exports. A direct byte comparison shows zero changes in both
of those property families across the live profile switch. They are
endpoint/device/default tuning data, not a trustworthy current-profile store.

Therefore the May-18/19 active profile must not be assigned from those registry
values alone.

## Current exact-chain capture-pack score

A fresh production build was made from the current source and verified as:

```text
SHA-256 230932e53734c0fc0749eb54c8b8db462c739d7a7bf32cd937be4cb635d9be2b
```

The twelve source WAVs were replayed offline through current Dynamic, Movie and
Music original-code chains. Each Windows loopback was aligned independently to
its source; observed start offsets are tightly clustered around 0.56-0.61 s.
No fitted per-file gain was used for the absolute RMS comparison.

Pack-wide absolute RMS error:

```text
profile    mean abs    RMSE      median abs     mean bias
Dynamic      1.049 dB   1.232 dB   1.156 dB      +0.317 dB
Movie        0.899 dB   1.021 dB   0.826 dB      +0.222 dB
Music        0.958 dB   1.315 dB   0.540 dB      -0.958 dB
```

Per-signal closest profile by absolute RMS error:

```text
Dynamic  3/12
Movie    2/12
Music    7/12
```

This does **not** prove Music was selected: Movie has the best pack-wide mean
error while Music wins the majority of individual signals and has the best
median. The correct engineering conclusion is that the unlabeled capture state
is still ambiguous, while the original-code chain is already substantially
closer to this Windows reference pack than the old hand-built approximation
families (historical v12 report: about 1.31 dB mean absolute loudness error).

## AudioEng limiter on the pack

The decoded exact Windows `AudioEng!CAudioLimiter` oracle was placed after each
current profile render.

It engages on loud transients/material but changes the pack score only slightly
and in the wrong direction:

```text
profile    raw RMS MAE   +AudioLimiter RMS MAE
Dynamic       1.049 dB          1.056 dB
Movie         0.899 dB          0.904 dB
Music         0.958 dB          0.966 dB
```

Therefore `CAudioLimiter` remains a real Windows graph actor / final safety
ceiling, but it is not the source of the missing loudness drive.

## 75-Hz low-level harmonic control

The pack includes a clean 75-Hz source at -18 dBFS. Over a steady six-second
window the source contains only PCM-quantization-level harmonics:

```text
source H3  about -84.99 dBc
source H5  about -89.59 dBc
```

The aligned Windows loopback is essentially equally clean:

```text
Windows fundamental  about -8.99 dBFS
Windows H3           about -82.98 dBc
Windows H5           about -89.53 dBc
```

This is strong evidence that Windows is **not continuously synthesizing a large
psychoacoustic harmonic-bass component** at ordinary level. It sharply
contrasts with the older loud 75-Hz staircase, where near the digital ceiling
Windows develops H3 around -34 dBc and H5 around -42..-44 dBc.

The large odd-harmonic effect is therefore level/ceiling dependent rather than a
continuously enabled named Virtual Bass generator.

For reference, cold current profile replays show slightly more low-level H3
residue (-69..-73 dBc) than Windows, another sign that exact runtime state/history
still matters and that a profile-unlabelled capture should not be overfit.

## `VR core+0x5E0` correction

The stable Windows-vs-Linux compact discrepancy at `VR core+0x5E0` was traced
back through `FUN_180039D64` / `FUN_1800376B0`. It is a lazy cache-dirty flag
behind the output-mode branch. Windows Music leaves the branch disabled while
our constructor path had initialized/consumed it. Omitting the output-mode
setter for Music is already proven bit-identical, so `+0x5E0` is not a missing
gain or limiter state.

## Current boundary

The strong remaining question is no longer "is there a permanently enabled fake
bass switch?". The evidence says no.

The remaining Windows-vs-Linux residual is a runtime/state/lifecycle problem
that becomes most visible near the loud ceiling. Candidate work should be judged
against profile-proven captures or live state, and should not be tuned to an
unlabelled historical waveform by assumption.
