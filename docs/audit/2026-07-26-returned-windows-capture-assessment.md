# Returned Windows Capture Assessment — 2026-07-26

## Decision

The second USB return is a valid Windows user-mode and ETW capture set. It
materially improves the parity baseline, but it still contains no raw KD
transcript and therefore does not replace the byte-recovered July 23 QGPR/KD
graph evidence.

Every returned file is now represented in a machine-readable SHA-256
inventory. Large ETLs remain in the ignored raw-evidence tree; compact reviewed
inventories, tests, and conclusions are eligible for source control.

The evidence supports these implementation gates:

- the public render contract is two-channel PCM at 48 kHz;
- shared, shared RAW, and exclusive PCM16 rendering all complete;
- the attempted 44.1 kHz shared format is rejected before initialization;
- the core Dolby driver APOs and `SurfaceAPO.dll` remain mapped even at idle;
- application-layer Dolby processing DLLs map during all active snapshots and
  unload at final idle;
- Dolby UI state or mapped-module presence cannot be used as proof of DSP
  bypass or processing order;
- no new ETW fact changes the canonical Windows DSP DEFAULT graph, its
  protection calibration order, or the single-VI-transport design.

## Canonical intake

The non-duplicated returned set is preserved at:

`artifacts/raw/windows-target-20260726/`

The reviewed inventory records 154 files, 676,439,479 bytes, and the role and
SHA-256 of every file:

`artifacts/reviewed/windows-target-20260726/capture-inventory.json`

It contains:

| Evidence class | Count |
| --- | ---: |
| ETL traces | 3 |
| Windows state-snapshot files | 117 |
| WASAPI probe results | 8 |
| Capture tooling | 12 |
| Source manifests/inventories | 6 |
| Blank operator-note templates | 2 |
| Generated narrative reports | 3 |
| Other capture auxiliaries | 3 |

The three generated narratives are retained as lead indexes only. A statement
in those files is not promoted unless the underlying bytes independently
support it.

## Trace identity and coverage

| Trace | Bytes | SHA-256 | UTC interval | Audio events |
| --- | ---: | --- | --- | ---: |
| `sp11_audio_parity.etl` | 377,487,360 | `c22779e6a36d5f7c82cc4a62bb5689f9b5277471a8779fb0d211dd1a1d3923b2` | 10:00:52–10:05:32 | 1,436,991 |
| `sp11_audio_parity_extra.etl` | 144,703,488 | `2e3969d42edf6d754b2de259da4db7f8fbcbd673d0cca7eb86ecd2fda0912eaf` | 10:21:14–10:23:01 | 235,079 |
| `sp11_audio_parity_real_youtube.etl` | 148,897,792 | `0daf9412de5dcb8edc90a5be9f96b245c47ab9a433a7475cdc7c7037f04b58bf` | 10:26:40–10:28:16 | 518,261 |

The header-level ETL inventory is reproducible with
`tools/windows_audio_etl_inventory.py` and stored at:

`artifacts/reviewed/windows-target-20260726/etl-header-inventory.json`

The reader exposes the standard `Microsoft-Windows-Audio` provider
`{AE4BD3BE-F36F-45B6-8D21-BDD6FB832853}` plus kernel process, thread, image,
and sampling providers. The trace does not embed a provider manifest usable by
the Linux reader. Event IDs, tasks, opcodes, payload sizes, PIDs, activities,
and scenario-window counts are therefore retained exactly, but event names and
payload meanings are intentionally not guessed.

## Probe results

| Scenario | Requested contract | Result | Frames |
| --- | --- | --- | ---: |
| Dolby UI bypass/shared | 48 kHz, stereo, PCM16, 20 s | complete | 960,000 |
| Windows enhancements off/shared | 48 kHz, stereo, PCM16, 20 s | complete | 960,000 |
| shared RAW | 48 kHz, stereo, PCM16, 30 s | complete; RAW property accepted | 1,440,000 |
| exclusive | 48 kHz, stereo, PCM16, 20 s | complete | 960,000 |
| volume-step run | 48 kHz, stereo, PCM16, 45 s | complete | 2,160,000 |
| Dolby active | 48 kHz, stereo, PCM16, 20 s | complete | 960,000 |
| 44.1 kHz shared | stereo, PCM16 | rejected; no initialization | 0 |
| microphone coexistence | 48 kHz, stereo, PCM16, 25 s | complete | 1,200,000 |

The 44.1 kHz file reports `IsFormatSupportedHr=0x00000001` and
`UnsupportedFormatNoInitialize=1`. Its snapshot-directory label contains the
word `active`, but the probe itself proves that the requested render stream
never initialized. The label must not override the HRESULT.

The dominant audio-provider event-ID distributions for bypass-shared,
enhancements-off, and Dolby-active are nearly identical. Shared RAW retains the
same recurring structure at a longer duration. Exclusive mode has a sharply
different event volume and mix. Without the manifest this is structural
correlation, not proof that individual effects were or were not contributing
to the signal.

## Nine state snapshots

All nine snapshots lock the same target driver:

- `qcadcm8380.sys` version `1.0.0.7966`;
- 768,224 bytes;
- SHA-256
  `37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429`;
- expected-hash comparison `True`.

The snapshots cover two cold-idle points, shared bypass active, shared RAW
active, final idle, the rejected-rate interval, microphone coexistence,
extra-final idle, and real YouTube playback.

The first cold snapshot maps these relevant modules:

- `DolbyAPOvlldp150.dll`;
- `DolbyApoVr.dll`;
- `DolbyDax3Apo.dll`;
- `SurfaceAPO.dll`;
- `VirtualSurroundApo.dll`.

Every later active snapshot maps those core modules plus:

- `DolbyAudioProcessing.dll`;
- `DolbyHrtfEnc.dll`;
- `voiceclarityapo.dll`.

At both later final-idle snapshots, `DolbyAudioProcessing.dll` and
`DolbyHrtfEnc.dll` are absent again, while the three core Dolby driver APOs,
`SurfaceAPO.dll`, `VirtualSurroundApo.dll`, and `voiceclarityapo.dll` remain.

This establishes address-space presence and lifetime only. It does not
establish APO instantiation, composite-effect order, bypass state, coefficients,
or signal contribution. In particular, the Windows Dolby toggle being off
does not reduce the audio engine to a Dolby-free module set.

The inventory also groups the hashes of every same-named state file. Registry,
device, signed-driver, service, package, event-log, and CLSID snapshots can
therefore be compared without silently overlooking duplicate or changed files.

## Missing evidence

The returned set contains no:

- raw WinDbg or `kd-mcp` transcript;
- KD command output with timestamp/scenario markers;
- completed ACDB selector or SET/GET packet log;
- new OOB `GRAPH_OPEN` body;
- human-entered scenario observations;
- screenshots.

`scenario-notes.txt` is the untouched template. Claims in
`sp11_audio_parity_captured_telemetry.md`, `extra-capture.md`, or
`power_rails.md` that depend on absent debugger bytes remain unverified. The
packet samples reviewed in the initial assessment remain useful, including the
correction that `0x0100100F`/`0x080014F3` is the two-GPIO hardware-resource
request and not graph close.

## Engineering consequence

The new capture closes the public-format and module-lifetime questions needed
for the first protected Linux boot:

1. expose a strict 48 kHz/two-channel speaker frontend;
2. remove the current Linux equalizer from the parity path;
3. preserve a Dolby boundary as identity/bypass, without pretending the
   proprietary APO processing was reproduced;
4. use the byte-recovered Windows DEFAULT graph and exact calibration ordering
   as the DSP authority;
5. retain a single WSA VI feedback transport;
6. fail closed with amplifiers muted if protection calibration or VI bring-up
   fails.

The ETLs do not authorize inventing missing module edges or calibration. Those
remain governed by the reviewed July 23 graph and parameter captures.
