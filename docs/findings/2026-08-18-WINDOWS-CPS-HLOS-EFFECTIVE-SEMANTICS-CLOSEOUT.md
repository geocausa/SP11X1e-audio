# Windows CPS HLOS effective-semantics closeout

Date: 2026-08-18  
Status: **GREEN effective HLOS CPS semantics for the reviewed SP11 Windows build**

## Question

Ledger item P10 remained AMBER because the project originally expected to recover and
name every field of the public Qualcomm `PARAM_ID_CPS_LPASS_HW_INTF_CFG`
(`0x08001259`) payload.

Later runtime and static evidence changes the correct question.  On the hash-locked
SP11 Windows audio stack, that public payload is not the HLOS source of the WSA8845
CPS SoundWire geometry.

## Negative public-parameter evidence

Hash-locked qcadcm:

- `qcadcm8380.sys`
- SHA-256 `37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429`

At the common GPR `APM_CMD_SET_CFG` boundary:

- 74 recognized SET_CFG submissions were inventoried;
- 135 complete mapped payload buffers, up to `0x28e0` bytes, were searched end-to-end;
- no `0x08001259` literal was found;
- the repeated `0xb18` graph-open OOB payload contains `INTENT_ID_CPS`
  (`0x08001537`) but not `0x08001259` or `0x08001254`;
- the CPS source IID `0x402b` does not appear in the observed qcadcm SET_CFG headers.

This establishes absence at the reviewed Windows qcadcm graph/configuration boundary;
it is not a claim about all Qualcomm products or firmware generations.

## Immediate Windows HLOS source

Hash-locked qcaucd:

- `qcaucd8380.sys`
- SHA-256 `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`

Static reconstruction closes the immediate source chain:

`WSA8845 SoundWire identity`
-> `FUN_140031430` identity-family classifier
-> class `0x50000`, template selector `5`
-> runtime registration
-> `FUN_14003ec58` selector-5 template copy
-> `FUN_14003df18`
-> `FUN_14003bf40` data-port programmer
-> `FUN_14003ac60` slave-command primitive
-> live DP6 register writes.

Both SP11 speaker identities match the WSA8845-family mask
`20 02 17 02 04 00`; qcaucd locally creates selector `5`.  No external CPS
payload is required to choose this table.

## Selector-5 effective payload

The two 16-byte image-backed template entries are:

- slot 13 / master-port-13 + left slave:
  `0d 06 00 00 03 00 1f 03 00 ff 0f 0f 18 00 ff ff`
- slot 14 / right-slave companion:
  `0e 06 00 00 03 00 1f 03 19 ff 0f 0f 18 00 ff ff`

The fields that affect the closed live DP6 programming path are semantically bound:

| Byte(s) | Effective role | Left | Right |
|---|---|---:|---:|
| 0 | qcaucd software state slot | 13 | 14 |
| 1 | slave SoundWire data port | 6 | 6 |
| 2 | logical slave-ID placeholder, overwritten at runtime | dynamic | dynamic |
| 4 | DP6 ChannelEnable / mask | `0x03` | `0x03` |
| 6 | DP6 SampleCtrl1 | `0x1f` | `0x1f` |
| 7 | DP6 SampleCtrl2 | `0x03` | `0x03` |
| 8 | DP6 OffsetCtrl1 | `0x00` | `0x19` |
| 9 | optional OffsetCtrl2; `0xff` suppresses write | suppressed | suppressed |
| 10/11 | HCtrl HStart/HStop nibbles | `0x0f/0x0f` | `0x0f/0x0f` |
| 12 | DP6 BlockCtrl1 | `0x18` | `0x18` |
| 13 | DP6 BlockCtrl3 | `0x00` | `0x00` |
| 14/15 | optional controls; `0xff` suppresses writes | suppressed | suppressed |

Template byte 3 is not copied by the state-population routine. Byte 5 is copied as an
adjacent state byte but produces no DP6 write in the closed runtime path.  Neither is
an unknown live transport parameter that Linux must guess.

Slot 14 is deliberately **not** physical master port 14.  `FUN_14003bf40` skips the
master-port programming block for software slot `0x0e` while still programming the
right slave.  The effective topology is therefore one shared physical master port 13
feeding two independently configured slave DP6 endpoints.

## Runtime confirmation

The read-only qcaucd slave-command trace observed exactly:

Left / logical device 2:

- `DP6 ChannelEnable=0x03`
- `SampleCtrl1=0x1f`
- `SampleCtrl2=0x03`
- `OffsetCtrl1=0x00`
- `HCtrl=0xff`
- `BlockCtrl1=0x18`
- `BlockCtrl3=0x00`

Right / logical device 1:

- same fields, except `OffsetCtrl1=0x19`.

Both teardown ChannelEnable writes were zero.  No DP4 positive scheduling occurred in
the retained ordinary-playback Windows FIFO capture.

## Linux parity binding

The accepted CPS-v3 Linux lineage implements the same effective contract through normal
SoundWire/ASoC mechanisms:

- one shared physical WSA master port 13;
- 24 kHz CPS transport / 800-clock interval;
- WSA8845 slave DP6 source on both speakers;
- native DP6 channel mask `0x03` on both speakers;
- left slave OffsetCtrl1 `0`;
- right slave OffsetCtrl1 `25` (`0x19`);
- writable DP6 BlockCtrl1 and Windows-proven SIMPLE transport fields;
- HStart/HStop `0x0f/0x0f`;
- VI and CPS readiness independently gate protected SP/SPVI startup.

The accepted implementation is represented by patches `0032` through `0039`; the
rejected split-mask CPS-Lab model remains rejected.

## P10 conclusion

For this exact SP11 Windows build, **effective HLOS CPS semantics are closed**.  The
project no longer has a meaningful parity requirement to discover or synthesize the
public `0x08001259` body because the reviewed HLOS stack sources its effective CPS
SoundWire state locally from qcaucd identity-selected static templates.

This does not claim that `0x08001259` is unused on all Qualcomm platforms, and it does
not claim calibrated dynamic limiter telemetry parity (P09).  P09 remains a separate
observability item and is already proven non-blocking for speaker rendering.

Primary prior findings:

- `docs/findings/2026-08-10-qcadcm-common-gpr-cps-runtime.md`
- `docs/findings/2026-08-11-qcaucd-dp6-private-boundary-runtime.md`
- `docs/findings/2026-08-11-qcaucd-cps-static-port-template-origin.md`
- `docs/findings/2026-08-11-qcaucd-slot14-shared-master13-correction.md`
- `docs/findings/2026-08-11-qcaucd-selector5-soundwire-identity-origin.md`
- `docs/findings/2026-08-11-linux-cps-v3-live-wsa-observation.md`
