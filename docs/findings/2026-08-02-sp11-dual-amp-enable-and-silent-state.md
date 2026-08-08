# SP11 dual amplifier enables and the clean2 silent-left state

Status: raw/static/dynamic corroborated diagnostic finding.  This document
separates proved facts from the remaining mapping hypotheses; recovered prose
was not accepted as evidence by itself.

## Acoustic failure that invalidates clean2

The operator heard only the right speaker during real YouTube playback.  The
failure reproduced after restarting PipeWire and with controlled stereo pink
noise.  At the same time:

- PipeWire had active front-left and front-right links;
- left/right mixer controls were symmetric and both complete DAPM routes were
  powered;
- SoundWire device `...:0` (Linux `SpkrLeft`) was enabled but held
  `PA_FSM_STA0=0x2a`;
- device `...:1` (Linux `SpkrRight`) was enabled and held
  `PA_FSM_STA0=0x2f`;
- `PA_FSM_STA1` and both error-condition registers were zero on both devices.

Evidence:
`artifacts/clean2-boots/20260802-clean2-511be77f84144b26a289dde98271342d/youtube-right-only-20260802T084523`.

The current recovery worker only tests error bits in `STA1`.  It therefore
cannot notice this persistent silent state.  The bit-level meaning of `0x2a`
versus `0x2f` is not public and remains unresolved; the values must not be
relabeled as named hardware states without new evidence.

## Windows exposes two independent APSS GPIO resources

Three independent sources agree:

1. The exact SP11 DSDT `AUCD._CRS` buffer contains two `GpioIo` resources on
   `\\_SB.GIO0`: pins `0xcc` (204) and `0xcd` (205), followed by interrupt
   pin `0xca` (202).  DSDT AML SHA-256:
   `46ed91e629ddb55229b64e1577df43fecd474eedc818cdd8900975116d65c596`.
2. The exact `MSHW0486` Surface extension INF declares two GPIOs.  GPIOUID 1
   resolves resource index 0 and GPIOUID 2 resolves index 1.  INF SHA-256:
   `eae4bc6c98288f7e5a4ca793655d1072b16cf8b97cb352606b63b778d65c2402`.
3. A KD hardware execution breakpoint on the exact `qcaucd8380.sys` GPIO-write
   function captured two back-to-back writes of value `1`, first to the handle
   for GPIOUID 1 and then to GPIOUID 2.  The writes came from consecutive loop
   iterations and no deassert pulse was captured.  KD log SHA-256:
   `050a1e264d42886cad4bac21aad1808fa99d116c0bffcdf4c4c75bce6248f6a8`.
   Driver SHA-256:
   `bd0c8276c51fc7a020c616e904dd613b6ccf187ec3e1fe6f94c2c811c8adc8bf`.
4. A separate raw KD read of TLMM registers captured `IN_OUT=0x3` and
   `CFG=0x1bc0` on both pins.  Under the Qualcomm TLMM layout this is output
   high, function 0, no pull, and 16 mA.  Capture SHA-256:
   `e7638119e9c850d07ba5259735b494fa49c16b16be26f6851ada51db3b909db7`.

This proves that Windows owns and holds two separate APSS-side enable/reset
resources.  It does not, by itself, prove which physical speaker is GPIOUID 1.

## Linux discrepancy observed live

On clean2 at `2026-08-02T09:17:40+01:00`, main-TLMM pins 204 and 205 were both
high, GPIO function, no pull, and 4 mA, but pinctrl reported both as
`UNCLAIMED`.  The device tree instead assigned both WSA8845 nodes the same
LPASS-TLMM GPIO 12 reset.

The firmware-left-high state explains why both amplifiers can enumerate, but
it is not equivalent to Linux owning the two lines across power-management and
reinitialization events.  The next diagnostic DT therefore:

- retains LPASS GPIO 12 high as a pinctrl state, avoiding an unproved removal;
- assigns APSS GPIO 204 and 205 as separate active-low `reset-gpios` to the two
  WSA nodes;
- configures both APSS pins for GPIO, no pull, and 16 mA to match the Windows
  active observation.

The provisional `204 -> SoundWire unique ID 0` and `205 -> unique ID 1`
association follows the stable resource/device order.  It is intentionally
classified as a diagnostic mapping, not final physical-left proof.

## Static Windows driver constraints

Fresh Ghidra analysis of the hash-bound Windows driver shows separate WSA
objects, but both objects copy the same global 2S/4-ohm/18-dB hardware profile.
The REV_0D ACDB also has only one matching hardware-profile row.  There is no
evidence for separate static left/right PA gains or loads.

The actual per-speaker differences live in the protection calibration.  The
next kernel therefore also provides, disabled by default:

- transition-only PA and SoundWire status logging;
- a one-shot recovery of a selected `STA0` value and selected SoundWire unique
  ID;
- a complete two-speaker SPVI binding swap, including R0/T0, V/I channel-map,
  520-byte model blocks and 52-byte per-speaker configuration blocks;
- a protection-parked graph switch for a deliberately low-gain diagnostic.

These controls permit several decisive tests after one reboot.  None changes
the default protected path until explicitly selected at runtime.

## Recovered archive triage

The wider recovered corpus contains mutually incompatible narratives: some
older files describe four speakers, two WSA buses, or a dedicated left render
backend, while later files describe the observed two-slave single-bus design.
None of those prose labels was used to assign GPIOs or calibration records.
The mapdiag candidate relies only on the hash-bound raw sources above, the
reviewed Windows packet bodies, current kernel source, and reproduced live
Linux state.  In particular, speaker index 0 is not called “physical left” in
the evidence ledger merely because Linux happens to name SoundWire UID 0
`SpkrLeft`.
