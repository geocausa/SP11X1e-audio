# Full qcaucd FIFO decode: Windows DP5/VISENSE gap

Date: 2026-08-14 (Europe/London)

> **Later 2026-08-14 scope correction:** this finding correctly decoded the
> 328 `CODEX_DP6BRIDGE` data-port/lifecycle records and led to the successful
> DP5 fix, but it did not parse the separate `CODEX_QCAUCD_V2CMD` amplifier
> initialization block in the same retained runtime log. Therefore its claim
> that the complete WSA profile was closed was too broad. The second decoder
> pass and patch `0052` are documented in
> `2026-08-14-WINDOWS-WSA8845-INIT-PARITY-CORRECTION.md`.

## Result

The user was correct that the retained Windows logs had already been copied to
the SP11 Linux installation.  The exact Aug-10 files are under
`/home/geoca/Documents/SP11-PROJECT/01-audio/11.08.2026/`, and their hashes
match the previously reviewed Windows-side evidence.

Decoding the complete 328-record command-FIFO log—not only its previously
reviewed DP6 subset—found one concrete Windows/Linux hardware-transport
mismatch:

- Windows programs WSA8845 DP5/VISENSE `ChannelEnable = 0x03` on **each**
  amplifier;
- the running Linux WSA884x driver requests DP5/VISENSE `ch_mask = 0x01` on
  each amplifier.

Windows repeats the same complete 109-command amplifier start/stop transaction
three times.  The mismatch is therefore repeated runtime evidence, not an
unused static capability or a single ambiguous register sample.

This is now the strongest evidence-backed downstream candidate for incomplete
speaker-protection behavior and the remaining physical tonal difference.  It
does not prove acoustic causality until a separately bootable Linux candidate
is validated.

## Evidence recovered locally

| Evidence | Size | SHA-256 |
|---|---:|---|
| `CPS_DP6_SLAVES_...660.log` | 48,229 | `a6bbf3574e6caaf5fdb0fc46ec4bad0106321af90cce91fbeb4f2015b60b66eb` |
| `CPS_SWR_RUNTIME_...880.log` | 139,427 | `ee8cb66eb3d7a44bf7fe4aadd61f04bb29ba85520df5e15e5ded95d2c1b3dc36` |
| `qcaucd-port-templates.txt` | 4,463 | `5d81834f2c9efdd92fa54ff07f22fcde394cfd3781ea25ab022903e8dd15f4ed` |

The reusable decoder is
`tools/decode_qcaucd_command_fifo.py`.  Its compact decode reports:

```text
records: 328
cycle starts: 0, 109, 218
cycle lengths: 109, 109, 109
payload-identical cycles, ignoring rotating command ID: yes
positive ChannelEnable: DP1, DP2, DP3, DP5 and DP6 on both devices
positive ChannelEnable on DP4: none
```

The final 328th record is a broadcast `0x0044=0x02` after the three cycles.

## Complete Windows slave-port matrix

Logical device 2 is WSA identity `0x0000000402170220` / left.  Logical device
1 is `0x0000000402170221` / right.

| WSA port | Function | Windows mask, both | Sample interval field | Offset1 left/right | Other observed fields |
|---:|---|---:|---:|---:|---|
| 1 | DAC | `0x01` | `0x07` | `0x01` / `0x02` | BlockCtrl3 `0x00` |
| 2 | COMP | `0x0f` | `0x1f` | `0x03` / `0x04` | Offset2 `0x07`, BlockCtrl3 `0x01` |
| 3 | BOOST | `0x03` | `0x3f` | `0x05` / `0x15` | Offset2 `0x1f`, BlockCtrl3 `0x01` |
| 4 | PBR | no positive write | — | — | right DP4 bank-1 teardown `0x00` only |
| 5 | VISENSE | `0x03` | `0x0f` | `0x06` / `0x0d` | BlockCtrl3 `0x01` |
| 6 | CPS | `0x03` | `0x031f` | `0x00` / `0x19` | HCtrl `0xff`, BlockCtrl1 `0x18`, BlockCtrl3 `0x00` |

The preceding master-controller runtime log independently records the active
VISENSE tuples for physical master ports 10 and 11 as:

```text
master port 10 / left:  0f 06 00 03
master port 11 / right: 0f 0d 00 03
```

Those bytes match qcaucd selector-5 static templates at RVAs `0x15c10` and
`0x15c20` exactly.  Thus DP5 mask `0x03` is supported by three independent
forms of the retained evidence: static template, master runtime programming
and slave runtime programming.

## Windows amplifier transaction versus Linux

The same Windows FIFO capture also contains the actual WSA register writes
around the port transaction.  On both amplifiers Windows sets:

- `VSENSE1 0x3020 = 0x67` and `ISENSE2 0x3021 = 0x07`;
- interrupt masks `0x3581/2 = 0x90/0x00`;
- class-H `0x34d0 = 0x67`;
- power-stage/high-side sequence `0x3067/0x304d = 0x08/0x52`;
- `PA_FSM_EN 0x3430 = 0x01`;
- then `0x3067/0x304d = 0x0c/0x5a`.

The running `7.1.5-sp11-render-parity-v2+` regmap matches `0x3020=67`,
`0x3021=07`, `0x304c=f6`, `0x3091=44`, and the exact 15 PBR thresholds.
Current kernel logs also show the exact Windows masks for DP1/2/3 and DP6, but
explicitly showed `num=5 ch-mask=0x1` for DP5 on both amplifiers. A later
state-aware comparison showed Linux did not reproduce the final
`0x3067=0x0c / 0x304d=0x5a` PA-start restoration and reopened the broader
codec initialization scope.

The Linux DP5 mismatch is already present in the current source:

```c
[WSA884X_PORT_VISENSE] = {
        .num = WSA884X_PORT_VISENSE + 1,
        .ch_mask = 0x1,
},
```

The older WSA881x upstream driver is a useful implementation precedent: it
declares a one-channel SIMPLE-port capability and stream configuration while
using VISENSE `ch_mask = 0x3`.  That shows the two-bit native port mask does not
by itself require inventing a four-channel AudioReach endpoint.  The SP11 fix
should nevertheless be board-scoped rather than changing every WSA884x user.

## PBR DP4 is now closed for this scenario

The selector-5 static qcaucd table contains a valid DP4/PBR schedule, but the
three captured playback cycles contain no positive DP4 ChannelEnable or other
DP4 programming.  They contain only one right-slave `0x0430=0x00` teardown per
cycle.  Ordinary Linux playback also leaves DP4 unscheduled while retaining
the exact internal 2-cell PBR/current-limit policy.

Therefore the correct parity action is **not** to enable DP4.  The retained
runtime evidence resolves the earlier capability-versus-use ambiguity for the
captured playback scenario.

## Candidate implementation and build gate

Patch `0050-ASoC-wsa884x-denali-use-native-VISENSE-channel-mask.patch`
implements a Denali/SP11-scoped WSA884x VISENSE mask override of `0x03`
through the normal SoundWire port configuration.  Machines without the new
property retain the generic `0x01` default.

The modified WSA884x object and complete module linked successfully, and the
Denali OLED DTB compiled successfully, against the current
`7.1.5-sp11-render-parity-v2+` build tree.  The patch also reverse-applies
cleanly to that source state.

An isolated one-shot candidate is now installed as
`sp11-audio-visense-parity`.  It reuses the live-gated Render-Parity v2 kernel
and archive closure, replaces only the signed WSA884x module, and applies the
two properties as an overlay to the already-proven combined Phase91/Wi-Fi/OLED
DTB.  A decompiled DTB comparison proves those are the only two platform-tree
changes.  The persistent saved entry remains unchanged.  Live acceptance is
still gated on:

1. both DP5 slave requests reporting mask `0x03` with offsets `6` and `13`;
2. master ports 10/11 allocating without a bus clash;
3. the 8 kHz / S32 / two-channel WSA macro backend and SP/SPVI graph starting;
4. no PA fault or XRUN during bounded moderate-volume playback;
5. an operator Windows-parity listening comparison.

No new Windows session is needed for this change.

## Corrected live result

The first one-shot was rejected as a false candidate because inclusion without
early activation allowed the old root WSA884x module to load; its DP5 remained
`0x01`, as expected.  The rebuilt initramfs explicitly force-loads the signed
candidate.  The corrected boot reported loaded srcversion
`782FC79EBBA505E52A2AE88` and DP5 `ch-mask=0x3` on both amplifiers at 8 kHz.

VI and CPS feedback became ready, SP/SPVI enable and `GRAPH_START` were
accepted, and a bounded PipeWire/Dolby playback pass completed at the existing
41% user volume with zero logged PA faults, XRUNs, SoundWire port conflicts or
timeouts.  Both amplifiers returned to runtime suspend.  Thus the proven DP5
transport mismatch is closed; subjective tonal improvement remains an operator
listening gate.

Machine-readable result:
`artifacts/reviewed/2026-08-14-windows-qcaucd-full-fifo-vs-linux.json`.
