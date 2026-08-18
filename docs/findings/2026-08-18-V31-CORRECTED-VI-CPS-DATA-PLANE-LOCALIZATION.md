# V31 corrected VI/CPS data-plane localization

Date: 2026-08-18
Status: **evidence checkpoint; CODEC_DMA/AFE handoff is the remaining live boundary**

## Why the earlier zero-tap experiment had to be repeated

The native SP11 Windows-Dolby userspace path was found with its hidden engine node
`effect_input.sp11_windows_dolby_engine` restored at volume `0.06` instead of the
design-required unity value. Direct ALSA and direct PipeWire-to-ALSA playback were
healthy, but playback through the virtual Dolby/control layer was acoustically silent.

The production fix is commit `20a71f2` (`audio: guard hidden Dolby engine unity on
recreation`). It enforces unity/unmuted only on the hidden Dolby engine for a bounded
bootstrap window after each node incarnation. Fault injection `0.06 -> 1.00` repaired
in under 200 ms in the immediate test and within about 350 ms when injected two
seconds after keeper startup. The visible endpoint remained `0.14`; the idle host
fallback remained `0.33`. A cold boot then produced a physical 997 Hz line at about
64.5 dB above the local SP7 spectral noise floor.

Because the earlier logger-zero runs were made while the virtual path was silent, all
protection tap conclusions were repeated with an independently proven non-silent
stimulus.

## Golden tap-1 control with valid native stimulus

Canonical Golden v31, canonical topology, working native Dolby path:

- 89 tap-1 `0x1586` packets captured;
- 88/89 packets carried nonzero payload;
- format: 48 kHz, stereo, S16;
- 8,544 decoded samples;
- peak `593`, RMS about `284.55`.

Raw capture:

`02-kernel/candidates/golden-v31-ckv-delta-20260818/psycho-bass-20260818/golden-native-valid-997hz/`

This proves the DIAG decoder and tap-1 payload interpretation are valid when real PCM
is flowing.

## CPS tap 3 with acoustically proven render

The isolated tap-3 topology disabled tap 1 and forced CPS logger tap 3 immediate / out
of island. During the same 997 Hz render:

- CPS tap 3 produced **zero `0x1586` audio packets**;
- SP7 independently measured the 997 Hz speaker tone at about **62.5 dB** above the
  local spectral noise floor.

Raw capture:

`02-kernel/candidates/golden-v31-ckv-delta-20260818/psycho-bass-20260818/tap3forced-valid-native-997hz/`

Therefore CPS logger silence is not explained by absent speaker render.

## VI tap 2 with acoustically proven render

The equivalent isolated tap-2 topology forced VI logger tap 2 immediate / out of
island. During the same 997 Hz render:

- VI tap 2 produced **zero `0x1586` audio packets**;
- SP7 independently measured the speaker tone at about **63.4 dB** above the local
  spectral noise floor.

Raw capture:

`02-kernel/candidates/golden-v31-ckv-delta-20260818/psycho-bass-20260818/tap2forced-valid-native-997hz/`

The common failure is therefore upstream of both protection loggers and is not a
CPS-only logger policy problem.

## Live kernel tracing

ftrace during a real playback start showed the hidden VI/CPS backends executing:

- `wsa_macro_hw_params()`;
- `qcom_snd_sdw_prepare()`;
- `sdw_prepare_stream()`;
- `sdw_enable_stream()`;
- `qcom_swrm_port_enable()`;
- `q6apm_lpass_dai_prepare()`;
- `q6apm_lpass_dai_trigger()`;
- shared `q6apm_graph_start()`.

Earlier read-only SoundWire MMIO also proved current bank 0 channel-enable `0x03` on
master ports 10, 11 and 13 during playback. The WSA8845 ADC/VI producer had changing
live ADC samples. Thus the physical sensing path is alive through the SoundWire master
controller while the AudioReach VI/CPS source branches still emit no logger frames.

The trace also showed `wsa_macro_enable_vi_feedback()` never executes through DAPM on
Golden. ASoC debugfs confirmed that the pseudo VI protection AIF is structurally
orphaned from the normal VI mixer/MCLK route, while the CPS protection AIF has routes
but remains unpowered as a macro DAPM stream.

## One-variable VI macro activation discriminator

A disposable v31-derived module candidate changed only the VI protection DAI
lifecycle: VI `hw_params` invokes the existing upstream
`wsa_macro_enable_disable_vi_feedback(..., true, ...)` helper and `hw_free` invokes the
same helper with `false`. No guessed register values were introduced.

Candidate artifacts:

`02-kernel/candidates/v31-vi-macro-dai-activation-diag-20260818/`

Important identities:

- live candidate srcversion: `F41558F5C3FC1831BD52D65`;
- signed compressed module SHA-256:
  `9aff30505b17d4c1e4d608c3953b2f53271e7cc0e34931f4e1785ce6635e6963`;
- derivative initrd SHA-256 is retained in `initrd.sha256`;
- Golden kernel and DTB were byte-identical in the candidate boot;
- persistent GRUB fallback remained `sp11-audio-golden-v31`.

During real playback the diagnostic helper definitely took effect: WSA macro TX
speaker-protection path-control registers changed from their baseline state to `0x10`
while active, and the explicit diagnostic marker was emitted.

Result:

- VI tap 2 still produced **zero audio packets**;
- simultaneous SP7 capture still contained the physical 997 Hz tone, about **40.8 dB**
  above the local spectral floor in the shorter overlap window.

Therefore the missing DAPM VI event is a real lifecycle discrepancy but **not
sufficient to explain the dead AudioReach source path**. The candidate is rejected as
a fix and must not be promoted into Golden.

## Current localization

The following are now individually proven alive or accepted during a real render:

1. userspace native Dolby/control path after the unity guard;
2. speaker render / CODEC_DMA sink path;
3. WSA8845 sensing producer activity;
4. DP5/DP6 selection and SoundWire enable;
5. master SoundWire ports 10/11/13 active in hardware;
6. VI 8 kHz and CPS 24 kHz endpoint configuration accepted;
7. VI/CPS hidden backend prepare/trigger and shared graph start;
8. explicit VI macro protection activation is insufficient to restore samples.

The remaining evidence-backed fault boundary is downstream of the SoundWire/macro
producer side and upstream of the AudioReach VI/CPS data-loggers: specifically the
**WSA macro / LPASS -> AFE CODEC_DMA_SOURCE handoff (0x4026 / 0x402b)** and its source
hardware-client/data-plane activation.

Next work should observe or instrument the CODEC_DMA source side directly rather than
repeating transport or logger-policy guesses.

## Safety / retained baseline

Golden v31 remains the persistent saved boot entry. The canonical topology on disk and
its Golden backup both hash to:

`1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`

No rejected VI macro diagnostic code has been promoted into Golden.
