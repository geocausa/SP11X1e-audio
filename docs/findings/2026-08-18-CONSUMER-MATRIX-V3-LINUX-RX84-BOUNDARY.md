# Sequential consumer matrix v3 — Linux RX84 boundary

Date: 2026-08-18
Status: **Linux side complete; matched native-Windows v3 matrix pending**

## Purpose

The earlier matched RAW v2 multisine proved a real Windows/Linux physical residual
downstream of a nearly identical digital Dolby/APO boundary, but the simultaneous
harmonically-related tones could not distinguish linear consumer gain from
harmonic/intermodulation energy landing back on analysis bins.

v3 therefore excites exactly one frequency and one source channel at a time, at
three source peaks (`0.0125`, `0.05`, `0.2`).  A stereo 1-kHz sync marker precedes
72 isolated conditions.

Source:

- generator: `tools/generate_sp11_consumer_matrix_v3.py`;
- WAV SHA-256: `ED983FB77F7F42FF4F593D75C981AD41E26F25EAE7FD46D23C49A9867A8558FE`;
- schedule SHA-256: `1068DF697FBFB5F4880895E3A574CB69BA77C7BDABF83194D94CC3DE726AB32B`;
- 48 kHz PCM16, 78.0 s;
- frequencies: `100,250,315,500,630,1000,1250,1600,2000,2500,4000,6300 Hz`;
- source levels: `0.0125`, `0.05`, `0.2`;
- left/right conditions are interleaved, one tone only;
- analysis window is the central 0.5 s of each 0.75-s tone.

## Linux acquisition

Golden v31 remained the booted kernel.  The active-RX84 userspace candidate was
used at a 25% Windows-Dolby endpoint state.  Two complete matrices were played
inside one stable protected graph generation.

SP7 external RAW microphone:

- endpoint hardware gain: exactly `0.000 dB` for the full capture;
- WAV SHA-256: `8ACE8E2A8A0CC3DAF065B8F2C6C8ECE674DC179FC0D4B918E6A6CAF67AFC836E`;
- physical-analysis JSON SHA-256: `93686019A5061578D9EE2996936D2663247D05E33A266438D69DF6AA2A5C4DD3`.

Linux hidden-hardware-sink monitor (valid post-Dolby digital tap):

- WAV SHA-256: `44FCAC279C6BECCB0FBA03D94546806BA241A862DAC21BE8BED832D69F5153FD`;
- digital-analysis JSON SHA-256: `969B8D20B5EA25F614656B2750827E607DBDFED8597246B330582668E1AB419A`.

The digital matrix is highly deterministic: median two-pass fundamental-level
change is `0.00256 dB`, p90 `0.01796 dB`, maximum `0.10052 dB`.

## Exact acoustic alignment

The full 1-kHz marker envelope, including its 50-ms linear ramps, was fitted in
both physical passes rather than using wall-clock filenames.

Physical marker starts:

- pass 1: `23.086 s` into the SP7 WAV;
- pass 2: `108.128 s`.

Physical source origins are therefore `21.086 s` and `106.128 s`; separation is
`85.042 s`.

The corresponding digital marker starts are `2.7852 s` and `87.8304 s`, giving
digital source origins `0.7852 s` and `85.8304 s`; separation is `85.045 s`.
The independent physical/digital fits therefore agree with the commanded run
spacing to a few milliseconds.

## SNR boundary

The `0.0125` physical rows are commonly at or near the room/microphone floor and
must not be used as an absolute transfer oracle.  Their two-pass changes can
reach several dB.  The stronger rows become highly repeatable, especially from
about 1.25 kHz upward and at source peak `0.2`.

This is intentional: v3 supplies a level ladder, and the analysis rejects rows
that do not repeat rather than interpreting room noise as level-dependent amp
behavior.

## Linux fundamental level dependence

For every frequency/channel pair where **both** the `0.05` and `0.2` physical
rows repeat within `1 dB`, consumer transfer was calculated as:

`20*log10(physical coherent fundamental / active digital coherent fundamental)`.

There are 17 such reliable pairs.  The transfer change from source peak `0.05`
to `0.2` is:

- median: **`-0.1069 dB`**;
- mean: **`-0.0405 dB`**;
- minimum: `-0.8785 dB`;
- maximum: `+2.3098 dB`.

Most reliable pairs are close to level-invariant.  Representative high-SNR
examples:

- 1250 Hz left `+0.14 dB`, right `+0.47 dB`;
- 1600 Hz left `-0.11 dB`, right `-0.30 dB`;
- 2000 Hz left `-0.50 dB`, right `-0.69 dB`;
- 4000 Hz left `-0.04 dB`, right `-0.03 dB`;
- 6300 Hz left `-0.88 dB`, right `-0.07 dB`.

The sparse outliers are not a broad compressor/expander curve.  Therefore the
active-RX84 Linux path does **not** exhibit a large generic level-dependent
fundamental-gain law over the reliably measured mid/high band.

This does not claim that the speaker is linear: physical harmonic content is
large in several lower-SNR/low-frequency rows.  It only separates fundamental
transfer from the multitone intermodulation ambiguity in v2.

## Next discriminator

Run the exact byte-identical v3 source on native Windows at the same 25%
endpoint state, with SP7 RAW capture explicitly gated to `0 dB` and native
WASAPI loopback recorded simultaneously.  Compare Windows physical/digital
fundamental transfer and harmonic generation against the Linux matrix.

If Windows isolated-tone fundamental transfer shows a repeatable level law that
Linux lacks, the missing consumer behavior is directly level-dependent.  If the
isolated fundamentals match but Windows harmonic output is stronger, the v2
residual is primarily nonlinear harmonic/intermodulation behavior rather than a
linear gain mismatch.
