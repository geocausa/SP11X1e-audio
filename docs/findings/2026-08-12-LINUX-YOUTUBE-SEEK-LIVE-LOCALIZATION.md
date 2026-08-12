# Live Linux YouTube seek localization — 2026-08-12

## Status

The user-reported Linux YouTube fast-forward/rewind spike is now digitally reproduced on the deployed SP11 Linux render stack. It is not a generic offline-Dolby theory and it is not created by the downstream PipeWire hardware sink.

## Real-browser harness

Firefox was launched through the logged-in GNOME session and driven through AT-SPI so the test used the real Firefox -> PipeWire -> `effect_input.sp11_windows_dolby` path. The player seek slider was focused through AT-SPI and moved with verified +/-5 second keyboard steps. This avoided false `set_current_value()` successes and coordinate-scaling errors.

A controlled localhost split-tone page first proved that generic Firefox `currentTime` seeking creates source silence/gaps but no obvious post-Dolby overshoot. The defect therefore narrowed to the real YouTube streaming/re-entry path.

## Accepted real YouTube capture

Capture directory:

`/home/geoca/Documents/SP11-AUDIO-AUDIT/live-youtube-capture-20260812`

Accepted two-tap run:

- `youtube-pre-dolby-real-seeks.wav`
- `youtube-post-dolby-real-seeks.wav`
- `youtube-real-seeks.json`

The active page was `Benny Benassi - Satisfaction - YouTube`. Verified seek transitions were approximately:

- 73 -> 104 s
- 106 -> 77 s
- 79 -> 125 s

The backward seek reached a post-Dolby 5 ms peak of about `0.2391` roughly 436 ms after the seek, compared with a nearby pre-seek post-Dolby peak around `0.1267`. The first forward seek also showed a large post/pre gain excursion during stream re-entry.

## Three-boundary localization

A second run recorded simultaneously:

- pre-Dolby virtual-sink monitor
- direct `effect_output.sp11_windows_dolby` output
- ALSA hardware-sink monitor

Files:

`/home/geoca/Documents/SP11-AUDIO-AUDIT/live-youtube-capture-20260812/triple/`

Verified seek transitions:

- 44 -> 74 s
- 77 -> 57 s
- 60 -> 95 s

After aligning the independent recorder starts, the Dolby-direct and ALSA hardware-monitor streams are exactly the same samples:

- fitted scale: `1.000000`
- correlation: `1.000000`
- residual: zero

Therefore the downstream PipeWire hardware-sink path is not introducing the transient. The transient already exists at the Dolby filter output. This does not rule out additional physical Qualcomm/amp behavior below PipeWire, but it proves that the audible defect has an upstream digital component before that boundary.

## Rejected SILENT-buffer hypothesis

Production `chain_run()` currently enters VR with `Conn.flags=1` (`VALID`) for every LADSPA block, even an all-zero source block. Because Windows APO connections have explicit SILENT metadata, a detached candidate changed exact all-zero blocks to `flags=2` (`SILENT`) before VR.

Candidate worktree:

`/tmp/sp11-silent-flag-candidate`

Candidate binary:

`/home/geoca/Documents/SP11-AUDIO-AUDIT/silent-flag-candidate-20260812/sp11_dolby_windows_chain.silent-candidate.so`

SHA-256:

`c27d27f1f3f232ab8b6017cd5f788e4c216c9d7be85808293120a9bb66085acb`

The captured real YouTube pre-Dolby WAV was processed offline through both the deployed binary and candidate at Movie / postgain -332 / 1024-frame host quantum. The candidate changed only tiny samples inside silence gaps (maximum absolute difference about `1.54e-4`) and did not change any tested post-gap re-entry peak or state. The hypothesis is rejected. The candidate was not deployed.

## Matched Windows follow-up

A later matched Windows real-YouTube run was captured at the same 25% endpoint state (`-20.74741 dB`) with verified video-element seek transitions approximately `43.97 -> 74.25 s`, `76.43 -> 57.14 s`, and `59.30 -> 95.02 s`. Windows WASAPI loopback also reaches roughly `0.987` full-scale during post-seek re-entry, despite the physical Windows speakers sounding smoothly capped. Therefore the audible smoothing is downstream of the Windows loopback boundary, not evidence of a missing userspace Dolby final limiter.

Review of the Windows DEFAULT AudioReach graph then identified a concrete Linux omission: the fourth module control link `POPLESS_EQ 0x4664 <-> VOL_CTRL 0x4663` with intent `INTENT_ID_P_EQ_VOL_HEADROOM (0x08001118)`. The deployed three-link topology omitted this record because `build_sp11_protected_topology.py` selected only `record_blocks[0]`. A corrected four-link isolated topology has since booted successfully and reached `GRAPH_START`; physical/WSA-side seek validation is the next discriminator.
