# WSA884x 2S supply gap and left-channel PA fault

Date: 2026-07-29

> **Erratum / superseded scope:** This finding validated the missing 2S
> initialization, but it did not close the nominal-load profile. A subsequent
> reboot logged five more identical left-PA faults. Structural ACDB decoding
> then proved that SP11 selects a 4-ohm load while this module still carried
> upstream 8-ohm/21-dB sensing and PBR values. See
> [`2026-07-29-wsa884x-sp11-4ohm-profile.md`](2026-07-29-wsa884x-sp11-4ohm-profile.md)
> and patch `0022`. The clean stress interval below remains historical
> evidence for the 2S correction; it must not be read as final root-cause
> closure.

## Result

The repeated left-speaker dropout reaches the WSA884x PA state machine and is
not a Dolby, PipeWire, mixer, or AudioReach graph-volume event. This patch
corrected one proven board-configuration fault: missing 2S initialization.

Both SP11 amplifiers report `VPHX_SYS_EN_STATUS = 0x02`, which Qualcomm
defines as `CONFIG_2S`.  The upstream Linux WSA884x driver nevertheless used
its fixed QRD8550-oriented defaults:

- 1S supply assumptions;
- 8-ohm PBR/class-H thresholds;
- fixed 21 dB speaker gain assumptions;
- no per-board battery, load, or gain properties;
- no Qualcomm 2S analogue initialization.

During full-volume playback the left amplifier repeatedly entered:

```text
PA fault: sta0=0x0 sta1=0x6 err0=0x8 err1=0x0
```

The temporary PA health worker recovered each event in approximately 1.5 ms,
but it did not remove the electrical cause.

## Evidence convergence

The 2S initialization is not inferred from listening tests.  It is supported
by three independent sources:

1. Both live SP11 WSA884x devices report supply configuration `0x02`.
2. Qualcomm's full downstream WSA884x source defines `CONFIG_2S = 2` and
   supplies explicit 2S register overrides, UVLO values, and PBR current
   limits.
3. A newer independently decompiled WSA884x module applies the same values
   when its battery-configuration field equals `2`.

Qualcomm's full driver also performs the one-shot
`ANA_WO_CTL_0 = 0xC0 | (pa_aux_gain << 2) | speaker_mode` write.  The upstream
driver omitted the `0xC0` VPHX enable bits and produced `0x1d`; the corrected
speaker value is `0xdd`.

The Windows live calibration reported:

- left `R0 = 4.955847740 ohm`;
- right `R0 = 5.370454669 ohm`.

This explains why the left channel can reach a protection boundary first, but
it was not by itself sufficient to declare the nominal load category. The
later structural ACDB decode proved the category is 4 ohms.

## Implemented correction

Patch
[`0021-wsa884x-recover-pa-and-apply-2s-supply.patch`](../../patches/0021-wsa884x-recover-pa-and-apply-2s-supply.patch)
adds:

- hardware detection through `VPHX_SYS_EN_STATUS`;
- Qualcomm's exact 2S class-H and DAC common-mode settings;
- the downstream UVLO sequence;
- the non-1S low-battery OCP correction;
- the 2S PBR current limit;
- the missing one-shot VPHX analogue-enable bits;
- PA-state health monitoring and bounded Qualcomm-sequence recovery as a
  safety net.

The driver retains upstream behavior for a detected 1S configuration and
warns rather than guessing for unsupported configurations.

## Runtime proof

Validated on:

```text
kernel:        7.1.5-sp11-audio-vi
module:        snd_soc_wsa884x
srcversion:    FA7950FAFC83EAEDC2F3A41
module SHA256: beaaeaf0a87cee9c6550e70a8e8e67ecb34713e0f8f759e2b0c53470a6e0a5fa
```

Both amplifiers logged:

```text
detected VPHX supply configuration: 2S
```

The programmed values on both devices were:

| Register | Value | Purpose |
| --- | ---: | --- |
| `0x3005` | `0x77` | UVLO thresholds |
| `0x3006` | `0x40` | UVLO control |
| `0x3040` | `0xd2` | clear non-1S low-VBAT OCP selector |
| `0x3045..0x3049` | `06 14 19 1b 1c` | 2S DAC VCM curve |
| `0x306a` | `0x02` | DAC VCM final override |
| `0x3091` | `0x44` | 2S PBR current limit (`0x11`) |
| `0x3433` | `0xc0` | PA FSM timer |
| `0x3460` | `0x1d` | UVLO deglitch |
| `0x34d1` | `0x21` | class-H 2S slew setting |
| `0x34d2` | `0x13` | class-H 2S PA voltage |
| `0x3504` | `0xdd` | VPHX enable, PA auxiliary gain, speaker clamp |

During active full-volume playback both devices simultaneously reported:

```text
PA_FSM_EN       0x01
PA_FSM_STA0     0x2f
PA_FSM_STA1     0x00
PA_FSM_ERR0     0x00
PA_FSM_ERR1     0x00
VPHX status     0x02
ANA_WO_CTL_0    0xdd
interrupt       0x00
```

Validation workload:

- eight alternating full-volume stereo pink-noise passes;
- twenty repeated playback-client start/stop cycles;
- protected render graph and 8 kHz stereo VI feedback active;
- Dolby boundary present in bypass mode.

Observed result:

```text
PA faults:            0
PA recoveries:        0
SoundWire IRQ storms: 0
```

Before the 2S correction, comparable full-volume playback repeatedly faulted
the left amplifier within seconds. The clean run validated the 2S correction
over that interval only. A later reboot reproduced the fault under the
remaining mixed 4-ohm/8-ohm profile, so it did not validate final dropout
removal or Windows tonal/loudness parity.

## Remaining WSA884x parity work

Patch `0022` now implements the evidence-backed 4-ohm/18-dB/2S sensing, OCP,
and PBR profile. Its first reboot loaded the intended module and, after 16
accepted protected graph starts, had logged zero PA faults, recovery actions,
XRUN-like events or SoundWire IRQ storms. A subsequent read-only capture
confirmed the exact advertised V/I gain, OCP, current-limit and 15-step PBR
register values on both codecs. Controlled full-volume stress, nonzero
protection feedback, PBR/CPS transport closure and any remaining per-device
OTP or board-policy overrides remain open.

Dolby dynamic processing remains a separate project.  Its module boundary is
present in bypass mode and was not used to conceal or compensate for this
hardware-driver fault.
