# qcaucd private DP6 boundary runtime closure

Date: 2026-08-11 (Europe/London)

## Follow-up static origin closure

The recommendation below to move above `FUN_14003bf40` has now been completed.
See `docs/findings/2026-08-11-qcaucd-cps-static-port-template-origin.md`.
`FUN_14003ec58` copies fixed qcaucd per-master-port templates into live state;
the selector-5 table contains the exact port-13/14 -> slave-DP6 CPS geometry,
including OffsetCtrl1 `0x00` / `0x19`. Do not repeat the DP6 runtime trace to
answer that state-origin question.

## Result

The strict-Windows private-boundary question below qcadcm is now closed at a
semantic SoundWire data-port layer inside `qcaucd8380.sys`.

Static analysis identifies `FUN_14003bf40` (RVA `0x3bf40`) as the qcaucd
SoundWire data-port programmer. It constructs per-port slave register addresses
such as `port * 0x100 + 0x20`, `+0x22`, `+0x23`, `+0x24`, `+0x25`, `+0x26`,
and `+0x03`, then submits them through `FUN_14003ac60` (RVA `0x3ac60`), a
direct slave-command write primitive.

A read-only runtime breakpoint at the live `+0x3ac60` boundary captured 18 DP6
writes during one ordinary internal-speaker playback cycle. Decoding the packed
argument reproduces the previously established Windows CPS layout exactly:

- logical device 2 / left WSA8845: active DP6 ChannelEnable `0x03`, OffsetCtrl1
  `0x00`;
- logical device 1 / right WSA8845: active DP6 ChannelEnable `0x03`, OffsetCtrl1
  `0x19`;
- both use SampleCtrl1 `0x1f`, SampleCtrl2 `0x03`, HCtrl `0xff`, BlockCtrl1
  `0x18`, BlockCtrl3 `0x00`;
- teardown writes zero to banked ChannelEnable registers for both devices.

This is stronger semantic evidence than observing only the final physical FIFO
helper: qcaucd itself is seen assigning standard SoundWire DP6 slave registers
before the command is lowered to the controller transport. No direct debugger
physical-MMIO read was used.

The public Qualcomm parameter literal `PARAM_ID_CPS_LPASS_HW_INTF_CFG`
(`0x08001259`) was still not observed. This finding closes the private Windows
per-slave DP6 programming boundary, not the literal public parameter-ID search.

## Binary/hash gate

Reviewed driver:

- `qcaucd8380.sys`
- SHA-256 `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`
- source copy:
  `C:\Users\SurfacePro7\Documents\blobs\sp11-driverdump\qcaucd8380.inf_arm64_53bcc309a68aba55\qcaucd8380.sys`

The existing analyzed Ghidra project was reused:

- `C:\Users\SurfacePro7\AppData\Local\Temp\GhidraQCAUCD\qcaucd.gpr`
- program `/qcaucd8380.sys`.

## Live module-base recovery and RVA validation

On this boot, KD's loader enumeration was not trustworthy: `lm` displayed only
`nt` even while the target-side service/device state showed the Qualcomm audio
stack running normally.

The qcaucd image base was therefore recovered independently on SP11 through a
read-only user-mode `NtQuerySystemInformation(SystemModuleInformation)` query:

- qcaucd live base: `0xfffff80329b70000`;
- image size: `0x5d000`;
- reported load-order index: 237.

The resulting live RVAs were **not** trusted blindly. Each runtime boundary was
validated against normal kernel virtual-memory disassembly in KD. No physical
address was read.

### Direct slave helper `+0x31188`

Live VA `0xfffff80329ba1188` matched the static helper. Its entry copies:

- `x0 -> x20`;
- `w1 -> w21`;
- byte-truncated `w2 -> w19`.

A bounded DP6-range logging breakpoint at this entry produced zero DP6 hits
during one internal-speaker playback cycle. This helper is therefore not the
path used by the data-port programmer in the tested cycle.

### Generic lower wrapper `+0x3e850`

Live VA `0xfffff80329bae850` matched `FUN_14003e850`. Static analysis shows the
packed register in low 32 bits of argument 1 and its byte value in bits 32..39.
A bounded DP6-range logging breakpoint here also produced zero DP6 hits during
one internal-speaker playback cycle.

This negative witness was important because it forced the static analysis one
layer deeper instead of repeating the same experiment.

### Direct slave command primitive `+0x3ac60`

Live VA `0xfffff80329baac60` matched `FUN_14003ac60`. The entry sequence includes:

```text
uxtb w23,w0
mov   x20,x1
mov   w22,w2
ubfx  x24,x20,#0x20,#8
```

Together with the static decompile, the runtime argument contract is:

- `w0` low byte: logical SoundWire slave device number;
- low 32 bits of `x1`: slave register address;
- bits 32..39 of `x1`: byte value;
- `w2`: controller/index context.

This boundary was the successful runtime trace point.

## Static data-port programmer: `FUN_14003bf40`

Fresh decompilation of RVA `0x3bf40` shows qcaucd iterating enabled SoundWire
ports and constructing standard per-data-port register addresses. The routine
calls `FUN_14003ac60` directly with addresses built from the port number,
including:

- `port * 0x100 + 0x20`;
- `port * 0x100 + 0x22`;
- `port * 0x100 + 0x23`;
- `port * 0x100 + 0x24`;
- `port * 0x100 + 0x25`;
- `port * 0x100 + 0x26`;
- `port * 0x100 + 0x03`.

For data port 6 these expressions are the observed `0x0620`, `0x0632`,
`0x0633`, `0x0634`, `0x0635`, `0x0636`, and `0x0603` families. The routine
also emits the banked ChannelEnable/BlockCtrl3 controls observed at `0x0630`
and `0x0637` through the same slave-command primitive.

This explains the two zero-hit wrapper traces: the data-port configuration path
bypasses `+0x31188` and `+0x3e850` and calls `+0x3ac60` directly from the
`+0x3bf40` programmer.

Static decompile evidence outside Git:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-3bf40.txt`
- size 9,925 bytes;
- SHA-256 `984E58423BCD56BFE3B2937E620EF223C8DA46C8B5B7D26F2669FACB09E5998C`.

## Runtime stimulus

The default multimedia render endpoint was independently checked on SP11 before
playback. It resolved to the internal Qualcomm `Speakers` endpoint, not the
paired Galaxy Buds endpoint.

Stimulus:

- `C:\Windows\Media\Alarm01.wav`;
- synchronous `System.Media.SoundPlayer.PlaySync()`;
- ordinary Windows internal-speaker playback through the normal effects path.

The successful trace used one read-only logging breakpoint at
`0xfffff80329baac60`, filtered to register addresses `0x0600..0x063f`, with
immediate `gc` and a 256-hit self-disable ceiling.

## Directly observed DP6 writes

The raw KD breakpoint text contains a `val=` field whose pseudo-register
formatting was not reliable. The `packed=` argument is authoritative. Every
value below is decoded as `(packed >> 32) & 0xff`.

| # | Logical device | Register | Decoded value | Controller/index | Packed argument |
|---:|---:|---:|---:|---:|---|
| 1 | 2 | `0x0630` | `0x03` | 2 | `0xfffff80300000630` |
| 2 | 2 | `0x0632` | `0x1f` | 2 | `0xfffff81f00000632` |
| 3 | 2 | `0x0633` | `0x03` | 2 | `0xfffff80300000633` |
| 4 | 2 | `0x0634` | `0x00` | 2 | `0xfffff80000000634` |
| 5 | 2 | `0x0636` | `0xff` | 2 | `0xfffff8ff00000636` |
| 6 | 2 | `0x0603` | `0x18` | 2 | `0xfffff81800000603` |
| 7 | 2 | `0x0637` | `0x00` | 2 | `0xfffff80000000637` |
| 8 | 1 | `0x0630` | `0x03` | 2 | `0xfffff80300000630` |
| 9 | 1 | `0x0632` | `0x1f` | 2 | `0xfffff81f00000632` |
| 10 | 1 | `0x0633` | `0x03` | 2 | `0xfffff80300000633` |
| 11 | 1 | `0x0634` | `0x19` | 2 | `0xfffff81900000634` |
| 12 | 1 | `0x0636` | `0xff` | 2 | `0xfffff8ff00000636` |
| 13 | 1 | `0x0603` | `0x18` | 2 | `0xfffff81800000603` |
| 14 | 1 | `0x0637` | `0x00` | 2 | `0xfffff80000000637` |
| 15 | 2 | `0x0620` | `0x00` | 2 | `0xfffff80000000620` |
| 16 | 1 | `0x0620` | `0x00` | 2 | `0xfffff80000000620` |
| 17 | 2 | `0x0630` | `0x00` | 2 | `0x0000000000000630` |
| 18 | 1 | `0x0630` | `0x00` | 2 | `0x0000000000000630` |

No `0x0631` or `0x0635` write was observed in this cycle.

The assignments agree exactly with the earlier final FIFO capture and with the
known identities:

- logical device 2 = left WSA8845 `0x0000000402170220`;
- logical device 1 = right WSA8845 `0x0000000402170221`.

## What this closes

The Windows CPS/SoundWire reconstruction now has both ends of the private
transport chain:

1. qcaucd semantic data-port state is converted into per-slave DP6 register
   commands by `FUN_14003bf40`;
2. those register/value commands cross the direct slave primitive
   `FUN_14003ac60` with the exact left/right DP6 schedule;
3. the already-reviewed lower FIFO capture shows those commands being packed
   and submitted to the SoundWire controller.

The private per-slave dataport programming path therefore no longer needs more
playback tracing.

If strict Windows archaeology continues solely to discover the semantic origin
of the missing public `0x08001259` contract, the next new layer must be **above
`FUN_14003bf40`**: identify the caller/state-population path that fills the
per-port data structure consumed by the programmer. Do not return to the closed
qcadcm SET_CFG/query/event experiments and do not repeat the already-closed
`+0x31188`, `+0x3e850`, `+0x3ac60`, or physical-FIFO traces.

For Linux parity work, the established implementation target remains:

- CPS on WSA DP6;
- ChannelEnable/mask `0x03` on both speakers;
- left OffsetCtrl1 `0`;
- right OffsetCtrl1 `25`;
- sample interval 800 clocks / 24 kHz endpoint;
- normal SoundWire/WSA port-parameter paths, not MMIO programming.

## Evidence and debugger safety

Raw runtime log outside Git:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\QCAUCD_DP6_HELPER_20260811_0001BST.log`
- size 14,257 bytes;
- SHA-256 `34C6BD115CB83B0A747F9BD4E4120FA1B53FF6171514D78F159162225CEBE568`.

Safety/closeout:

- a single `kd.exe` owner was verified before attach;
- no classic WinDbg/cdb/ntsd owner was present;
- no direct debugger physical-MMIO read was performed;
- no debugger MMIO write was performed;
- no DSP write was performed;
- no SoundWire slave-register or driver-state write was performed by the debugger;
- only normal kernel virtual disassembly plus bounded read-only logging
  breakpoints were used;
- breakpoints were cleared with `bc *`;
- log closed with `.logclose`;
- debugger detached with `qd`;
- SP7 was verified afterward to have zero `kd` processes and zero running
  PiMaster jobs.
