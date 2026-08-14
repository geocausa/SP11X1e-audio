> **SUPERSEDED 2026-08-01 (later same day).**
>
> The conclusions here are WRONG. The loudness ceiling was
> `snd_soc_limit_volume(card, "SpkrLeft PA Volume", 6)` in
> `sound/soc/qcom/x1e80100.c` -- an upstream software cap, not the amplifier
> gain profile, not the topology, not speaker protection.
>
> In particular, `G_18_DB` in the WSA 4-ohm profile is a system-profile LABEL,
> not an 18 dB output boost, and `PA_AUX` is kept at 0 dB by that profile
> deliberately. Chasing it cost three reboots and was reverted.
>
> See: docs/findings/2026-08-01-SOLVED-loudness-ceiling-upstream-cap.md

# PA_AUX gain to manufacturer spec + runtime loudness limiter

Date: 2026-08-01

## Problem

Maximum Linux loudness is roughly 10% of Windows. Every user-reachable control
is already at maximum (PipeWire 1.00, WSA digital 81/81, PA Volume 6/6), so
there is no headroom left in the mixer.

## Root cause — established, not inferred

`OBSERVED`: both amplifiers report `OTP_REG_0 = 0x05` = `WSA884X_OTP_ID_WSA8845`
(speaker part), not `0xc` = `WSA8845H` (haptics part).

`SOURCE`, `wsa884x_init()`:

```c
/* Assume that compander is enabled by default unless it is haptics sku */
if (variant == WSA884X_OTP_ID_WSA8845H)
        wo_ctl_0 |= FIELD_PREP(..PA_AUX_GAIN_MASK, ..PA_AUX_18_DB);   /* 0xa */
else
        wo_ctl_0 |= FIELD_PREP(..PA_AUX_GAIN_MASK, ..PA_AUX_0_DB);    /* 0x7 */
regmap_write(wsa884x->regmap, WSA884X_ANA_WO_CTL_0, wo_ctl_0);
```

This board takes the `else` branch and programs the amplifier auxiliary gain to
**0 dB**, in a field that supports **18 dB**.

`ANA_WO_CTL_0` is at `0x3504`, `PA_AUX_GAIN_MASK = 0x3c` (bits 5:2). The field
is a selector at roughly 6 dB per step:

```text
0x0 = disabled
0x7 = 0 dB      <- currently programmed
0x8 = ~6 dB
0x9 = ~12 dB
0xa = 18 dB     <- what Windows programs
```

18 dB is about 8x in voltage, matching the reported ~10x loudness gap.

## Windows uses 18 dB — evidence

From `docs/findings/2026-07-29-wsa884x-sp11-4ohm-profile.md`, derived from
static analysis of this machine's own `qcaucd8380.sys` and the REV_0D ACDB:

* ACDB WSA codec-driver record is `1, 3, 0, 4, 0, 1`; the fourth word is the
  nominal load selector, value **4 ohms**.
* *"load 4 or 6 ohms selects `OCP_CTL = 0xf6`"* — matches this machine live.
* *"gain 18 dB is encoded as `0x24` in the half-dB Windows selector"*.
* Qualcomm downstream tables converge on the same **G_18_DB / CONFIG_2S /
  WSA_4_OHMS** pairing.

The rest of that profile is already correctly applied here. Verified live:

| Register | Expected | Live | |
|---|---|---|---|
| `OCP_CTL` 0x304c | f6 | f6 | match |
| `UVLO_PROG` 0x3005 | 77 | 77 | match |
| `PA_FSM_TIMER0` 0x3433 | c0 | c0 | match |
| `UVLO_PROG1` 0x3006 | 40 | 40 | match |
| `CLSH_V_HD_PA` 0x34d2 | 0x13 | 13 | match |
| `DAC_VCM_CTRL_REG2` 0x3045 | 06 | 06 | match |

**Only the gain field is wrong.**

## Ruled out along the way

* **`CSR_GAIN` / `CSR_GAIN_EN`** (`DRE_CTL_1` 0x30b1 = 0xaa, gain 21, EN=0).
  Not a volume path. It is the static fallback gain used **only when the
  compander is off**; COMP is on here, so `EN=0` is correct and Windows behaves
  identically. Do not chase this.
* **Supply/Class-H profile.** Correctly applied, see table above.
* **"Windows loudness comes from dynamics."** Overreach, retracted. Windows has
  a static volume slider spanning silence to full output; dynamics change
  perceived loudness at a given setting, they do not create the range.

## Plan

### Part 1 — bake the hardware to manufacturer spec

Change the variant test so speaker parts on this board also get 18 dB. Match
what Windows programs on this silicon rather than inventing a Linux value.

> **2026-08-14 correction:** the DRE register discussed by the earlier audit is
> `0x34b1`, not `0x30b1`. Any `0x30b1=0xaa` conclusion is invalid. The new
> Windows FIFO decode also proves state-2 `ANA_WO_CTL_0=0x9d`; the former Linux
> `0xdd` value mis-encoded the two-bit supply field. Patch `0052` supersedes
> this plan's supply-profile claim.

`ANA_WO_CTL_0` is **write-once at codec init**, so this needs a kernel build
and a reboot. There is no runtime path.

### Part 2 — govern loudness at runtime, not in the bake

`WSA WSA_RX0/RX1 Digital Volume`:

* range 0..81, **1 dB steps**, 84 dB span
* **runtime writable** via `amixer`, no rebuild, no reboot
* in the WSA macro, **upstream of the amplifier**, so it attenuates before the
  analog gain stage

Net level after the bake:

```text
PA_AUX 18 dB + digital 81  ->  +18 dB   full Windows parity
PA_AUX 18 dB + digital 75  ->  +12 dB
PA_AUX 18 dB + digital 69  ->   +6 dB   <- chosen starting point
PA_AUX 18 dB + digital 63  ->    0 dB   today's loudness
```

Start at **69**, per the instruction to begin conservatively. Walk up 1 dB at a
time from there.

Operator's stated envelope, recorded so it is not second-guessed: start any
ceiling at 50%, keep it at max 80% even with a perfect clone, never listens
above 40-50% on Windows anyway, so 50-80% is the acceptable working range.

### Part 3 — make it persistent and easy to edit

Put the chosen digital-volume value in the UCM profile with a comment, so it
survives reboots and future sessions change loudness by editing one number.

## Caveat

Attenuating digitally ahead of an 18 dB analog boost is not identical to
reducing analog gain; some digital resolution is traded. At these levels on a
24-bit path this is negligible, and it is what the extra 84 dB of digital range
exists for. Windows does the same: fixed analog gain, variable digital level.

## Safety position

This raises the ceiling for the entire chain, so it is only defensible because
speaker protection is confirmed working on this machine:

* VI feedback live at 8 kHz on both amps (`WSA_CODEC_DMA_TX_0`)
* R0/T0 calibration byte-identical to Windows
  (4.956 ohm / 38.7 C, 5.370 ohm / 37.0 C)
* 106 of 107 graph calibration frames accepted
* 20 protection stages accepted at graph start

If any of that stops being true after a topology or kernel change, re-verify
before running at raised gain.

Diagnostic kernel only. `audio-vi` stays as the known-good rollback entry.
