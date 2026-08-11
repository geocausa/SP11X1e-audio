# Linux CPS per-slave SoundWire transport gap

Date: 2026-08-11 (Europe/London)

## Result

The remaining Linux parity blocker can now be stated at source level without
having the missing `23aa077` kernel tree.

The established SP11 topology is one physical WSA SoundWire master port 13
shared by two WSA8845 slave DP6 endpoints. Windows programs both slave masks as
`0x03`, but gives the left DP6 OffsetCtrl1 `0` and the right DP6 OffsetCtrl1
`25` (`0x19`). Windows also programs the 800-clock interval on each slave as
SampleCtrl1 `0x1f` plus SampleCtrl2 `0x03`, with HCtrl `0xff` and BlockCtrl3
`0x00`.

The tracked Linux lineage and upstream SoundWire code reveal two independent
gaps that must be audited in the exact local kernel before a replacement
candidate is built:

1. **per-slave OffsetCtrl1 is lost by normal Qualcomm master-port mapping**;
2. **WSA884x DP6 is declared `SDW_DPN_SIMPLE`, so generic SoundWire core does
   not execute the extended slave transport writer that emits SampleCtrl2,
   HCtrl and BlockCtrl3.**

The native WSA884x CPS channel mask is already `0x03`. The rejected `0x01` /
`0x02` split-mask experiment was therefore solving the wrong layer.

## Gap 1: both slaves inherit the same master-port-13 Offset1

In the reviewed upstream Qualcomm SoundWire implementation,
`qcom_swrm_compute_params()` walks each slave runtime port and does:

```text
m_port = slave->m_port_map[p_rt->num]
pcfg = &ctrl->pconfig[m_port]
p_rt->transport_params.sample_interval = pcfg->si + 1
p_rt->transport_params.offset1 = pcfg->off1
p_rt->transport_params.offset2 = pcfg->off2
p_rt->transport_params.blk_pkg_mode = pcfg->bp_mode
p_rt->transport_params.blk_grp_ctrl = pcfg->blk_group_count
p_rt->transport_params.hstart = pcfg->hstart
p_rt->transport_params.hstop = pcfg->hstop
p_rt->transport_params.lane_ctrl = pcfg->lane_control
```

On SP11 both WSA speaker nodes map their local CPS DP6 to physical master port
13:

- left: `qcom,port-mapping = <1 2 3 7 10 13>`;
- right: `qcom,port-mapping = <4 5 6 7 11 13>`.

The reviewed Phase91 WSA master-port-13 configuration has Offset1 `0`.
Consequently this baseline algorithm assigns **Offset1 `0` to both slave
DP6 runtimes**. It has no representation for Windows' required right-slave
Offset1 `25` while both slaves share the same physical master port.

This is the exact reason a Linux parity implementation needs a per-slave
transport distinction somewhere after/beside the master-port mapping. Creating
a fake physical master port 14 is not the answer; the qcaucd slot-14 correction
proved Windows does not program one.

## Tracked local-lineage check

`patches/0020-sp11-audio-vi-cumulative.patch` has SHA-256:

`2311438E938F6FC941FABFA2D4972FA3C799F2E6795654596030758D858A1E51`

Its `drivers/soundwire/qcom.c` diff starts from blob prefix `3d8f5a8`. The
reviewed upstream qcom.c at Linux commit
`d58772d8520c7ef247c4b95c9bd76d3a25da9ff5` has full blob SHA:

`3d8f5a81eff19511d80e33c76f54972691ccf530`.

Thus the tracked patch is demonstrably based on the same qcom.c implementation
reviewed here. Its only qcom.c hunks are around stream-allocation direction,
hw_params invocation and DAI registration/rates. It does **not** alter
`qcom_swrm_compute_params()` or provide a per-slave offset override.

This establishes the transport gap for the tracked local lineage through that
patch. The later local commit `23aa077` remains unavailable, so this finding
does not assume whether that missing commit changed the function; the recovered
exact tree must be checked first.

## Gap 2: WSA884x DP6 is SIMPLE but Windows uses extended DP6 registers

Current upstream `wsa884x.c` and the tracked local WSA patch lineage declare
WSA884x CPS DP6 as:

```text
num = WSA884X_PORT_CPS + 1
.type = SDW_DPN_SIMPLE
.simple_ch_prep_sm = true
.read_only_wordlength = true
```

The same upstream WSA port configuration gives CPS:

```text
num = WSA884X_PORT_CPS + 1
ch_mask = 0x3
```

So the native `0x03` mask already agrees with Windows.

Generic SoundWire core's `sdw_program_slave_port_params()` always programs the
basic slave port state including SampleCtrl1 and OffsetCtrl1. It calls the
extended `_sdw_program_slave_port_params()` only when the DPN type is **not**
`SDW_DPN_SIMPLE`.

For a FULL DPN the extended path writes, among other fields:

- OffsetCtrl2;
- BlockCtrl3;
- SampleCtrl2 from the high bits of `sample_interval - 1`;
- HCtrl from HStart/HStop.

Therefore a CPS runtime with `sample_interval = 800` and a slave DP6 still
classified as SIMPLE will write low SampleCtrl1 `0x1f`, but the generic core
path will not itself write the required high SampleCtrl2 `0x03`, HCtrl `0xff`
or BlockCtrl3 `0x00`.

Windows directly proved that both WSA8845 slave DP6 endpoints receive those
extended values. The Phase91 DTB already supplies the corresponding
master-port-13 values, but master-side controller programming does not by itself
prove that the slave DP6 registers receive them.

## Important caution: do not blindly change DP6 to FULL

This finding does **not** recommend an unreviewed one-line
`SDW_DPN_SIMPLE -> SDW_DPN_FULL` patch.

A FULL classification changes which generic slave registers the core writes,
including fields Windows may leave untouched/suppressed in its qcaucd template
(e.g. optional OffsetCtrl2). WSA884x also marks word length read-only, while
Windows writes BlockCtrl1 `0x18`; those semantics need to be reconciled against
the exact local driver and device capability behavior.

The safe implementation requirement is narrower:

- preserve the normal SoundWire framework;
- ensure the WSA8845 CPS DP6 receives the proven 800-clock extended transport
  state by a capability-correct path;
- avoid direct physical-MMIO or ad-hoc raw slave-register workarounds;
- review every additional register a proposed DPN-type change would cause the
  core to write.

## Rejected CPS-Lab runtime now makes sense at this layer

The rejected CPS-Lab candidate showed:

- left/right split masks `0x01` / `0x02` caused repeated SoundWire bus-clash
  alerts;
- left CPS alone with mask `0x01` still clashed;
- left CPS alone with the codec's native mask `0x03` completed its controlled
  test window without a bus-clash/retry message.

That result is consistent with the Windows evidence that channel-mask splitting
is not the speaker-separation mechanism. Windows preserves mask `0x03` and
separates the two slave schedules with OffsetCtrl1 `0` / `25` on shared master
port 13.

The runtime result does not by itself prove which Linux source change is
sufficient; it reinforces where the next source audit must focus.

## Exact source/binary recovery gate

The exact kernel tree containing local commit `23aa077`
(`ASoC: qcom: add dedicated SP11 CPS feedback backend`) is still not available
on the connected hosts or authenticated GitHub repositories.

A search of SP7 local/ignored artifact locations also did not recover the exact
CPS-Lab module pair. The only local modules found are the earlier Phase91
artifacts:

- Phase91 `soundwire-qcom.ko.zst` SHA-256
  `68137DF872AB28204DD53196E1A82744156598968E069CA2962CE8B45D991D85`;
- Phase91 `snd-soc-wsa884x.ko.zst` SHA-256
  `68EF789E982D6CD04EEC1BEC668FAF475A8BD127DEAAFDEAE7F571B59EA4857B`.

They do not match the rejected CPS-Lab recorded hashes:

- CPS-Lab `soundwire-qcom`:
  `B4524693A5458C5E672D248DA6D77C8DAD7ABED6DCBEC31E81C55225DE65AE0F`;
- CPS-Lab `snd-soc-wsa884x`:
  `3106227BAC14FB342EB6ADF841F52C81E9B33E846CCFE4698E9753F39D8BF78B`.

Do not claim the missing `23aa077` tree has or lacks these fixes until its source
or exact binaries are recovered.

## Source-recovery checklist

When the exact tree is recovered, inspect these items **before editing**:

1. `drivers/soundwire/qcom.c` / `qcom_swrm_compute_params()`:
   - does it still copy slave DP6 Offset1 exclusively from mapped master port
     13?
   - is there already an SP11/per-slave override in `23aa077` or later local
     work?
2. `sound/soc/codecs/wsa884x.c` CPS DPN properties:
   - is DP6 still `SDW_DPN_SIMPLE`?
   - is the native CPS `ch_mask` still `0x03`?
3. `drivers/soundwire/stream.c`:
   - is the SIMPLE/FULL programming split unchanged?
4. the dedicated CPS backend changes from `23aa077`:
   - confirm they affect graph/DAI/backend plumbing only or identify any
     transport-param changes explicitly.

Only after that audit should a replacement patch be designed.

## Candidate requirements if the gaps remain

If the exact tree still has the baseline behavior, the replacement must achieve
all of the following without adding physical master port 14:

- shared physical WSA master port 13;
- both WSA8845 local DP6 ChannelEnable masks `0x03`;
- left DP6 OffsetCtrl1 `0`;
- right DP6 OffsetCtrl1 `25`;
- slave DP6 SampleCtrl1/2 equivalent to `0x031f` (800 clocks / 24 kHz);
- slave DP6 HCtrl `0xff`;
- slave DP6 BlockCtrl3 `0x00` where capability semantics require/program it;
- preserve 48 kHz render, 8 kHz VISENSE and accepted PBR behavior;
- normal SoundWire/WSA framework path only.

A clean design should represent the **per-slave** distinction explicitly rather
than abusing channel masks or inventing another physical master port.

## Primary source references

Reviewed upstream reference commit:

`d58772d8520c7ef247c4b95c9bd76d3a25da9ff5`

Files:

- `drivers/soundwire/qcom.c`
  - blob `3d8f5a81eff19511d80e33c76f54972691ccf530`;
  - `qcom_swrm_compute_params()` copies mapped master `pconfig` into slave
    transport params;
  - `qcom_swrm_transport_params()` programs the physical master/controller
    static port configuration.
- `drivers/soundwire/stream.c`
  - blob `5d20e95a1e23eb5cb78ea25386a04088ef1e3bea`;
  - `sdw_program_slave_port_params()` and
    `_sdw_program_slave_port_params()` define the SIMPLE/FULL slave-register
    programming split.
- `sound/soc/codecs/wsa884x.c`
  - reviewed blob `6c6b497657d0c8512c3356fde9f91868af0154bf`;
  - WSA884x CPS DP6 is SIMPLE and native CPS port config mask is `0x03`;
  - `qcom,port-mapping` is loaded into `sdw_slave::m_port_map`.

The same WSA DP6 SIMPLE declaration and native `0x03` CPS mask remain present
in current upstream at the time of review.

## Safety

No target mutation was performed for this finding. No KD session, physical-MMIO
access, SoundWire register write, kernel build, module replacement, GRUB change
or reboot was used.
