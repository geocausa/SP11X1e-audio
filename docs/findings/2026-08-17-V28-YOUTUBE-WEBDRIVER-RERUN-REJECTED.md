# v28 real-YouTube WebDriver rerun — rejected harness, no audio verdict

Date: 2026-08-17

## Decision

A stricter repeat of the historical real-YouTube seek complaint was attempted on the live v28 speaker stack. The run is **REJECTED AS AUDIO EVIDENCE** because the temporary Firefox/WebDriver browser profile failed to sustain representative YouTube seek/re-entry behavior.

Do not use this run to change L03, W03, or any physical-speaker verdict. The already accepted v28 local-file SP7-external physical seek gate remains the current objective L03 evidence.

## Audio state reached correctly

Before the rejected browser run, the live SP11 was still the committed v28 candidate. The normal Movie generation was recreated at the historical 25% endpoint state. Volume sync queued `postgain=-332` (`-20.750 dB`) and the Dolby host later acknowledged that exact value. Thus the audio-generation setup itself reached the intended matched state.

## Browser-control validation

The exact historical video was recovered as `Benny Benassi - Satisfaction` (`a0fkNdPiIL4`), duration `146.701 s`.

The clean WebDriver session could drive trusted pointer clicks on YouTube's progress bar with readback errors below 0.052 s:

- target 74 s -> 73.9960 s;
- target 57 s -> 57.0517 s;
- target 95 s -> 95.0481 s;
- return 42 s -> 41.9900 s.

Therefore the pointer-seek mechanism itself was valid.

## Rejection reason

During the measured real-browser run, seek 1 landed correctly near 74 s but YouTube entered buffering and did not resume within the acceptance window. A subsequent muted pre-warm attempt at `tiny` video quality showed the same profile-specific failure:

- 42 s segment: resumed successfully; buffered range approximately `40.001..60.001 s`;
- seek to 74 s: player reset to state `-1`, `readyState=0`, and the buffered-range list became empty.

This is a browser/media-source delivery failure, not an SP11 audio result.

A clone of the user's normal Firefox profile preserved AdGuard, but this Firefox/geckodriver combination would not create a WebDriver session with an explicitly supplied cloned profile (`Failed to set preferences: unknown error`). Direct `--remote-debugging-port` also exposed no listening endpoint in this snap build. No further browser-plumbing workaround was promoted into the audio investigation.

## Acoustic provenance / rejected SP7 capture

The external recorder used the SP7 Realtek Microphone Array, 48 kHz stereo S16, matching prior acoustic captures. The rejected directory is explicitly marked `REJECTED.txt` on SP7:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-youtube-seek-v28-25pct-20260817`

The 30 s WAV in that directory must not be cited as a three-seek acoustic result.

## Restore

After abandoning the harness, the SP11 was restored to the exact pre-test user state:

- profile Movie;
- VLLDP generation postgain `-785` (`-49.062 dB`);
- visible Dolby endpoint `17%`;
- zero-only activation was used to consume the restored pending postgain;
- ALSA PCM returned `closed`;
- WSA/VA runtime blocks returned to suspend;
- persistent GRUB fallback remained `sp11-audio-cps-v3`.

## Consequence

Do not spend more L03 time on temporary-profile WebDriver plumbing. The remaining L03 closure item is still the user's direct listening verdict on the already objective-clean v28 seek behavior, or a future real-browser capture made through a known-good normal browser session without media-source instability.
