# Windows hardware-resource and COMP-route closeout

Date: 2026-08-15
Status: CLOSED for host resource payload and DP2 transport; WSA producer semantics remain OPEN

## Question

After the rejected Linux `DRE_CTL_1=0` cold-boot experiment produced unsafe speaker noise, two lower-stack possibilities remained especially important:

1. Windows might send a hidden runtime codec-register/WSA producer payload to ADSP when the native speaker path starts.
2. Linux might schedule the WSA8845 DP2/COMP slave port correctly while feeding it through the wrong master port/channel allocation.

Both are now closed as explanations for the failure.

## Windows qcadcm hardware-resource trace

Exact installed `qcadcm8380.sys`:

- SHA-256 `37f76305c5ad4840153f940b608cb3756396973463744fb306f7f3d08180a903`
- `AudioHwRscIoctl` RVA `0x89380`
- `gsl_command_hw_rsc_custom_config` RVA `0x5c5d8`
- custom hardware-resource GPR opcode `0x2004`

Classic KDNET breakpoints were armed only on those host-side functions. They printed operation IDs and bounded kernel-owned payload headers and auto-continued. No physical MMIO, SoundWire register or DSP-memory read was performed.

A marked eight-second native-speaker playback interval produced exactly 12 `AudioHwRscIoctl` calls and 12 matching custom-resource sends. Every request belonged to only three already-known resource families:

- `0x08001032` — hardware core enable/disable;
- `0x0800102c` — clock enable/disable;
- `0x080014f3` — endpoint DSP GPIO configuration.

There was no fourth resource ID and no codec-register operation list. Ordinary Windows speaker start/stop therefore does **not** inject a hidden WSA-macro/compander register table through qcadcm at stream start.

Raw SP7 log:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_HW_RSC_RUNTIME_20260815.log`
- final size `83934` bytes
- SHA-256 `b045f38185beb95714d5179fd9801353be56bb9c834d819e5cd9958ea9b80e42`

Reviewed machine-readable summary:

- `artifacts/reviewed/2026-08-15-windows-qcadcm-hw-resource-playback.json`

The marked SP7 playback-delta extract is retained with SHA-256 `030584665ae2f270905e7bea94e146a786743e6c6706cf28b5bafa10d098cdc0`.

## COMP master/slave routing

The retained Windows qcaucd FIFO already proves each WSA8845 DP2/COMP slave port uses:

- ChannelEnable `0x0f`;
- SampleInterval `0x1f`;
- left Offset1 `0x03`;
- right Offset1 `0x04`;
- Offset2 `0x07`.

The Denali device tree maps the same endpoints to the Qualcomm WSA SoundWire master as:

- left WSA8845 COMP DP2 -> master port 2;
- right WSA8845 COMP DP2 -> master port 5.

Hamoa's master port table gives those ports exactly the same Windows intervals and offsets. Therefore the Linux master-side route does not swap, merge or misplace the COMP stream.

The native WSA8845 DPN capability advertises `max_ch=1` for COMP while the runtime port request uses `ch_mask=0x0f`. Source audit confirms Qualcomm SoundWire runtime bandwidth/allocation uses `hweight32(ch_mask)`, so the active `0x0f` request is accounted as four channels. The suspicious `max_ch` value is incomplete metadata on this path, not a one-channel runtime truncation mechanism.

## What remains open

The unsafe `DRE_CTL_1=0` result is no longer plausibly explained by:

- a hidden qcadcm WSA register table at native speaker start;
- a missing DP2 lane;
- wrong left/right COMP master-port assignment;
- wrong COMP interval or offset;
- one-channel truncation caused by the DPN `max_ch` field.

Together with the previous passive boot trace and first-MCLK pre/post-sync observation, the remaining high-value boundary is the **active LPASS WSA-macro runtime producer state and COMP semantics after DAPM configuration but before WSA8845 PA unmute**.

No new `CSR_GAIN_EN=0`/DRE-disabled speaker candidate is permitted until that active producer state is captured and understood.
