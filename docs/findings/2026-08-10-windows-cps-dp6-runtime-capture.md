# Windows CPS / WSA DP6 runtime capture

Date: 2026-08-10 (Europe/London)

## Result

The Windows runtime transport question that blocked the Linux CPS work is now
resolved at the per-slave SoundWire level.

Windows does **not** split CPS channel masks `0x1` / `0x2` between the two
WSA8845 amplifiers. During three repeated playback initializations, both
amplifiers were programmed on SoundWire data port 6 with native channel enable
`0x03`. The per-speaker distinction is the DP6 timing offset:

- WSA `0x0000000402170220` / left: `OffsetCtrl1 = 0x00`;
- WSA `0x0000000402170221` / right: `OffsetCtrl1 = 0x19` (25).

This directly falsifies the rejected split-mask Linux candidate and identifies
the transport property that Linux must preserve if a later CPS candidate is
built.

This finding closes the **per-slave DP6 transport** question. It does not claim
that `PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) itself was captured, and it
does not claim that the strict handoff's idle/active/post-stop current-bank
snapshot sub-gate was completed. Direct physical debugger MMIO reads were not
repeated after they had already proven unsafe on this target.

## Runtime evidence observed directly

### Hash gate

The exact handoff-locked binaries were verified before RVA tracing:

- `qcadcm8380.sys`: `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`
- `qcaucd8380.sys`: `bd0c8276c51fc7a020c616e904dd613b6ccf187ec3e1fe6f94c2c811c8adc8bf`

### WSA slave identities

A prior qcaucd runtime trace preserved the SoundWire enumerator pairs:

- logical slot 1: low `0x02170221`, high `0x00000004`;
- logical slot 2: low `0x02170220`, high `0x00000004`.

The resulting 64-bit identities are therefore:

- logical device 1: `0x0000000402170221`;
- logical device 2: `0x0000000402170220`.

### Command FIFO boundary

The successful capture placed one read-only logging breakpoint at the
hash-locked `qcaucd8380.sys + 0x1bf80` helper and restricted logging to writes
to the WSA command FIFO at physical `0x06b15020`.

No debugger MMIO write was performed. The breakpoint only displayed the four
bytes already being submitted by the Windows driver and immediately continued.

The packed command words decode as:

```text
packed = reg | (cmd_id << 16) | (logical_dev << 20) | (data << 24)
```

The successful session produced 328 decoded FIFO writes. Three playback
initializations repeated the same CPS DP6 programming.

### Logical device 1 — `0x0000000402170221`

Observed active DP6 programming:

| Register | Value |
|---|---:|
| `0x0630` ChannelEnable | `0x03` |
| `0x0632` SampleCtrl1 | `0x1f` |
| `0x0633` SampleCtrl2 | `0x03` |
| `0x0634` OffsetCtrl1 | `0x19` |
| `0x0636` HCtrl | `0xff` |
| `0x0603` BlockCtrl1 | `0x18` |
| `0x0637` BlockCtrl3 | `0x00` |

Teardown also wrote ChannelEnable `0x00` to both `0x0620` and `0x0630`.

No write to `0x0631` BlockCtrl2 or `0x0635` OffsetCtrl2 was observed in this
capture.

### Logical device 2 — `0x0000000402170220`

Observed active DP6 programming:

| Register | Value |
|---|---:|
| `0x0630` ChannelEnable | `0x03` |
| `0x0632` SampleCtrl1 | `0x1f` |
| `0x0633` SampleCtrl2 | `0x03` |
| `0x0634` OffsetCtrl1 | `0x00` |
| `0x0636` HCtrl | `0xff` |
| `0x0603` BlockCtrl1 | `0x18` |
| `0x0637` BlockCtrl3 | `0x00` |

Teardown likewise wrote ChannelEnable `0x00` to both `0x0620` and `0x0630`.

No write to `0x0631` BlockCtrl2 or `0x0635` OffsetCtrl2 was observed.

### Sample interval

The observed pair `SampleCtrl2:SampleCtrl1 = 0x03:0x1f` forms `0x031f`.
With the SoundWire `sample_interval - 1` encoding, this is an interval of 800
bus clocks, matching the independently reconstructed 24 kHz CPS endpoint at a
19.2 MHz SoundWire clock.

### Master port 13 evidence from the preceding qcaucd trace

The preceding runtime trace preserved these Windows driver writes in the WSA
master port-13 window:

```text
0x06b11d24 = 0x0000001f
0x06b11d2c = 0x00000018
0x06b11d34 = 0x000000ff
0x06b11d38 = 0x00000000
0x06b11d3c = 0x00000003
0x06b11d54 = 0x00000003
0x06b11d64 = 0x00ff001f / 0x0300001f during programming transitions
0x06b11d74 = 0x000000ff
0x06b11d78 = 0x00000000
0x06b11d7c = 0x00000003
```

These are traced writes performed by the Windows driver, not debugger-generated
MMIO writes.

## Source-backed interpretation

The following field meanings are interpretation backed by primary upstream or
Qualcomm source, rather than names emitted by the proprietary Windows driver.

Qualcomm's SoundWire controller source resolves a matching enumerator entry by
looping logical device numbers beginning at 1 and returning that loop index as
`dev_num`. This binds the observed FIFO `dev=1` and `dev=2` fields to the two
enumerator identities above.

Linux SoundWire register definitions use a `0x100` stride per data port. For
data port 6, the observed `0x0603`, `0x0620`, and `0x0630` ranges decode as DP6
BlockCtrl1 and banked DP6 data-port controls.

The upstream WSA884x driver defines CPS as the sixth SoundWire port and gives it
native channel mask `0x3`. At the WSA8845 side it is described as a SoundWire
sink port.

Qualcomm's Kalama audio device tree independently gives exactly the same
speaker-specific CPS arrangement recovered from Windows:

- master WSA port 13: CPS mask `0x3`;
- `wsa884x@02170220`, `SpkrLeft`: CPS Offset1 `0`;
- `wsa884x@02170221`, `SpkrRight`: CPS Offset1 `25`.

That source cross-check matches the Windows bytes without needing to guess a
Linux mask or offset.

Primary-source paths used for interpretation:

- Qualcomm: `drivers/soundwire/swr-wcd-ctrl.c`
- Linux: `include/linux/soundwire/sdw_registers.h`
- Linux: `sound/soc/codecs/wsa884x.c`
- Qualcomm: `kernel/msm-extra/audio-devicetree/kalama-audio-overlay.dtsi`

## What this changes

The rejected Linux reconstruction assumed two overlapping native CPS ports had
to be split into masks `0x1` and `0x2`. Windows shows that assumption was wrong.
Both WSA8845 devices retain ChannelEnable `0x3`; their CPS DP6 scheduling differs
by OffsetCtrl1 (`0` versus `25`).

A future Linux candidate should therefore preserve native mask `0x3` for both
CPS DP6 ports and express the per-device offset through the normal
SoundWire/WSA port-parameter path. Direct MMIO programming is neither necessary
nor appropriate for that implementation.

Also note the direction terminology: on the WSA8845 slave, CPS DP6 is a
SoundWire source. It feeds the LPASS/AFE CPS `CODEC_DMA_SOURCE` endpoint through
WSA controller master port 13, which belongs to the controller's DIN range.
The upstream WSA884x declaration that groups CPS with sink ports does not match
this SP11 transport and must not be used to reverse the proven data direction.

## Remaining gap versus the strict KD handoff

The transport evidence is complete enough to answer the per-speaker allocation
question, but two literal handoff items remain unclaimed:

1. the runtime `0x08001259` structure itself was not observed at the captured
   qcadcm SET_CFG boundary, and a broader read-only scan of the installed audio
   package files found no static `0x08001259` literal;
2. the successful no-physical-read session did not directly sample the WSA
   master's current-bank status at idle, active, and post-stop.

The latter was intentionally not forced: direct debugger physical MMIO access
had already caused target failures. The successful session instead preserves
the actual bank-1 DP6 programming commands submitted by the Windows driver and
both-bank ChannelEnable teardown writes.

If the exact `0x08001259` payload or strict current-bank snapshots are still
required, the next observation should follow a natural Windows driver read or
runtime-structure boundary. It should not reintroduce direct physical debugger
MMIO reads.

## Evidence files and hashes

Raw debugger evidence remains outside Git pending any separate privacy/secret
review:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_DP6_SLAVES_20260810_2007BST_25f4_2026-08-10_20-07-15-660.log`
  - size: 48,229 bytes
  - SHA-256: `A6BBF3574E6CAAF5FDB0FC46EC4BAD0106321AF90CCE91FBEB4F2015B60B66EB`
- `C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_DP6_SLAVES_20260810_2007BST-extract.txt`
  - size: 4,182 bytes
  - SHA-256: `D262B3842F9D4620E26DF495EA016412C419010FC24321825978F126ED0662EF`
- preceding master/enumerator runtime log:
  `C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_SWR_RUNTIME_20260810_1909Z_2d7c_2026-08-10_19-09-12-880.log`
  - SHA-256: `EE8CB66EB3D7A44BF7FE4AADD61F04BB29BA85520DF5E15E5DED95D2C1B3DC36`

Machine-readable reviewed extraction:

- `artifacts/reviewed/2026-08-10-windows-cps-dp6-runtime.json`

## Debugger closeout

- no direct debugger physical MMIO read in the successful DP6 session;
- no debugger MMIO/DSP/driver-state write;
- breakpoint cleared with `bc *`;
- log closed;
- detached using `qd`;
- SP7 process check after detach: zero `kd.exe` processes.
