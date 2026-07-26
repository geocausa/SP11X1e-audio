# SP11 has one WSA macro and one two-channel VI backend

## Result

The Surface Pro 11 MSHW0486 audio design has one enabled WSA SoundWire macro,
not separate left and right WSA/WSA2 macros. Two WSA8845 slave devices share
that one macro and one SoundWire master. The old recovered proposal to add two
VI links and choose between `TX0+TX1` and `TX1+TX2` was built from a
four-amplifier/two-macro model and does not apply to this machine.

The correct Linux structural target is one two-channel VI backend for the
existing WSA macro. In Linux naming, the high-confidence counterpart of the
existing `WSA_CODEC_DMA_RX_0` playback backend is:

```text
WSA_CODEC_DMA_TX_0
  -> lpass_wsamacro DAI 2 (wsa_macro_vifeedback)
  -> two active VI speaker slots
```

This Linux DAI-name selection is a cross-layer implementation deduction, not a
literal name recovered from Windows. It must receive one muted, instrumented
Linux boot test before deployment. The one-macro and two-speaker facts
themselves are exact.

## Hash-bound evidence

| Source | SHA-256 | Role |
|---|---|---|
| Windows `surface_aucdext8380.inf` | `eae4bc6c98288f7e5a4ca793655d1072b16cf8b97cb352606b63b778d65c2402` | MSHW0486 WSA macro count, interface, instance and interrupt |
| Linux `x1-microsoft-denali.dtsi` | `9de46d6628c224324b37073dbcbe1ae528c2a02f9e32ae06845e36df3b4cbcd9` | One WSA macro, one SoundWire master, two WSA8845 slaves and exact port maps |
| Linux `lpass-wsa-macro.c` | `8636964acd1e1d2c9c1691dfa005c417efa5d4085268e6217396618ca133ce35` | Single VI DAI and its two speaker-slot controls |
| Linux `wsa884x.c` | `1a642229730fb47c69013df93aef3bf7b11d8ea672925e6d8fa5e551a1f630e3` | Six slave ports, masks and SoundWire stream construction |
| Linux SoundWire `qcom.c` | `6dea347565244472ff25c641672ac30c13cb47258cd7f272ada2b6b5b9f754d9` | Static master-port allocation and duplicate handling |
| Linux `x1e80100.c` | `75d5dc2f56310c0dd34137c36e70668d7c05fab960e9c1ce4f18ec5dab00f361` | Current playback channel map and missing WSA TX handling |
| Linux `q6afe.c` | `77ea51ffe6ed5dbc8f6f431e9a721ee00e598555aa5b2bd1f3d2828b743c0ff4` | Paired WSA codec-DMA RX0/TX0 port identities |
| Installed SP11 UCM | `956bfdcb273b08a5e981cde0c2ccaa9a536d888370b7a32db13f7b1dd942a9ec` | Current PBR-on, VISENSE-off policy |

The Windows root graph, exact endpoint formats, and two-speaker proof are
bound separately in:

- `artifacts/reviewed/windows-root-codec-dma-hwif.json`;
- `artifacts/reviewed/windows-inf-speaker-vi-formats.json`;
- `artifacts/reviewed/windows-qgpr-root-protection-cfg.json`.

## Exact Windows lower-layer shape

For MSHW0486, the installed AUCD extension INF says:

```text
NumSlaveTypes       = 1
SlaveType           = 0  (WSA)
SlaveInterface      = 2  (SoundWire)
SlaveInstance       = 0
SlaveConnectionType = 1  (internal boost)
SlaveEnableStatus   = 1
NoofInterrupts      = 1
InterruptID         = 3  (SWR_WSA)
```

The same INF documents interrupt ID 4 as `SWR_WSA2`, but MSHW0486 does not
install it. This is direct Windows policy evidence for one WSA macro instance.
`SlaveDevicesAttached=0` is not an amplifier-count claim: the devices are
enumerated dynamically on SoundWire.

The live Windows AudioReach graph independently contains:

- one root `CODEC_DMA_SINK` on WSA interface 1 for 48 kHz/16-bit/two-channel
  render;
- one root `CODEC_DMA_SOURCE` on the same WSA interface 1 for
  8 kHz/32-bit/two-channel VI;
- exactly two protected speakers;
- SP_VI order `[speaker 1 V, speaker 1 I, speaker 2 V, speaker 2 I]`.

There is no Windows evidence for a second WSA interface in the selected root.

## Exact Linux physical description and live enumeration

The current board description has one `&swr0` controller and two WSA8845
slaves:

| Linux label | SoundWire address | Prefix | Slave-to-master port map |
|---|---|---|---|
| `left_spkr` | `<0 0>` | `SpkrLeft` | `1 2 3 7 10 13` |
| `right_spkr` | `<0 1>` | `SpkrRight` | `4 5 6 7 11 13` |

The function map is:

| WSA8845 slave port | Function | Left master port | Right master port |
|---:|---|---:|---:|
| 1 | DAC | 1 | 4 |
| 2 | COMP | 2 | 5 |
| 3 | BOOST | 3 | 6 |
| 4 | PBR | 7 | 7 |
| 5 | VISENSE | 10 | 11 |
| 6 | CPS | 13 | 13 |

The Qualcomm allocator appends one master-port entry for every active slave
port. For a static mapping it selects the requested master port directly,
sets that port's bit, appends the entry and increments the entry count. It
does not reject or merge an already-set bit. The SoundWire core then allocates
and copies exactly that number of master-port entries without de-duplication.
Consequently, enabling the same function on both amplifiers produces repeated
master-port entries for shared PBR port 7 and shared CPS port 13. Static
analysis proves the repeated list, but not whether programming the same
master-port registers twice is the cause of a runtime fault.

On the running `7.1.5-sp11+` kernel, sysfs exposes exactly:

```text
sdw-master-1-0
sdw:1:0:0217:0204:00:0
sdw:1:0:0217:0204:00:1
```

There is no second WSA SoundWire master and no third or fourth amplifier.

The current playback link already uses:

```text
WSA_CODEC_DMA_RX_0
  -> left_spkr + right_spkr + swr0 DAI 0 + lpass_wsamacro DAI 0
```

That link and the board `audio-routing` bind WSA speaker output 1 to the Linux
left amplifier label and output 2 to the right label. This proves the current
Linux intended mapping; the missing Windows physical listening observation
means the Windows speaker-1/speaker-2 to physical-left/physical-right binding
remains corroborated rather than independently observed.

## Do not confuse macro slots with backend DAIs

`lpass-wsa-macro.c` exposes one capture DAI:

```text
name      = wsa_macro_vifeedback
DAI id    = WSA_MACRO_AIF_VI (device-tree index 2)
rate      = 8 or 48 kHz
channels  = 1..4
```

Its mixer has two controls:

```text
WSA_SPKR_VI_1 -> internal WSA_MACRO_TX0 speaker slot
WSA_SPKR_VI_2 -> internal WSA_MACRO_TX1 speaker slot
```

Those `WSA_MACRO_TX0/TX1` values are channel bits inside one macro DAI. They
are not the Linux backend DAIs named `WSA_CODEC_DMA_TX_0` and
`WSA_CODEC_DMA_TX_1`. The old recovered work conflated the two namespaces and
then assigned one backend DAI per physical side.

When speaker slot 1 is active, the macro enables its first pair of protection
paths (`CDC_WSA_TX0`/`TX1`). Speaker slot 2 enables the second pair
(`CDC_WSA_TX2`/`TX3`). This is structurally consistent with the Windows SP_VI
order of V/I for speaker 1 followed by V/I for speaker 2 while exposing one
two-channel VI endpoint.

## Why only one Linux backend is required

The Linux AFE port table defines paired WSA codec-DMA ports:

```text
WSA_CODEC_DMA_RX_0 -> AFE 0xb000
WSA_CODEC_DMA_TX_0 -> AFE 0xb001
WSA_CODEC_DMA_RX_1 -> AFE 0xb002
WSA_CODEC_DMA_TX_1 -> AFE 0xb003
```

The SP11 has one WSA macro instance and already selects RX0 for its one
playback backend. Windows selects a sink and a source on the same one WSA
hardware interface. Therefore the minimal Linux counterpart is the paired TX0
backend connected to the same macro's VI DAI, carrying both protected-speaker
slots. Adding TX1 or TX2 would introduce a second backend that neither the
board description nor the selected Windows graph requires.

## Current Linux gaps

The running card has no `WSA_CODEC_DMA_TX_*` capture backend. Its two generic
multimedia capture PCMs can route only to VA/TX codec-DMA inputs.

The current UCM speaker sequence:

- enables PBR for both amplifiers;
- disables VISENSE for both amplifiers;
- leaves both `WSA_AIF_VI Mixer WSA_SPKR_VI_1/2` controls off.

The X1E machine driver also lacks WSA TX cases in:

- backend initialization;
- the 8 kHz/two-channel/32-bit hardware-parameter fixup;
- TX channel-map programming.

The WSA8845 codec exposes only a playback DAI. Its enabled PBR/VISENSE/CPS
SoundWire ports are added to the existing playback SoundWire stream. The new
VI backend therefore belongs to the WSA macro capture DAI only; it must not
invent separate amplifier capture DAIs.

## Implementation boundary

The next offline candidate should contain exactly:

1. one DT DAI link: `WSA_CODEC_DMA_TX_0 -> lpass_wsamacro 2 -> q6apm`;
2. X1E machine-driver support fixed to 8 kHz, 32-bit, two channels with
   FL/FR TX channel mapping;
3. a UCM protection sequence enabling both VISENSE ports and both VI speaker
   slots only when the corrected protection graph is active;
4. temporary SoundWire instrumentation that records final master-port lists,
   especially shared PBR port 7 and the two VISENSE ports 10/11;
5. amplifier mute and conservative gain until graph start, VI telemetry and
   port programming all succeed.

The transport-only portion is now implemented as the offline candidate
`patches/0001-sp11-add-single-wsa-vi-backend.patch`. It applies cleanly to the
7.1.5 source, its ARM64 `x1e80100.o` builds successfully, and the actual SP11
parent target `x1e80100-microsoft-denali-oled.dtb` compiles successfully. A
DTB round trip confirms that the compiled link selects WSA macro DAI 2 and the
single `WSA_CODEC_DMA_TX_0` CPU DAI.

The UCM activation, protection graph and temporary SoundWire instrumentation
remain separate from the transport patch because they can alter live amplifier
state. The observation-only instrumentation is now implemented as
`patches/0002-qcom-soundwire-log-static-port-allocation.patch` and its ARM64
object builds successfully. It reports static-port reuse only when dynamic
debug is explicitly enabled. No live files or boot configuration were changed
for this finding.
