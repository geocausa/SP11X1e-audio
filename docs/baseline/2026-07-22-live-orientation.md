# Live orientation — 2026-07-22

## Machine and software

- Machine: Microsoft Surface Pro 11 OLED (`microsoft,denali-oled`)
- SoC: Qualcomm X1E80100 / SC8380XP
- Distribution: Ubuntu 26.04 LTS, arm64
- Kernel: `7.1.3-sp11-baseline1+`
- ASoC card: `X1E80100-Microsoft-Surface-Pro-11`
- Userspace: PipeWire 1.6.2 with WirePlumber
- Codecs: two WSA884x devices enumerated on SoundWire link 1

Upstream Linux 7.1.3 already supplies the Qualcomm machine driver, Q6APM /
AudioReach support, WSA macro support, WSA884x codec driver, and the Surface
Pro 11 device-tree sound description.  This is therefore an integration and
topology-correction project, not a from-scratch device-driver port.

## Reproduced speaker path

The active UCM profile directs speaker playback to `hw:0,0` (MultiMedia1).  A
two-second silent playback probe using 48 kHz, signed 16-bit stereo PCM reached
ALSA `RUNNING`.  ASoC debugfs showed the MM1 front end, AudioReach graph,
`WSA_CODEC_DMA_RX_0`, `WSA Playback`, and both speaker endpoints powered.  No
new kernel audio message appeared during the probe.

This proves that the basic render transport works.  It does not assess audible
quality, volume calibration, repeated start/stop, suspend/resume, microphone
capture, voltage/current feedback, or closed-loop speaker protection.

## Confirmed topology defect

The installed topology has SHA-256:

```text
b199990b65d7f07b8dca6e69e3ecad7706c998b510720520aec6dbbf7d9b5a2d
```

Decoding it with `alsatplg` shows two module definitions with token 201
(`AR_TKN_U32_MODULE_INSTANCE_ID`) set to decimal 24608 / `0x6020`:

```text
stream0.msiir0    0x6020
stream2.logger1   0x6020
```

The live kernel reports:

```text
Duplicate Module Instance ID 0x00006020 found
```

The Qualcomm topology parser returns `-EINVAL` for the duplicate, so at least
the later module definition is rejected.  The collision must be removed before
secondary graph or capture failures are investigated.

An existing comparison binary named `no-extra-msiir` has SHA-256
`36adf284bdb3ad9477bf85e6d4ddd6f5316a9316b1f115e0a04739b758ca14f8`.
The validator finds 66 DAPM-backed module definitions in it and no duplicate
module instance IDs, compared with 69 definitions and one collision in the
active topology.  This makes it useful for graph-level comparison; it does not
by itself establish that the candidate is functionally correct or safe to
deploy.

## Kernel delta requiring redesign

The local kernel differs from upstream WSA884x handling by excluding PBR,
VISENSE, and CPS ports in `wsa884x_hw_params()`.  The change is global to every
WSA884x rather than scoped to this machine.  In the current UCM state VISENSE
and CPS are disabled, and no amplifier-to-SoC sense data path has been observed.

The safe conclusion is that ordinary playback works with those ports omitted.
It is not evidence that telemetry or closed-loop protection works.  Port
direction and channel mapping must be established from raw hardware/Windows
evidence before a separate feedback stream or a device-specific quirk is
implemented.

## Other boot observations

- One AudioReach `APM_CMD_GET_SPF_STATE` timeout occurs during startup, after
  which the card registers.  Treat it as a firmware-readiness race to measure,
  not the first blocker.
- The WSA macro warns that it is using a zero-initialized flat register cache.
  Track this after a deterministic topology baseline exists.
- MultiMedia2 reports no backend DAI, but the active speaker route is MM1.

## Evidence policy

The attached historical analysis contains useful Windows ETW, KDNET, static
analysis, topology binaries, hashes, and deployment records.  Its narrative
documents contain mutually incompatible root-cause claims.  Raw captures,
exact binaries, hashes, and live observations are primary evidence; narrative
conclusions remain hypotheses until reproduced.

## First milestone

1. Capture a stable manifest of the current kernel, DTB, modules, topology,
   UCM, controls, and logs.
2. Validate module instance IDs and graph references automatically.
3. Derive a minimally changed topology that keeps the working MM1 render graph
   while removing the `0x6020` collision and unproved grafts.
4. Test through a separate boot entry with rollback, covering repeated silent
   opens, audible channel tests at conservative gain, capture, and suspend.
5. Only then redesign and prove the WSA884x protection-feedback path.
