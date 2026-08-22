# UbiG M6: bounded WSA protection observer

Date: 2026-08-22
Candidate base: `6da6140`
Kernel: `7.1.5-sp11-render-parity-v4+`
Result: **PASS bounded active-PA common-readback gate**

The WSA884x kernel observer is present in the current Golden-v32-class kernel as module parameter `snd_soc_wsa884x.sp11_observe_samples`. It is instrumentation only: modinfo describes it as read-only 100-ms WSA status snapshots on speaker start. The parameter was armed for 12 samples per amplifier, a three-second nonzero stereo two-tone was sent through the live UbiG candidate with exact endpoint DSP mute asserted, and the parameter was explicitly returned to zero immediately afterward.

The observer produced 24 rows total, 12 from each physical amplifier. Sequence 0 on both sides was taken before PA enable and therefore reads the expected all-zero startup state except current-limit register `0x44`. Sequences 1 through 11 are the active-PA set. Every one of those 22 active rows matches the reviewed Golden observer's common fault/status contract exactly:

- PA enable `0x01`;
- status `0x2f/0x00`;
- error `0x00/0x00`;
- interrupt `0x00/0x00`;
- failed-read mask `0x0`;
- current-limit register `0x44`, code 17, override disabled;
- WAVG `0x00`;
- CPS local control `0x00`.

This is compared directly against `artifacts/reviewed/linux-render-parity-wsa-observer-20260814.json`, not just against hard-coded expectations. Both amplifiers also returned independent changing ADC/temperature/VBAT words while active. Those raw words are retained in the reviewed evidence but are not required to numerically match an older run: they are explicitly uncalibrated and depend on supply, temperature, and instantaneous operating state.

The UbiG filter-chain PID remained unchanged, the endpoint mute was cleared afterward, the visible volume stayed at 0.22, and the observer parameter is back at `0`. No new WSA error, interrupt, failed-read, SoundWire, xrun, or candidate crash condition was observed.

This advances the M6 PA/protection gate substantially: the source-owned userspace candidate does not disturb the bounded active-amplifier status/fault surface. It does not yet replace the final longer physical-output telemetry/acoustic comparison, because this run intentionally kept endpoint DSP mute asserted and therefore did not exercise audible speaker power.

Machine-readable evidence, including all 24 decoded samples, is `artifacts/reviewed/2026-08-22-ubig-native-candidate-wsa-protection-observer.json`.
