# Corrected diagnostic boot and calibration-frame closure

Date: 2026-07-31

Status: **successful live observation; no installation or reboot is required to
repeat the result.** The one-shot per-frame diagnostic has completed its purpose
and must not be carried into the normal daily kernel.

## Executive result

The corrected full-configuration diagnostic kernel booted with the ALSA card,
touchscreen and audible speaker path working:

```text
running kernel   7.1.5-sp11-audio-diag-observe+
config            4,061 built-in; 7,651 modules; 15,516 lines
module tree       7,886 module files
ALSA card         X1E80100-Microsoft-Surface-Pro-11
```

The earlier 159-module candidate failure is closed. Its reduced configuration
changed probe ordering, prevented both WSA884x codecs from obtaining their reset
GPIO and produced no sound card. The corrected build uses the exact
`audio-vi` configuration and keeps the diagnostic release isolated.

## Calibration-frame diagnostic

The diagnostic kernel re-sent each protected graph-calibration frame
individually once after the normal aggregate `SET_CFG` returned
`AR_EUNSUPPORTED`:

```text
sp11 graph-cal frame diagnostic: frames=107 accepted=106 rejected=1 first-rejected=63
SP11 OOB frame 63 offset 8352 iid 0x0000412b param 0x0800113d size 28 rejected: -95
```

`-95` is `-EOPNOTSUPP`. The result proves that 106 of the 107 manufacturer
calibration records are individually supported by this SPF build and isolates
the aggregate warning to one unsupported parameter on IID `0x412b`.

This closes the earlier theory that the calibration corpus might be broadly
invalid or wholly unsupported. It strongly supports retaining Qualcomm GSL's
policy of continuing after the aggregate `AR_EUNSUPPORTED` response. The
frame-by-frame retry is not, by itself, a formal proof of aggregate transaction
atomicity; the exact post-aggregate state remains a separate question where a
parameter is queryable.

The eleven apparent Q6APM `SET_CFG` errors in the preserved boot are repeated
reports of this same aggregate condition as graphs are created, not eleven
independent defects.

## GET_CFG and SoundWire observations

Patch `0023` captured complete bounded GET_CFG responses and the selected WSA
ports. The preserved success capture contains:

```text
10 complete GET_CFG response bodies
55 hexadecimal dump lines
5 copies of one 92-byte response
5 copies of one 68-byte response
```

The earlier phrase "65 GET_CFG bodies" conflated the 10 response headers with
55 hexadecimal dump lines. The preserved capture contains 10 bodies. A later
read-only extraction from the same still-running boot had accumulated 22
response bodies and 121 hexadecimal dump lines.

The two preserved response payloads are stable across all five repetitions:

```text
92 bytes  sha256 d46a099f70b2ac0ee001d2adc7f7db2e018e73fbcf6dd10db8034926e27eafa6
68 bytes  sha256 8abc3b615f4f57322365c5c921eb2dcba8a1a3736276b8758f2456e3a3a271f4
```

Both amplifiers selected render ports 1, 2 and 3 at 48 kHz and selected only
port 5 for the 8 kHz VI source DAI. The VI backend reached ready state and
`GRAPH_START` completed repeatedly.

## What is now closed

- The corrected diagnostic kernel is a controlled full-config variant of
  `audio-vi` and boots with audio.
- Patch `0023` works and captures full GET_CFG bodies and actual selected-port
  masks.
- The protected calibration corpus is not being rejected wholesale: 106 of 107
  frames are individually accepted.
- The repeated aggregate `AR_EUNSUPPORTED` warning is explained by one
  unsupported frame, IID `0x412b`, parameter `0x0800113d`.
- The physical 8 kHz VI transport is selected on both amplifiers.

## What remains open

- Decode the semantic fields in the 92-byte SP and 68-byte SPVI responses.
- Establish whether the V/I samples are nonzero, correctly ordered and scaled.
- Establish live R0, temperature and excursion behaviour rather than relying on
  replayed Windows R0/T0 constants.
- Implement runtime DSP `VOL_CTRL` updates; the existing ALSA callback only
  changes a cached field and emits no DSP packet.
- Select volume-appropriate MSIIR calibration instead of one fixed full-volume
  row.
- Reconstruct the missing per-speaker Windows tuning and monitoring subgraphs
  only after the bounded volume-control work.
- Design PBR and CPS transport separately; do not add their shared SoundWire
  ports blindly to the playback stream.

## Evidence and provenance

Reviewed summary:

```text
artifacts/reviewed/linux-audio-diag-observe-success-20260731.json
```

Local raw captures:

```text
artifacts/diagnostic-boots/20260731T190946Z-SUCCESS/
```

Diagnostic source additions:

```text
patches/0023-sp11-observe-getcfg-and-soundwire.patch
patches/0024-sp11-diagnose-graph-calibration-frames.patch
```

Patch `0024` is deliberately diagnostic-only. It re-sends 107 records during
first graph setup and can delay audio start. The experiment is complete; the
normal `audio-vi` kernel remains the clean rollback and daily baseline.
