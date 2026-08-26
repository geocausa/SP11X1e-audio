# 2026-08-26 FullIO v19c — Golden protected render + native MicArray acceptance

## Scope

This checkpoint promotes the combined native SP11 audio chain from separate
working speaker and microphone candidates to one simultaneously usable topology.
Suspend/resume is **not part of this acceptance**: it is a known platform issue
owned by a separate dedicated RE and was deliberately not exercised here.

## Root cause closed

Native Audio v18 used the accepted TX MicArray capture closure but its playback
side was the generic multi-stream topology. That topology did not instantiate
the Golden protected render graph (SP/SPVI, protected VI/CPS, MSIIR and final
VOL_CTRL), so the exact Windows endpoint transaction correctly returned
`-ENODEV` even while playback was running.

A Golden+Mic merge then exposed a second integration boundary. The diagnostic
capture graph used subgraph/container ID `0x4003`, while Golden protected render
already owns module IID `0x4003`. Capture worked alone but SPF rejected graph 2
when Golden was resident. There were no same-class ID collisions; the failure
was a cross-object AudioReach instance-ID alias.

FullIO v19c keeps the proven capture module IIDs and moves only its graph-object
allocations into disjoint high namespaces:

```text
capture FE subgraph  0xb0000203
capture FE container 0xe0000203
capture BE subgraph  0xb0000209
capture BE container 0xe0000209
```

The builder now rejects module/subgraph/container cross-class collisions before
it emits a merged topology.

## Reproducible topology

Builder:

`tools/build_sp11_native_audio_topology.py`

SHA-256:

`15f0c48680de2315b841df3204bfeaedac3d1409af835bb6e0be657ed840d672`

Reviewed Golden inputs remain:

- `artifacts/reviewed/windows-default-speaker-structural-model.json`
- reviewed Golden-v33 protection-stage evidence set (kept outside the public release payload)
- `artifacts/reviewed/windows-default-control-link-topology-data.json`

The protected base is still required to compile to exact Golden-v33 SHA-256:

`1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`

FullIO v19c source:

`deploy/native-audio-v19c/X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0.conf`

SHA-256:

`241f32cd2278c7df745f17a6c70f3259109b68ffeea1d4984353de4afd99bc39`

Compiled topology:

`deploy/native-audio-v19c/X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0-tplg.bin`

SHA-256:

`e7bb06a03e7bd9b869825a51775355a6743477d1579d78eb09fad5881cfb20f0`

A fresh builder run after adding the collision gate reproduced that topology
byte-for-byte.

## Boot isolation

FullIO v19c changes topology selection only. Kernel and initrd are inherited
byte-for-byte from accepted native-audio v18:

```text
kernel bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a
initrd ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d
DTB    2fcfa738c229b32764ff2722847cf4056b3153c64a12f8490429309f29df6d00
```

The v19c DTB differs from v19b only in the same-length sound-model selector;
the v19b-to-v19c file comparison had exactly one differing byte (`b` -> `c`).

Current boot identity during acceptance:

`sp11_entry=7.1.5-sp11-fullio-v19c`

## Card and desktop publication — PASS

ALSA exposes:

```text
card0/device0 MultiMedia1 Playback
card0/device2 MultiMedia3 Capture
```

WirePlumber/PipeWire publishes both at the same time:

```text
Built-in Audio Speaker playback
Built-in Audio Internal microphone array
```

The v19b `No graph found with id 2` / GRAPH_OPEN rejection is absent.

## Golden protected render — PASS

The v19c boot accepted the Golden stages including:

- SP operating mode/tag/configuration;
- SPVI configuration, R0/T0, channel mode, processing mode and tag calibration;
- render endpoint and VI endpoint calibration;
- VOL_CTRL gain;
- full-volume MSIIR calibration;
- VOL_CTRL mute;
- GRAPH_START.

## Exact Windows endpoint volume/mute — PASS

The production sync path reports:

```text
volume_transaction_control_values=288
mode=windows-lr
ckv_delta=prior-new
volume_only_control_values=16
endpoint_mute=exact-dsp
wsa_rx=windows-0db-active
```

Controlled playback proved:

- 10%: GainStep 1, 216-byte delta, final Q28 `0x0051b9d7`;
- 35%: GainStep 9, 272-byte delta;
- mute: `endpoint_dsp_mute=1`;
- unmute: `endpoint_dsp_mute=0`;
- restore 10%: reverse CKV transition to GainStep 1;
- hardware scalar stayed 1.0; no host-attenuation fallback occurred;
- WSA RX moves to 84 (0 dB Windows-active producer) while active and returns
  to 81 (-3 dB Golden idle baseline) after playback.

## PipeWire MicArray — PASS

User-facing PipeWire capture at 48 kHz, stereo S16_LE produced about 4.245 s:

```text
203776 frames
channel 0: 181007 nonzero, min -59, max 58, RMS 14.62
channel 1: 182216 nonzero, min -64, max 60, RMS 15.06
```

No AudioReach/ASoC error accompanied the capture.

## Full duplex — PASS

Protected playback and MicArray capture were simultaneously `RUNNING`.
The concurrent capture produced:

```text
347136 frames / 7.232 s
channel 0: 324043 nonzero, min -113, max 104, RMS 23.28
channel 1: 325386 nonzero, min -121, max 103, RMS 24.56
```

No GRAPH_OPEN failure, ASoC error, xrun, underrun or overrun was logged during
the duplex interval. Exact DSP volume control remained active.

## Runtime PM / teardown — PASS

After both streams close:

```text
PCM0 playback: closed
PCM2 capture:  closed
6b00000.codec WSA: suspended, runtime_usage=0
6ae0000.codec TX:  suspended, runtime_usage=0
6d44000.codec VA:  suspended, runtime_usage=0
vreg_l1b_1p8: use_count 0
6d44000.codec-vdd-micb: 0
```

This is a runtime-PM/idle gate only. It is **not** a system suspend/resume test.

## UbiG profile-control defect closed

The `ubig-control 0.1.3` .deb and its production v2 control page were healthy.
The broken component was the old compatibility helper `sp11-ubig`:

1. substring node matching for `effect_input.sp11_ubig` selected
   `effect_input.sp11_ubig_bypass` first and made bypass the default;
2. the helper wrote obsolete `/run/user/1000/sp11-ubig-profile.control` bytes,
   while the production engine consumes `/run/user/1000/ubig-control-v2`.

`deploy/ubig/sp11-ubig` now uses exact node-name matching and `/usr/bin/ubigctl`
for v2 profile, postgain and GEQ transactions. The installed helper is
byte-identical to the repository copy:

`32a8afc24a8c81dd7acafca4e7765392a3905b493b67217d54eaab8bbcb3a1b4`

Live helper testing with audio flowing passed:

```text
Game -> Music -> Voice -> Course -> Dynamic -> Movie -> Custom
```

For every profile, desired == active; the UbiG sink remained default and the
filter-chain PID stayed `1842` throughout. The same existing Custom 20-band
curve was re-applied through v2 without restarting the engine.

Final production control state:

```text
request_generation=19
ack_generation=19
desired_profile=Custom
active_profile=Custom
desired_postgain=-545
active_postgain=-545
last_error=0
```

The preserved Custom curve is:

```text
138,125,78,40,26,0,0,-11,13,40,70,82,80,101,93,96,0,0,0,0
```

The obsolete two-byte runtime control file was removed after verifying that the
production units are explicitly bound to `ubig-control-v2`.

## Acceptance disposition

All non-suspend native audio gates required for FullIO v19c are green:

- Golden protected speaker graph: PASS
- exact Windows volume/mute/GainStep/MSIIR path: PASS
- UbiG profile switching: PASS
- UbiG Custom EQ preserved/applied: PASS
- PipeWire MicArray: PASS
- simultaneous playback/capture: PASS
- runtime PM and mic-bias release: PASS

System suspend/resume is externally deferred to its dedicated RE and does not
block this audio-chain acceptance.

## Promotion state

After the non-suspend acceptance gates and clean-clone verifier passed, the
release installer regenerated the v19c GRUB entry and promoted it from one-shot
validation to the saved default without rebooting the already-running accepted
boot.

```text
running:     sp11_entry=7.1.5-sp11-fullio-v19c
saved_entry: sp11-audio-fullio-v19c
first rollback:  sp11-audio-dmic-broker-div4-v18
second rollback: sp11-audio-golden-v33-topcfg1-physical-vi
```

Both rollback entries remain present in the generated GRUB configuration.
`deploy/native-audio-v19c/install-grub-entry.sh` reproduces the entry using the
current root UUID and refuses promotion unless the live v19c verifier passes.
