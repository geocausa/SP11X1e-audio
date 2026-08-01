# Gain chain and loudness investigation — 2026-08-01

Operator report: **maximum Linux loudness is roughly 10% of Windows loudness.**
This document records what the gain chain actually contains, what was ruled
out, and what remains open.

Provenance tags: `OBSERVED` = read from this machine. `SOURCE` = read from
kernel source. `STATIC` = from an on-disk blob. `INFERRED` = reasoned, inputs
named.

---

## 1. The full gain chain as deployed

`OBSERVED` on `7.1.5-sp11-audio-diag-observe+`:

| Stage | Control | Value | Range | dB |
|---|---|---|---|---|
| PipeWire sink | software | 1.00 | 0..1 | 0 dB |
| DSP | `VOL_CTRL` x3 modules | set once at graph start | — | — |
| WSA macro | `WSA WSA_RX0/RX1 Digital Volume` | **81** | 0..81 | max, scale min -84 dB, 1 dB step |
| Amplifier | `SpkrLeft/Right PA Volume` | **6** | 0..**6** | 0 dB, scale min -9 dB, 1.5 dB step |

**Everything user-reachable is already at maximum.** There is no headroom left
in the mixer. The ceiling is genuinely low.

### Correction to an earlier claim in this project

An earlier note recorded WSA digital volume as "81 of 84, drifted from the
Windows-derived 72". That is wrong. The control's **maximum is 81**, so it is
pinned at the top, not drifted.

---

## 2. `PA Volume` reports max=6 but the register holds 21

`SOURCE`, `sound/soc/codecs/wsa884x.c`:

```c
static const DECLARE_TLV_DB_SCALE(pa_gain, -900, 150, -900);

SOC_SINGLE_RANGE_TLV("PA Volume", WSA884X_DRE_CTL_1,
                     WSA884X_DRE_CTL_1_CSR_GAIN_SHIFT,
                     0x0, 0x1f, 1, pa_gain),
```

`xmin=0x0, xmax=0x1f` (31), yet ALSA reports `min=0, max=6` and silently clamps
a write of 12 back to 6. **This is identical in pristine upstream 7.1.5**, so it
is not a local patch. Why ALSA reports 6 is **UNRESOLVED**.

`OBSERVED`, live register from `wsa-regmaps-full.txt`, both amps:

```text
DRE_CTL_1 (0x30b1) = 0xaa = 1010 1010b
  CSR_GAIN    (bits 5:1, mask 0x3e) = 21   -> -9 + 1.5*21 = +22.5 dB
  CSR_GAIN_EN (bit 0)               = 0    -> disabled
```

Note `0xaa` is a classic reset pattern, so 21 may simply be an untouched
power-on default.

---

## 3. `CSR_GAIN` is NOT a volume control — ruled out

`SOURCE`, `wsa884x_init()`:

```c
if (wsa884x->port_enable[WSA884X_PORT_COMP]) {
        /* compander on  -> CSR_GAIN_EN = 0 */
} else {
        /* compander off -> CSR_GAIN_EN = 1 */
}
```

`CSR_GAIN` is the **static fallback gain used only when the compander (DRE) is
not running**. COMP is on on this machine, so `EN=0` is correct behaviour, and
the 21 in the field is unused. Enabling it would conflict with the compander,
not add +22.5 dB.

**Do not chase `CSR_GAIN_EN` as the missing loudness.** Windows runs the
compander too and will behave the same way.

An earlier draft of this investigation claimed this was "a disabled gain stage
Windows might be enabling". That was wrong and is retracted here.

---

## 4. Windows' volume slider — where it actually lands

`INFERRED` (inputs: the control inventory in section 1, the DSP graph module
list, and the `VOL_CTRL` findings of 2026-07-30).

Ruled out or already maxed:

* `CSR_GAIN` — DRE-internal, not a volume path (section 3)
* `WSA RX0/RX1 Digital Volume` — already at max 81
* `PA Volume` — already at max 6

Remaining candidate: **the DSP `VOL_CTRL` modules**. There are three in the
deployed graph, and `PARAM_ID_VOL_CTRL_MASTER_GAIN` (`0x08001035`) is the
parameter Windows would drive.

This matters because of a separately verified defect:
`audioreach_put_vol_ctrl_audio_mixer()` in `topology.c` **stores the value and
sends nothing**. `audioreach_gain_set_vol_ctrl()` is only called from
`audioreach_pga_event()` on `POST_PMU`, i.e. once at graph start. Every later
change is dropped. A fix for this was written on 2026-08-01 but is currently
**inert**, because no ALSA control is wired to those DSP modules
(no `SectionControlMixer` exists for `sp11.vol_ctrl.4a63`).

---

## 5. Windows loudness is NOT primarily dynamics — correction

`STATIC`, `SP11_DOLBY_SOUND_QUALITY_SPEC_2026-05-30.md` describes the VLLDP
chain: input trim -8..-10 dB, program level detector, low-band dynamic gain,
makeup/adaptive program gain, multi-band regulator, envelope limiter capping
at -1.0 dBFS.

An earlier claim in this session attributed the entire Windows/Linux loudness
gap to this dynamics chain. **That was an overreach and is retracted.**
Windows has a volume slider spanning silence to full output; that is a static
gain path, not compression. Dynamics change perceived loudness *at a given
setting*; they do not create the range.

The dynamics chain is still relevant to perceived level and to tonal parity,
but it is not the explanation for a 10x maximum-loudness difference.

---

## 6. OPEN LEAD: supply/gain profile — `G_18_DB / CONFIG_2S / WSA_4_OHMS`

`SOURCE`, `wsa884x_apply_supply_config()` carries a comment:

> Qualcomm's full WSA884x driver has board-specific supply initialization
> which is not represented by the upstream driver's QRD8550-oriented 1S
> defaults. VPHX_SYS_EN_STATUS is a hardware strap/status value, so use it to
> select the electrically correct initialization without depending on an
> incomplete board description.

and a register sequence named `sp11_2s_4ohm_pbr` referencing Qualcomm
downstream **`G_18_DB / CONFIG_2S / WSA_4_OHMS`**.

**18 dB is a large, specific number** and this is an output-stage/boost
configuration, exactly the class of setting that could account for a large
static level difference. Whether the correct profile is being applied on this
board is **NOT YET VERIFIED**.

This is the strongest remaining lead for the loudness gap.

---

## 7. Operator's stated volume envelope

Recorded so it is not lost or second-guessed:

* start any static volume ceiling at **50%**, to protect speakers and amps
* even with a perfect Dolby/driver/topology clone, keep the ceiling at
  **max 80%**
* the operator never listens above 40-50% on Windows anyway
* **50-80% is the acceptable working range**

Any change to a static gain ceiling should land inside that envelope, and
should start at the bottom of it.

---

## 8. Process note

Two listening tests were run against a payload that could not have worked
(wrong MSIIR wire format), and one was run with YouTube still playing over the
test tone. Additionally, test volume was repeatedly reset to 0.30 "for safety"
while the operator's actual complaint was that maximum is far too quiet.

Decode wire formats before testing on hardware. Confirm nothing else is
playing. Do not silently reduce the level the operator is trying to evaluate.
