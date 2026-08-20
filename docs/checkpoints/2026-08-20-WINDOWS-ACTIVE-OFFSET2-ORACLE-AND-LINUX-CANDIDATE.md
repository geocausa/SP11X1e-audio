# 2026-08-20 Windows active Offset2 oracle and Linux candidate

## Decision

The SoundWire WSA feedback-port `Offset2` ambiguity is resolved from preserved native-Windows runtime chronology. Windows programs `Offset2=0` on feedback master ports 10/11/13 immediately before `APM_GRAPH_START`; `Offset2=0xff` belongs to the later stop/shadow state. Golden Linux incorrectly preserves `0xff` while enabling ChannelEnable, yielding `0x03ff....` instead of Windows active `0x0300....`.

## Native Windows lifecycle evidence

Preserved KD trace: `C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_SWR_RUNTIME_20260810_1909Z_2d7c_2026-08-10_19-09-12-880.log`.

Immediately before graph start Windows writes the active feedback bank:

- port 10: `0x06b11a64 <- 0x0300060f`
- port 11: `0x06b11b64 <- 0x03000d0f`
- port 13: `0x06b11d64 <- 0x0300001f`

Those values encode ChannelEnable `0x03` and `Offset2=0x00`. The trace then issues `0x01001002`, proven elsewhere in this repo to be `APM_GRAPH_START`.

After `0x01001003` (`APM_GRAPH_STOP`), Windows later programs the corresponding zero-channel/shadow state with `Offset2=0xff`:

- port 10: `0x00ff060f`
- port 11: `0x00ff0d0f`
- port 13: `0x00ff001f`

Therefore `Offset2=0xff` must not be carried into the active enabled feedback-bank state.

## Golden Linux divergence

Golden v31 active rendering had the WSA master feedback-port state:

- port 10: `0x03ff060f`
- port 11: `0x03ff0d0f`
- port 13: `0x03ff001f`

The exact Golden SoundWire source explains this mechanically. `qcom_swrm_transport_params()` writes `pcfg->off2 << 16`; `qcom_swrm_port_enable()` subsequently read-modify-writes only ChannelEnable at bits 31:24. Thus an inactive/shadow `off2=0xff` survives enable and becomes the observed `0x03ff....` active state.

Only one qcom-soundwire controller exists on this SP11 Linux boot: `/sys/devices/platform/soc@0/6b10000.soundwire`, compatible `qcom,soundwire-v2.0.0`, i.e. the WSA master already bound to the Windows-derived physical base.

## Disposable candidate

Candidate root:

`/home/geoca/Documents/SP11-PROJECT/02-kernel/candidates/v31-feedback-active-offset2-zero-20260820`

Base source is the exact Golden build source:

`/home/geoca/Documents/SP11-PROJECT/02-kernel/sp11-softpause-src-20260813/drivers/soundwire/qcom.c`

Base source SHA-256:

`161199a2a2a7d18dd25262857ce1fba792ce0bab5a01bf68080c7299f9196884`

The candidate adds one opt-in module parameter:

`soundwire_qcom.sp11_feedback_active_offset2_zero=1`

When and only when that parameter is enabled, `qcom_swrm_port_enable()` clears bits 23:16 before applying ChannelEnable for master ports 10, 11 and 13. Disable behavior and all other ports are unchanged.

Candidate module:

- srcversion: `CE1DADE19E1CE61B7FC8843`
- vermagic: `7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64`
- SHA-256: `f9dbc887ed6a06c60a73bb83b9688b021e9c12927b66a962e7191f9e336fff76`

Candidate initrd:

`initrd.img-7.1.5-sp11-render-parity-v4+-feedback-active-offset2-zero`

SHA-256:

`036711799330547b01d0a9f5d211bbf1326914923f8b0ebd76765be45f531a06`

Independent unpack verification confirms:

- patched SoundWire srcversion `CE1DADE19E1CE61B7FC8843`;
- Golden q6apm srcversion remains `687B16CF9C43B43E90C0746`;
- canonical Render-Parity topology remains SHA-256 `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`.

## Runtime gate

This candidate is not a success merely because it boots or writes `0x0300....`. It must preserve normal playback and produce real nonzero Windows-shaped feedback at both source paths:

- CPS / IID `0x402b` / 24 kHz source / tap3;
- VI / IID `0x4026` / 8 kHz source / tap2.

If payloads remain zero, reject the Offset2 hypothesis and return to Golden v31. Golden must remain `saved_entry=sp11-audio-golden-v31` with no persistent default change.
