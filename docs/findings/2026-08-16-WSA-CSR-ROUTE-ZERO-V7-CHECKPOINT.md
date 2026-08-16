# WSA CSR route-time-zero v7 checkpoint — 2026-08-16

## Scope

This is a **partial safety/lifecycle checkpoint**, not an acoustic parity
promotion. It isolates the Windows `DRE_CTL_1[5:1]=0` gain code at the lifecycle
boundary Linux already uses for that field, rather than rewriting it at PA
unmute as rejected v6 did.

## Candidate construction

v7 reuses the exact proven v5 WSA8845 kernel module/unmute semantics:

- loaded `snd_soc_wsa884x` srcversion: `A6C0298DDDFCEF0A2C3605F`;
- initramfs module zstd SHA-256:
  `feea9384643d37e041d86d63f84ca814d7f06a6e72036a5254cf5a8d8d646e76`;
- `CSR_GAIN_EN` remains clear on the SP11 2S unmute path;
- there is no v6-style gain-field write in `mute_stream()`.

A boot-scoped, read-only bind-mounted UCM changes only four PA control writes
(two verb-level, two Speaker-device-level):

```text
SpkrLeft/Right PA Volume 24 -> 31
```

The inverted ALSA control therefore stores CSR gain code zero at profile/route
construction time. The underlying normal UCM remains unchanged and the overlay
disappears automatically at reboot. CPS-v3 remains the persistent GRUB fallback.

Normal UCM SHA-256:
`d9cc675fd4d432f62fd3e01fba32a8afe22a64e988fac476120161e67c63fb54`

v7 UCM SHA-256:
`8ff6b20edb161ea969f7c97298f07580b24a8e1ddc59a13eb838c52382d0a6e6`

## Cold-boot gate

The v7 marker booted successfully. The overlay service was active, the target
UCM mount was read-only, both PA controls resolved to 31, every Dolby/speaker
node was suspended and ALSA speaker PCM was `closed`. No new WSA/PA/SoundWire/
XRUN/ADSP runtime fault was present.

## Write-history proof

Read-only `regmap_update_bits_base()` tracing on register `0x34b1` proves the
intended distinction.

During UCM/profile reconstruction both amplifiers receive:

```text
mask=0x3e val=0x00
```

before SoundWire prepare/resume. Subsequent speaker setup touches only bit 0.

During an ordinary later demand wake there are **no `[5:1]` writes**. Both
speaker POST_PMU, PA unmute and PA mute touch only:

```text
mask=0x01 val=0x00
```

This is the key semantic difference from v6, which invented a gain-code-zero
write immediately before PA enable.

Evidence:

- `artifacts/reviewed/2026-08-16-v7-ucm31-route-write-history.trace`
- `artifacts/reviewed/2026-08-16-v7-ucm31-demand-write-and-safety.trace`

## 1% real-program gate

The fixed local Seven Nation Army MP3 was run through the real hidden Dolby ->
speaker path at endpoint 1%. One second into playback the physical ALSA PCM was
`RUNNING`. Producer/COMP readiness preceded PA unmute; after playback the PA
muted before producer/SoundWire teardown and PCM returned to `closed`. No new
WSA/PA/SoundWire/XRUN/ADSP fault was observed.

The external-mic take contained an isolated later room/impulse event but no
sustained post-playback crackle signature. Hardware lifecycle evidence confirms
the speaker path had shut down.

## 90-second idle gate

After the 1% run, a one-Hz watcher sampled the physical speaker PCM for 90
seconds:

- samples: `90`;
- `closed`: `90`;
- non-closed: `0`.

Evidence:

`artifacts/reviewed/2026-08-16-v7-ucm31-idle90.log`

## Status / next gate

**AMBER.** v7 has passed cold provenance, write-history, 1% real-program and
90-second idle gates. It has **not** yet passed:

1. the bounded 5% real-MP3 gate;
2. five byte-identical 12% synchronized chirp captures;
3. comparison of that five-run median against Windows, RX84 baseline and v5.

Do not promote v7 or rewrite production UCM until those gates pass. v6 remains
rejected and must not be re-armed.
