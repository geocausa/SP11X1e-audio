# Linux CPS V3 live WSA protection observation

Status: accepted read-only runtime evidence. The observer is bounded,
default-off, and does not write amplifier telemetry or protection state.

## Result

The CPS V3 kernel captured 12 consecutive 100 ms snapshots from each WSA8845
during successful playback through the Windows-Dolby PipeWire default. All 24
snapshots completed without a register-read failure. Both power amplifiers
remained active, both reported the same stable PA state, and every observed
fault and interrupt byte remained zero.

The raw ADC, temperature and VBAT register pairs were live and independently
different between the two amplifiers. This closes the earlier question of
whether Linux can observe per-amplifier WSA sensor data while protected audio
is running. The values remain uncalibrated register words: this result does not
assign volts, amperes or degrees, and it does not derive coil temperature or
speaker excursion.

## Exact runtime identity

- capture: 2026-08-11 22:55:52.392761 through 22:55:53.650634 BST;
- boot ID: `20843dbc-dfeb-47d3-87ce-b805c88c292c`;
- kernel release: `7.1.5-sp11-cps-v3+`;
- kernel observer commit: `223f3f1bc197bf18bf410b76b7dea635ee236c71`;
- signed module srcversion: `E084BC31719EE85BB8DEABD`;
- signed module SHA-256:
  `ccc9a4d1a3e0cc34e4761a0b4ddaebbbc152b822bb4f579dd512374e9fe4251e`;
- initramfs SHA-256:
  `8504076a0f40926eee09233451b078d32da1fbb72bb52d4556bec528bd6e6153`;
- reviewed decoded evidence SHA-256:
  `bb2382287bae8de470d70c6e0f00f3d43da91ef1c07f0521afbdf5f8e82d6326`.

The observer parameter was armed with 12 samples immediately before playback
and reset to zero immediately afterward. Playback used
`/usr/share/sounds/alsa/Front_Center.wav` at the existing default-sink volume
of 0.11. The graph accepted playback, 8 kHz VI, 24 kHz CPS, SP/SPVI setup and
`GRAPH_START` during the same transaction.

## Decoded observations

| Observation | Amplifier 0 | Amplifier 1 |
|---|---:|---:|
| samples / read-failure mask | 12 / `0x0` | 12 / `0x0` |
| PA enable / status | `0x01`, `0x2f/0x00` | `0x01`, `0x2f/0x00` |
| error / interrupt bytes | all zero | all zero |
| raw ADC span / unique words | `0x7220..0xc7e0` / 9 | `0x7020..0xc7e0` / 9 |
| raw temperature span / unique words | `0x72c0..0x7300` / 2 | `0x7080..0x7100` / 3 |
| raw VBAT span / unique words | `0xc580..0xc600` / 3 | `0xc5c0..0xc640` / 3 |
| WAVG / CPS control | `0x00` / `0x00` | `0x00` / `0x00` |
| current-limit register | `0x44` | `0x44` |

`CURRENT_LIMIT=0x44` decodes through the upstream WSA884x masks as current
override disabled and current-limit code `0x11`. That is exactly the driver's
PBR-enabled, 2-cell policy. The PBR policy is therefore present on both live
amplifiers during playback. This does not establish that PBR slave DP4 is
scheduled as a SoundWire data sideband; playback still selected sink ports
1/2/3 while VI selected source DP5 and CPS selected source DP6.

Both local `CPS_CTL` reads remained `0x00`. At the same time, both CPS DP6
streams were selected at 24 kHz with mask `0x03`, and the DSP accepted the
VI+CPS protected graph. Consequently the correct evidence boundary is:

- CPS transport and DSP graph integration are active;
- the local amplifier `CPS_CTL` bit meaning or required HLOS programming is
  unresolved;
- this capture does not prove a nonzero local CPS limiting action.

The zero WAVG value is recorded without interpretation. No fault was induced,
and no thermal, over-current or speaker-protection limit was deliberately
crossed.

## Observer design and reproduction

Patch `0042-ASoC-wsa884x-add-bounded-live-status-observer.patch` extends the
driver's existing PA health worker. On speaker POST_PMU it captures at most 40
read-only snapshots, at the existing 100 ms health interval, when the
`sp11_observe_samples` module parameter is nonzero. PRE_PMD cancels remaining
samples. The default is zero, so normal playback produces no additional
register traffic or log volume.

`tools/sp11_wsa_live_decode.py` converts the parseable journal records to the
reviewed JSON artifact. Its output deliberately labels ADC, temperature and
VBAT pairs as raw words.

## Remaining evidence questions

Linux no longer needs MAX34417 or another platform regulator observer for
per-amplifier live WSA telemetry. The remaining Windows/KD questions are the
exact CPS HLOS hardware-interface/threshold payload semantics, whether PBR DP4
is scheduled by Windows, and acoustic binding of amplifier identity to the
physical left/right speaker. Calibrated CPS gain, coil temperature, excursion
and actual limiter intervention also remain unproven because this firmware
rejects the known public dynamic DSP-statistics IDs.
