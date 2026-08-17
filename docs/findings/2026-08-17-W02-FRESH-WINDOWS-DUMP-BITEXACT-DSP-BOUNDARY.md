# W02 fresh Windows dump: DSP chain bit-exact through AudioLimiter — 2026-08-17

## Result

A fresh state-pinned Windows run closes the remaining ambiguity about whether the ~59.78 dB W02 residual belongs to Dolby, Surface MFX, AudioLimiter, final device format conversion, or Windows run-to-run state.

It does not belong to the reproduced speaker DSP chain through AudioLimiter.

Fresh Windows is deterministic and, on the loud 75-Hz trajectory where W02 is strongest, Windows and Linux are **bit-exact at every directly captured stage from source through AudioLimiter**:

`source -> Dolby VR -> VLLDP -> Surface MFX copy -> AudioLimiter`

The remaining W02 difference belongs to the Windows WASAPI loopback/tap boundary rather than a missing speaker-render DSP stage.

## Fresh deterministic Windows control

The SP11 was booted one-shot into Windows with the persistent GRUB fallback left at `sp11-audio-cps-v3`. The exact Aug-12 state was reproduced:

- playback app/path: `System.Media.SoundPlayer`, `Load(); PlaySync()`;
- source SHA-256: `FD5898DB52F2292C2D3F603CC0A9CE7C9A1128B5A6BEF89BA53AD52E184431CD`;
- endpoint scalar: `0.100000016`;
- endpoint attenuation: `-34.04602 dB`;
- unmuted.

Fresh clean loopback:

`C:\Users\Geoca\Documents\SP11-Audio-Audit-20260812\w02-movie-clean-20260817\windows-loopback-20260817-211646.wav`

SHA-256:

`1B60EE975937FFA519E76F14DE6B7FC6CF11DC3C36A13591BD3FA6A3920EA09B`

After independent source alignment, this fresh capture is **sample-for-sample identical over the complete 29.45-s program** to the retained Aug-12 clean oracle `1D73D8FE...6A709`. Therefore W02 is not Windows run-to-run adaptive variance.

## Fresh full-memory state pin

A second exact-source run took one full-memory `audiodg` dump at approximately source t=26.17 s, in the loud 75-Hz region:

- dump: `audiodg-11496-source26p1.dmp`;
- bytes: `119217888`;
- SHA-256: `9B5D2C104C98447068233AE3C5D00E7868E1E474BD9FCF974823DA684CEED680`;
- accompanying loopback SHA-256: `E0CB9CD92938AEC415C2D9EB9EF33F56B9C7E68476ADBC6F2446AB3349109B50`.

The dump-run loopback matches the clean deterministic trajectory until the ProcDump suspension point. ProcDump wrote the complete full-memory dump even though its one-dump termination returned exit status 1.

Current `AudioEng.dll`:

- version `10.0.26100.8972`;
- SHA-256 `843430C1516A2867FE716E89BCC35399E59E5040D992BFAFF7468EAB1CB63A93`;
- PDB `{86BD4A63-EA96-E509-EA46-34121370ED6E}`, age 1.

## Live graph

The full-memory CAPONode walk resolves:

`VirtualSurround -> Dolby DAX SFX -> AudioMeter -> CAudioVolume -> AudioConstrictor -> mixers -> ASAR -> Dolby DAX MFX/VLLDP -> Surface MFX -> AudioLimiter -> AudioFormatConvert`

The only real `CAudioRateConvertCMPT` CAPONode found in this trajectory is upstream, `h48 -> h49`; its valid 528-frame overlap is byte-for-byte identical. It is not a W02 actor.

## Direct Dolby wrapper/core capture

Fresh module bases:

- `DolbyApoVr.dll` `0x7ff839280000`;
- `DolbyAPOvlldp150.dll` `0x7ff8032f0000`;
- `AudioEng.dll` `0x7ff803770000`.

VR wrapper:

- object `0x1fe3c13c2f0`;
- core `0x1fe3c1dd808`;
- fill `128` frames.

VLLDP wrapper:

- object `0x1fe3c68c1f8`;
- core `0x1fe3c68c360`;
- fill `128` frames;
- postgain pending/applied `-545/-545`;
- system gain `0`;
- peak-level `0`;
- ceiling `0.9998999834060669`.

A detached Linux replay from the exact production source/control contract was instrumented only to expose intermediate buffers. Its final SHA-256 remained the canonical replay:

`d2b2a539304192f1b4d308fadca5a45e4bf39b0b6f8ada3b9688ac83e37ed781`.

At matching fill=128 / 75-Hz FIFO phase, the valid Windows and Linux staging buffers are **bit-for-bit identical**:

- VR input;
- VR output;
- VLLDP input;
- VLLDP output.

Windows VR output is also byte-identical to Windows VLLDP input, consistent with the previously proven transparent Microsoft interstitial path.

The remaining stable VLLDP integer differences at `core+0x1088` / `core+0x10b8` (`Windows 6`, Linux `4`) are the already-closed disabled sliding-bass boundary copies; prior original-setter A/B proved them sample-transparent and they are not reopened.

## AudioLimiter is directly bit-exact

The fresh Windows `h58 -> h59` limiter CAPONode retained a 480-frame block. Its h58 input block appears exactly once in the Linux VLLDP output stream near the same 75-Hz trajectory. At that same stream index:

- Windows limiter input == Linux VLLDP output: exact;
- Windows limiter output == Linux exact AudioLimiter output: **480/480 frames bit-exact**;
- max float difference: `0`;
- RMSE: `0`.

Thus the exact translated AudioEng limiter is not W02's source.

## Final device-format converter is identified but not the WASAPI residual source

The terminal CAPONode is `h59 -> h60`.

Connection descriptors decode directly:

- h59: stereo, 4 bytes/sample, 32-bit float, 48 kHz;
- h60: stereo, 2 bytes/sample, 16-bit, 48 kHz.

The live `CAudioFormatConvert` object selects:

`CAudioFormatConvert::ConvertFloat32ToInt16Dither_NEON`

at runtime VA `0x7ff803779e70` (matching PDB native section-2 offset `28272`).

Its dither table contains 4096 float values, standard deviation `~1.246e-5`, maximum magnitude `~4.55e-5` (~1.49 PCM16 LSB), and is therefore far too small to explain a ~28.5-LSB aligned loopback difference by itself.

The exact converter arithmetic was replayed for all 4096 possible dither cursors against the aligned retained Windows loopback block. Best residual remained ~`28.505` PCM16 LSB RMS; no cursor came close to a direct match. Therefore the saved WASAPI loopback is **not the terminal h60 device-format stream**.

The h60 output buffer is all zero in this particular dump because ProcDump froze `audiodg` while that terminal callback had not populated its output. No downstream valid CAPONode consumes h60.

## Current boundary

W02 is no longer evidence for a missing render-stage or Dolby mismatch. The fresh Windows trajectory directly closes:

- Windows repeatability;
- Dolby VR;
- VLLDP;
- transparent Microsoft interstitial path;
- Surface MFX copy;
- AudioLimiter;
- equal-rate h48->h49 rate conversion;
- terminal Float32->Int16 dither as the source of the observed WASAPI residual.

The remaining target is the **WASAPI loopback tap/mix branch itself**. The current PowerShell recorder obtains the default endpoint mix format via `IAudioClient::GetMixFormat`; its saved WAV does not reveal whether the incoming mix stream is float32 or PCM16 because it normalizes float input to PCM16 before writing. A fresh diagnostic capture should preserve the raw WASAPI packet bytes and log the exact mix format. If float32, those raw samples can be searched directly in the existing full-memory dump to identify the loopback producer boundary.
