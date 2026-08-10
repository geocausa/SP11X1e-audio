# SP11 protection transport closure after repository-wide audit

Date: 2026-08-10 (Europe/London)

## Plain-language result

The clean Linux boot has a working protected speaker-render path and real
voltage/current feedback. Dolby rendering, the SP/SPVI DSP graph, 48 kHz
speaker output, and 8 kHz VISENSE feedback all start cleanly. The two WSA8845
amplifiers also expose useful local status registers through debugfs.

The remaining gap is narrower than earlier reports suggested:

- PBR policy and amplifier controls exist, but the separate PBR SoundWire
  sideband is not proven to be transported by the clean layout.
- CPS DSP modules and the CPS source endpoint are reconstructed, but the exact
  Windows per-speaker SoundWire addressing and scheduling have not been
  recovered. The attempted Linux schedule was falsified by bus-clash alerts.
- Linux does not yet provide the DSP CPS module with the HLOS runtime hardware
  interface payload that tells it how to read each amplifier's voltage and
  temperature registers and where the LPASS command/FIFO registers are.

No further speculative kernel or GRUB candidate should be armed from the
current evidence. The saved clean boot is the correct working baseline.

## Relationship to the pushed Dolby work

The remote repository was checked directly. The earlier publishable Dolby
integration is on `main` at `849350d`. The newer live Dolby/VLLDP continuation
is pushed as `agent/vr-vlldp-order-correction-20260809` at `7c6ad09`.

The current local protection branch,
`agent/audio-integration-protection-20260810`, is a direct descendant of that
remote Dolby branch. Its five later commits add the power-lab and CPS work, so
there is no missing Dolby merge. The layers are connected as follows:

```
PipeWire Windows-Dolby filter -> AudioReach protected render graph
                             -> WSA8845 speaker output
WSA8845 VISENSE feedback     -> SPVI protection input
```

The unresolved CPS sideband is therefore a lower-level amplifier/protection
transport gap. It does not mean the Dolby processing is detached from the
working speaker pipeline. The current protection branch does not yet exist as
its own remote branch; its upstream still points at the pushed Dolby branch.

## Why `qcslimbus8380.sys` is not the missing speaker driver

The old reverse-engineering material was not ignored. It was re-imported into
a persistent Ghidra project and its port-allocation/MMIO call graph was
expanded. The result is conclusive: `qcslimbus8380.sys` controls SLM1, the
Bluetooth SLIMbus path, not the WSA speaker bus.

Independent package evidence agrees:

- the driver INF identifies SLM1 and a BAM base at `0x06c04000`;
- the recovered endpoint ledger identifies SLM1 at approximately
  `0x06c40000` as Bluetooth-only;
- the physical speaker bus is the WSA SoundWire controller at `0x06b10000`,
  owned on Windows by `qcaucd8380.sys`.

The `qcslimbus` work is still useful for understanding Qualcomm's generic
port lifecycle, but it cannot supply WSA port-6 CPS parameters.

## What Windows calibration proves

The recovered ACDB fixes the DSP endpoint contract for CPS:

- CODEC_DMA_SOURCE instance `0x402b`;
- 24,000 Hz, signed/fixed-point 32-bit, two channels;
- channel mask `0x3`;
- LPAIF type WSA, interface index 3;
- corresponding Linux AFE endpoint candidate `WSA_CODEC_DMA_TX_1` (`0xb003`).

That matches the SP11 SoundWire master table: master port 13 has an interval
of 800 clocks at 19.2 MHz, which is 24 kHz.

ACDB does **not** contain `PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) or
`PARAM_ID_CPS_LPASS_SWR_THRESHOLDS_CFG` (`0x08001254`). This is expected, not
missing parser output. The recovered AudioReach header defines `0x08001259`
as an HLOS-provided runtime payload. In SoundWire mode it contains:

- packed voltage and temperature register addresses for every speaker;
- LPASS SoundWire write-command and read-command physical addresses; and
- the LPASS read-FIFO physical address.

This explains why static ACDB reconstruction reached the DSP endpoint but
could not determine whether one amplifier sources CPS, both use native mask
`0x3` with different offsets, or Windows assigns another non-overlapping
transport arrangement.

## What the Linux experiments proved

The dedicated CPS-Lab boot successfully started the DSP graph and prepared
48/8/24 kHz render, VISENSE, and CPS backends. It was nevertheless rejected:

- two amplifiers with split CPS masks `0x1`/`0x2` produced repeated SoundWire
  bus-clash alerts;
- left-only mask `0x1` also clashed;
- left-only native mask `0x3` was clean in its bounded test window;
- the right-only result was contaminated by retained controller state and is
  not evidence for or against a particular Windows layout.

Therefore neither split masks nor simple Linux master-port de-duplication is a
safe answer. Two slave devices can still drive the same physical slots even
if the master-side software table contains only one entry.

## MAX34417 disposition

The Gemini claim that PA05 is an amplifier-current monitor is false. Recovered
Surface ACPI places PA05 at I2C address `0x12` and labels its channels as
memory, Wi-Fi, and CPU rails. The other MAX34417 nodes likewise describe
platform rails; none names a speaker-amplifier rail. Live Linux probes also
received NACK from all five optional devices.

MAX34417 could be useful for broad platform-power experiments if the devices
are present in another firmware state, but it cannot identify which speaker
is drawing current or whether WSA protection has fired. WSA8845 local status,
VI feedback, and DSP telemetry are the relevant sources.

## Current-boot observation

Controlled playback on the clean boot selected the established 48 kHz render
and 8 kHz VISENSE transports. Both amplifier PA state machines became active;
their error-condition registers remained zero, no bus-clash or PA-fault log
appeared, and `CPS_CTL` remained zero. Raw ADC, voltage, and temperature words
were readable, but they must not be treated as calibrated live current values.
The kernel temperature interface returns a cached value while the PA is on.
After testing, both left and right CPS mixer flags were returned to `off`;
PBR and VISENSE remain enabled on both amplifiers.

`tools/sp11-wsa-live-observer.sh` now records these registers without writing
anything or changing playback. It is an observer, not the missing CPS feed.

## Exact next evidence needed

The next Windows runtime capture should use `kd-mcp` as the only debugger
owner and save raw output while protected speaker playback is active. It must
capture both sides of one boundary:

1. WSA master port 13 active-bank transport registers at controller base
   `0x06b10000`; and
2. the runtime `0x08001259` payload or the equivalent per-slave DP6
   configuration for both WSA8845 devices.

A master-port snapshot alone is insufficient because it cannot identify
which slave drives the slots. The `qcaucd` binary contains generic physical
MMIO read/write helpers, but the reviewed routines do not expose SoundWire
slave DP6 registers. A capture is complete only when it preserves the two
amplifiers' identities, packed register addresses, channel/offset allocation,
active bank, and scenario timestamps.

Until that evidence exists, Linux remains on the clean boot and no reboot is
required.
