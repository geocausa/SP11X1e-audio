# qcaucd slot-14 correction: shared physical master port 13

Date: 2026-08-11 (Europe/London)

## Correction

The earlier static-template finding correctly recovered two selector-5 state
entries at RVAs `0x15c40` and `0x15c50`, and correctly bound their slave-DP6
payloads to the left and right WSA8845 devices. It **over-interpreted** the
second state-table index as physical SoundWire master port 14.

A direct review of `FUN_14003bf40` (RVA `0x3bf40`) proves that interpretation is
wrong.

The routine iterates software state slots `1..14` using `uVar10`. For every
active slot it can emit the per-slave data-port commands through
`FUN_14003ac60`, but the controller/master-port programming block is explicitly
guarded by:

```text
if (uVar10 != 0xe) {
    ... program master/controller port state ...
}

... program slave data-port registers ...
```

Therefore:

- state slot **13** (`uVar10 == 0x0d`) programs physical master port 13 **and**
  the first WSA slave DP6 payload;
- state slot **14** (`uVar10 == 0x0e`) skips physical master-port programming
  entirely and still programs the second WSA slave DP6 payload.

Slot 14 is thus a **slave-only companion software slot**, not physical master
port 14.

This reconciles all earlier runtime evidence: CPS is a shared physical master
port 13 transport feeding two WSA8845 slave DP6 endpoints.

## Corrected selector-5 entries

Selector-5 table base remains RVA `0x15b70`.

### State slot 13 / physical master port 13 + left slave

Entry RVA `0x15c40`:

```text
0d 06 00 00 03 00 1f 03 00 ff 0f 0f 18 00 ff ff
```

After `FUN_14003ec58` replaces the slave-ID placeholder, this slot is applied to
logical device 2 / left WSA8845 `0x0000000402170220`.

It supplies the observed left slave DP6 state:

- ChannelEnable `0x03`;
- SampleCtrl1 `0x1f`;
- SampleCtrl2 `0x03`;
- OffsetCtrl1 `0x00`;
- HCtrl `0xff`;
- BlockCtrl1 `0x18`;
- BlockCtrl3 `0x00`.

Because this is slot 13, `FUN_14003bf40` also executes its master/controller
programming block for physical master port 13.

### State slot 14 / right-slave companion only

Entry RVA `0x15c50`:

```text
0e 06 00 00 03 00 1f 03 19 ff 0f 0f 18 00 ff ff
```

This slot is applied to logical device 1 / right WSA8845
`0x0000000402170221` and supplies the same DP6 shape except OffsetCtrl1 is
`0x19` (25).

Because `uVar10 == 0x0e`, the routine bypasses the physical master-port
programming block for this slot. Only the slave-side DP6 commands are emitted.

This is the mechanism by which Windows applies asymmetric per-speaker slave
OffsetCtrl1 values while retaining one shared physical CPS master port.

## Linux Phase91 cross-check

The existing reviewed engineering artifact
`artifacts/audio-powerlab-candidate-20260810/x1e80100-microsoft-denali-sp11-audio-powerlab-phase91.dtb`
provides an independent Linux-side consistency check.

DTB SHA-256:

`3A3A1530C23690B7B765A47FADC40730356A405E85B8E5A03DDEDEB3E0462231`

At `/soc@0/soundwire@6b10000` (WSA):

- `qcom,din-ports = <4>`;
- `qcom,dout-ports = <9>`;
- total configured master ports = 13;
- the 13th `qcom,ports-sinterval` entry is `0x031f` (799 register value,
  corresponding to an 800-clock sample interval);
- the 13th `qcom,ports-offset1` entry is `0x00`;
- the 13th HStart/HStop entries are `0x0f` / `0x0f`;
- the 13th word-length entry is `0x18`.

The upstream Qualcomm SoundWire driver parses array element `i` into
`ctrl->pconfig[i + 1]`, so the 13th element is unambiguously physical master
port 13. The current upstream implementation also documents valid master port
numbers as 1..14 while the platform-specific `nports` bound controls which are
available; this Phase91 WSA node configures only 13.

The same DTB maps both speaker CPS endpoints to master port 13:

- `SpkrLeft` `qcom,port-mapping = <1 2 3 7 10 13>`;
- `SpkrRight` `qcom,port-mapping = <4 5 6 7 11 13>`;
- both speaker nodes carry `qcom,enable-cps`.

This is fully consistent with the corrected Windows model: one physical master
port 13 shared by both speakers, with two independently configured slave DP6
endpoints.

Upstream indexing reference used for this cross-check:

- `drivers/soundwire/qcom.c` at upstream Linux commit
  `d58772d8520c7ef247c4b95c9bd76d3a25da9ff5`;
- `qcom_swrm_get_port_config()` stores DT array index `i` into
  `ctrl->pconfig[i + 1]`.

This upstream source is used only to establish the stable DT array-indexing
semantics. It is **not** a substitute for the missing exact SP11 kernel tree
containing local commit `23aa077`.

## Linux parity consequence

Do **not** add or require a Linux physical master port 14 for CPS.

The parity target is:

- one physical WSA SoundWire master port **13**;
- 24 kHz / 800-clock CPS transport on that master port;
- both WSA8845 slaves use local DP6;
- both slave DP6 ChannelEnable masks remain `0x03`;
- left slave OffsetCtrl1 `0`;
- right slave OffsetCtrl1 `25`;
- normal SoundWire/WSA port-parameter mechanisms only.

The existing Phase91 DTB already expresses the shared master-port-13 mapping
and the correct master-side 800-clock / Offset1=0 shape. The unresolved Linux
implementation delta is specifically how the local kernel lineage applies the
**per-slave DP6** parameters, especially the right-slave OffsetCtrl1 `25`, while
preserving native mask `0x03` on both speakers.

The rejected CPS-Lab split-mask experiment (`0x1` / `0x2`) must remain rejected.
It attempted to separate the speakers in channel-mask space instead of
preserving the Windows per-slave offset distinction.

## Source-tree gate remains in force

The exact kernel source tree containing local commit `23aa077`
(`ASoC: qcom: add dedicated SP11 CPS feedback backend`) is still absent from the
connected Fedora host and is not present in any GitHub repository accessible to
the authenticated account. No replacement kernel patch should be guessed
against a different tree.

Until that exact lineage is recovered, safe work is limited to source-independent
artifact review, requirements capture, topology/DTB validation, and updating the
no-boot candidate plan.

## Safety

No KD session was opened for this correction. No physical MMIO read or write,
DSP write, SoundWire slave write, driver-state write, boot-target change, GRUB
change, or kernel build/deployment was performed.
