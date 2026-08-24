# UbiG M6 fresh Windows acoustic gate

Date: 2026-08-23
Status: **objective physical/acoustic gate PASS; operator promotion verdict still pending**

## Why a fresh Windows capture was required

The first UbiG Movie matrix was initially compared with the retained Aug-21
Windows Golden-v32 capture. Digital drive was close but the physical comparison
showed several multi-dB row offsets. Because the digital boundary did not carry
those offsets, stale room/microphone geometry was the leading explanation.

The SP11 was therefore rebooted through a one-shot EFI `BootNext` into native
Windows. The normal firmware order and saved GRUB default were left unchanged.
Windows was then measured again in the **same current SP7 geometry** used for the
UbiG capture. Afterward the machine returned to Golden v32 Linux.

## Matched conditions

The source is the deterministic 78-second consumer-matrix-v3 WAV:

`ed983fb77f7f42ff4f593d75c981ad41e26f25eae7fd46d23c49a9867a8558fe`

Native Windows Dolby state was read directly from the Dolby Access UWP settings
hive and decoded as:

`{"IntelligentEqualizerType":"Off","Type":"Movie"}`

The UbiG side therefore used the recovered `Movie` profile. Both systems used:

- 25% visible speaker endpoint;
- two complete matrix passes;
- the same SP7 RAW capture endpoint;
- SP7 hardware capture gain exactly 0.000 dB, unmuted;
- coherent single-tone fundamental extraction;
- same-run digital normalization;
- <=1.0 dB repeat delta as the row-admission rule.

Windows endpoint state at the test point was independently recorded as 25% /
`-20.74741 dB` / unmuted. Its pre-test 10% endpoint was restored afterward.
The Linux side restored the saved Custom profile, exact 20-band EQ and 14%
endpoint after the comparison.

## Fresh evidence identities

Physical SP7 RAW:

- Windows: `205c57da4b52e7c8de1ae52064b58032228c052e7173605235cd223dc6362a78`
- UbiG: `1e84911defa35c5622cf0de9a598e831021d7da31fba75bcdb6297637d5bfa37`

Digital boundary captures:

- Windows WASAPI loopback:
  `4b418c6769f3c982db7126cf6405b7d290d445c08ba456f7797084b7e335f1e1`
- UbiG post-DSP PipeWire capture:
  `674485308a6aaa8d404e797b6498e36ce476c80d1b6ca05dce2956b845c077a7`

The complete reviewed row comparison is
`artifacts/reviewed/2026-08-23-ubig-m6-fresh-windows-acoustic/fresh-windows-vs-ubig-comparison.json`.
Raw WAVs are preserved outside Git under
`/home/geoca/Documents/SP11-PROJECT/00-RE-archive/ubig-m6-acoustic-20260823/`.

## Decisive normalized transfer result

On rows repeat-stable on **both** systems, comparing physical output normalized
by that same run's digital fundamental:

| Band | Common rows | Mean absolute difference | UbiG-Windows bias | Max absolute |
| --- | ---: | ---: | ---: | ---: |
| 315 Hz+ | 34 | **0.227030 dB** | -0.004343 dB | 1.010819 dB |
| 630 Hz+ | 30 | **0.221240 dB** | +0.015177 dB | 1.010819 dB |

The level-dependent 0.05 -> 0.20 transfer law also overlays. Across the ten
frequency/channel pairs stable on both systems, UbiG-vs-Windows law difference
is **0.264778 dB MAE**, +0.208133 dB bias and 0.767633 dB maximum absolute.

The raw physical-only common-stable comparison is looser (about 0.60 dB MAE at
315 Hz+ and 0.69 dB at 630 Hz+) because each OS's digital drive is not
byte-identical. The same-run normalized transfer is the correct speaker/consumer
comparison and is substantially tighter.

This fresh result also explains the earlier stale-capture discrepancy: the
multi-dB offsets disappear when Windows and UbiG are recorded in the same
current room/microphone geometry.

## Real-program consecutive A/B

The operator then heard consecutive native-Windows and UbiG versions of the
same retained program source:

`The White Stripes - Seven Nation Army (Official Music Video).mp3`

SHA-256:
`951a65cc63fee17622485c1d94708614005524c7e20f86d3d815327f6bd0e8b3`.

Both used the 19-49 second excerpt, Movie profile and 10% endpoint. SP7 RAW and
digital captures were preserved for both. These captures are retained as the
operator-listening sanity pair, not as the quantitative matrix oracle, because
Windows MediaPlayer and Linux GStreamer expose different playback-start
latencies.

No subjective verdict is inferred from the fact that the A/B was played.
Promotion still requires the operator to explicitly accept the listening result.

## Post-return safety gates

After returning to Linux:

- boot marker is Golden v32;
- saved GRUB default remains `sp11-audio-v32-feedback-exact-golden`;
- one-shot EFI `BootNext` is cleared;
- Golden-v32 verifier: **PASS**;
- saved Custom profile and all 20 EQ values restored exactly;
- visible endpoint restored to 14%;
- UbiG control request/ack `2/2`, postgain request/ack `1/1`, `last_error=0`;
- filter-chain and volume-sync services active with zero restarts;
- no new WSA/SoundWire/XRUN/GLINK/kernel audio fault matched after the return boot;
- native candidate control test: PASS;
- Python regression suite: **211 passed, 3 skipped, 6 subtests passed**.

## Decision

The **objective M6 physical acoustic matrix gate is GREEN and closed**. Golden
v32 rollback remains the protected baseline and the Windows userspace bridge is
retained until the operator explicitly accepts the subjective A/B and requests
promotion.
