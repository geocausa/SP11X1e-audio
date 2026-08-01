# SOLVED: loudness ceiling was an upstream software cap — 2026-08-01

**Validated result:** the temporary upstream PA-volume cap was the dominant
Linux loudness restriction. The accepted clean operating point is PA 24
(+27 dB) with WSA digital 81 (-3 dB). The operator considers the resulting
ceiling usable and substantially better, although still below Windows.

This supersedes the conclusions in `2026-08-01-gain-chain-loudness-investigation.md`
(written earlier the same day, before the cause was found) and the whole of
`docs/plans/2026-08-01-pa-aux-gain-to-spec.md`, which chased the wrong thing.

---

## 1. Root cause

`sound/soc/qcom/x1e80100.c`, in `x1e80100_snd_init()`:

```c
/*
 * Set limit of -3 dB on Digital Volume and 0 dB on PA Volume
 * to reduce the risk of speaker damage until we have active
 * speaker protection in place.
 */
snd_soc_limit_volume(card, "SpkrLeft PA Volume", 6);
snd_soc_limit_volume(card, "SpkrRight PA Volume", 6);
```

`snd_soc_limit_volume()` rewrites the control's **maximum** at card init. That
is why `PA Volume` reported `max=6` while the hardware register
(`DRE_CTL_1`, mask `0x3e`, bits 5:1) supports 0..31, and why
`amixer cset ... 12` was silently clamped back to 6.

Scale: value N = `-9 + 1.5*N` dB. So 6 = 0 dB, 31 = +37.5 dB.

### It was never hardware, topology, or protection

Ruled out by measurement during the investigation:

* every stage reported maximum: PipeWire 1.00, DSP `VOL_CTRL` master gain
  `0x10000000` = unity in Q28, WSA digital 81/81, PA 6/6
* `CSR_GAIN`/`CSR_GAIN_EN` is the compander-off fallback gain, not a volume
  path; COMP is on, so `EN=0` is correct and Windows behaves the same
* the SP11 2S/4-ohm supply profile is correctly applied (six registers
  verified live against the driver sequence)
* speaker protection is not attenuating anything

## 2. Upstream's own words: the cap was meant to be temporary

From Johan Hovold's January 2024 series *"ASoC: qcom: sc8280xp: limit speaker
volumes"*:

> "Limit the digital gain and PA volumes to a combined -3 dB in the machine
> driver to reduce the risk of speaker damage **until we have active speaker
> protection in place (or higher safe levels have been established)**."

> "Note that this will probably need to be generalised using machine-specific
> limits, but a common limit should do for now."

Srinivas Kandagatla, reviewing:

> "LGTM, **We can get rid of this limit once we have Speaker protection
> inplace.**"

The series went through four revisions with the value moving 12 -> 1 -> 17,
and one revision notes the choice was partly driven by a userspace quirk, not
acoustics: *"the PA volume limit cannot be set lower than 0 dB or PulseAudio
gets confused when the first 16 levels all map to -3 dB."*

The SP11 cap of `6` is also **lower** than sc8280xp's `17`, and the
accompanying `WooferLeft`/`TweeterLeft`/`WooferRight`/`TweeterRight` entries
show it was written generically for four-speaker machines, not tuned for this
two-speaker board.

### The precondition is satisfied on this machine

Verified live 2026-08-01:

* `SPEAKER_PROTECTION` `0x4027` and `SPEAKER_PROTECTION_VI` `0x4024`
  instantiated and enabled with VI feedback
* VI feedback stream at 8 kHz on **both** amplifiers via `WSA_CODEC_DMA_TX_0`
* R0/T0 calibration byte-identical to Windows
  (4.956 ohm / 38.7 C, 5.370 ohm / 37.0 C)
* 106 of 107 graph calibration frames accepted by the DSP
* 20 protection stages accepted at graph start

## 3. Changes applied

### Kernel: `sound/soc/qcom/x1e80100.c`

All six `snd_soc_limit_volume(... "PA Volume", 6)` calls **removed**, exposing
the full 0..31 range. The four Digital Volume limits (81, -3 dB) are retained
as upstream. Backup: `x1e80100.c.bak-before-palimit-20260801`.

### UCM: `SP11-HiFi.conf`

```text
cset "name='SpkrLeft PA Volume' 24"     # was capped at 6
cset "name='SpkrRight PA Volume' 24"
cset "name='WSA WSA_Softclip0 Enable' 1"
cset "name='WSA WSA_Softclip1 Enable' 1"
```

24 = **+27 dB**. Values up to 26 were tested during bring-up, but 24 is the
accepted clean baseline. Backups: `.bak-before-pa12-20260801`,
`.bak-before-pa26-20260801`, `.bak-before-softclip-20260801`.

## 4. Hardware soft clipping — was disabled, now enabled

The LPASS WSA macro contains **dedicated soft-clip hardware, one block per RX
channel**, with its own clock enable, mux and CRC registers
(`CDC_WSA_SOFTCLIP0_SOFTCLIP_CTRL` etc. in `lpass-wsa-macro.c`). ALSA controls
are already exposed:

```text
numid=18  'WSA WSA_Softclip0 Enable'    was off
numid=19  'WSA WSA_Softclip1 Enable'    was off
```

Both were **off**. Now enabled and written into UCM.

Soft clipping rounds peaks that would otherwise clip hard, avoiding the harsh
odd-order harmonics hard clipping produces. Upstream leaving it off is a
reasonable default when PA gain is capped at 0 dB. It is enabled at the
protected +27 dB operating point.

Applied via `wsa_macro_config_softclip()` on `SND_SOC_DAPM_POST_PMU`, so it
takes effect at **stream start**, not immediately on the control write.

## 5. Operator observation worth keeping

> "especially on increased volume sound less rough than on lower volume which
> is weird"

Not weird, and diagnostic. If the roughness were overdrive distortion, more
level would make it worse. The opposite means the low PA gain was **hurting
quality**: the chain attenuated digitally and then amplified a weak signal, so
fewer bits were in use, quantisation noise sat proportionally higher, and the
amplifier noise floor was closer to the music. Raising analog gain lets the
digital path run near full scale, which is how it is designed to operate.

This is why the manufacturer sets analog gain high and controls volume
digitally rather than the reverse. It is not only about loudness.

## 6. Current state

```text
PA Volume        24 of 31   (+27 dB)   both channels matched
Digital Volume   81 of 81   (-3 dB)    runtime adjustable, 1 dB steps
Softclip 0/1     on
PipeWire sink    1.00
```

Register headroom remaining: 7 PA steps (+10.5 dB). This is not a recommendation
to use it; 24 is the accepted operating point.

**Warning kept in the driver comment:** protection limits excursion and coil
temperature. It does not make arbitrary levels safe on 4 ohm micro-speakers.
Treat 24 as already assertive; sustained operation near 31 can damage them.

Check coil temperature within ~3 s of stopping playback (the amps
runtime-suspend and reads then fail):

```sh
cat /sys/class/hwmon/hwmon*/temp1_input
```

Baseline 40-42 C. 50s under load is normal; sustained 60s+ means back off.

## 7. Remaining gap to Windows

Do not close the remaining perceived-loudness gap by raising PA gain again.
The user has accepted the protected PA 24 ceiling. The remaining candidate is
the Windows dynamic-processing layer, especially its volume leveler. Windows'
`AUCD_DEV_0C29_SUBSYS_MSHW0486_REV_0D_ADCM_*.xml`
for this exact device has all static gains at **zero** (`system-gain`,
`postgain`, `pregain`, `calibration-boost`) but sets
`volume-leveler-in-target` and `out-target` to `-320` with
`volume-leveler-drc-enable=1` and `regulator-enable=1`. A leveler driving
average level toward its target can increase perceived loudness on real music
without simply raising peaks.

Note the unresolved units question already flagged in the archive: `-320` was
read as `-32 dBFS` in one document and `-5 dBFS` in another. Settle that before
implementing.

## 8. Process notes

Corrections made to this project's own earlier claims during this
investigation:

* `G_18_DB` in the WSA 4-ohm profile is a **system profile label**, not an
  18 dB output boost. The driver comment already said the profile "keeps
  compander offset/minimum/aux gain at 0 dB". Chasing `PA_AUX_18_DB` cost three
  reboots and was reverted.
* "WSA digital volume drifted from 72 to 81" was wrong: **81 is the maximum**,
  so it was pinned at the top, not drifted.
* "Windows loudness comes from dynamics" was an overreach when first stated —
  the static path was capped and that was the dominant factor. The leveler is
  the remaining layer, not the whole story.

A genuine upstream bug was also found and is documented in commit `444c8fe`:
`wo_ctl_0 = 0xc` pre-sets bits 3:2 which lie inside `PA_AUX_GAIN_MASK` (0x3c),
and the code only ORs, so any gain selector without those bits set is silently
mangled (`0xa` becomes `0xb`). Currently reverted along with the PA_AUX work,
but valid on its own merits.

## 9. Clean-build validation closure

The full clean kernel `7.1.5-sp11-audio-clean+` was booted after an `mrproper`
rebuild. Both live WSA884x regmaps report `0xdd`, proving that the PA_AUX 18 dB
experiment (`0xe9`) is absent. PA 24 and digital 81 remained matched through
controlled and sustained playback. The protected graph started with both VI
paths active, and no PA fault, recovery loop, channel dropout, XRUN or
SoundWire error was observed. This is the baseline to preserve while Dolby is
developed separately.
