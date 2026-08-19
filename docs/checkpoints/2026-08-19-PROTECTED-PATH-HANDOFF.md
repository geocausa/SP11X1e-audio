# SP11 protected-path handoff — 2026-08-19

## Mission
Reach Windows-equivalent protected physical speaker behavior on SP11 Linux before Movie/Music/Dynamic-EQ userspace tuning. Native Windows is the behavioral oracle. Golden v31 remains protected; invasive experiments use disposable candidates.

## Current branch / prior checkpoints
Branch: `agent/psycho-bass-20260818`

Prior durable checkpoints:
- `20a71f2` — guard hidden Dolby engine at unity; closes cold-boot/runtime restoration to 0.06 that had contaminated an early DIAG experiment with silence.
- `3113e37` — corrected Linux VI/CPS data-plane localization using an acoustically proven stimulus.
- `dcb0b31` — native Windows protection graph-birth checkpoint.

## Causal localization already closed
The remaining Windows physical level-dependent expansion is downstream of Dolby. Native Dolby/VLLDP is dynamic but at the decisive 630 Hz / 1 kHz rows it compresses the high-level stimulus relative to low; it does not generate the missing positive physical expansion. Replaying the exact Windows-processed trajectory through Linux protected bypass remained much flatter than Windows.

Golden v31 already strongly matches Windows on WSA producer state, WSA8845 lifecycle, protection topology/calibration, SP/SP_VI static configuration, reviewed IMCL links and relevant event registrations.

Native Windows graph birth is forced by restarting ADCM/qcadcm. Normal playback, `audiodg` recycle and AUCD restart do not rebuild the protected graph.

Hard Windows/Linux GET_CFG parity:
- SP `0x4027 / 0x080011e8`: 92-byte body SHA-256 `d46a099f70b2ac0ee001d2adc7f7db2e018e73fbcf6dd10db8034926e27eafa6` on both OSes.
- SP_VI `0x4024 / 0x080011f6`: 68-byte body SHA-256 `8abc3b615f4f57322365c5c921eb2dcba8a1a3736276b8758f2456e3a3a271f4` on both OSes.

Do not revive the public `0x08001259` theory: this retail Windows build did not materialize that parameter through the audited qcadcm/graph-open/GET/event/PRM paths.

## Corrected Linux DIAG result
After the hidden-engine unity bug was fixed and the speaker stimulus was independently proven by the SP7 microphone:
- Golden Linux tap 1 (`DATA_LOGGING 0x4003`, post-SP) carries real nonzero 48 kHz PCM.
- Linux VI tap 2 (`0x4025`, immediately after `CODEC_DMA_SOURCE 0x4026`) emits **no packets**, even forced immediate/out-of-island.
- Linux CPS tap 3 (`0x402a`, immediately after `CODEC_DMA_SOURCE 0x402b`) emits **no packets**, even forced immediate/out-of-island.
- SP7 simultaneously measured the 997 Hz speaker tone ~62–63 dB above the local spectral floor.

Linux hardware-side feedback is nevertheless live:
- WSA8845 sensing/ADC changes independently on both amps.
- SoundWire DP5/DP6 are enabled.
- WSA master ports 10/11/13 are live with expected geometry.
- endpoint SET_CFG records are accepted.
- v31 includes the reviewed per-slave CPS Offset1 and DP6 transport fixes.
- forcing the existing WSA-macro VI feedback helper changed the intended macro registers but did **not** resurrect tap 2; that diagnostic candidate is rejected.

Thus the Linux failure was localized to the LPASS/AFE source handoff into AudioReach `CODEC_DMA_SOURCE 0x4026/0x402b`, not the physical WSA sensor/wire geometry.

## Decisive new Windows LPASS DIAG result
Retail Windows LPASS DIAG has now been reached directly from userspace through Qualcomm IPC Router; the absent `qcdiagrouter8380` engineering provider is not required.

Windows evidence root:
`C:\Users\geoca\Documents\SP11-Audio-Audit-20260812\windows-protection-deconstruct-20260819`

Important files:
- `NEW-CHAT-HANDOFF-20260819.md`
- `WINDOWS-LPASS-DIAG-PROTOCOL-CHECKPOINT.md`
- `windows-lpass-1586-summary.json`
- `windows-lpass-1586-tap-summary.csv`
- `windows-lpass-1586-packet-stats.csv`
- `windows-lpass-1586-data.json`
- `windows-lpass-1586-control.json`
- `windows-lpass-1586-collector.ps1`
- `windows-lpass-diag-control-frames-decoded.csv`

Protocol facts:
- `QCIPC_ROUTER` = `qcipcrouter8380.sys` on `ACPI\QCOM0C0D`, local ProcessorID 1.
- `qsocketipcrum.dll` talks to `\\.\IPC`, private family `0x1b`.
- Full IPC namespace enumeration was already completed; do not redo it.
- Host publishes service `0x1001` instances 64 control, 66 data, 68 DCI.
- LPASS node 5 then advertises instance 65 CMD dynamically.
- passive FEATURE cmd8 mask is `0x0BBEF7`; negotiated intersection with the known-good Linux diag-router is `0x0008A053`.
- DIAG-ID v2 registrations are acknowledged as root=2, charger=3, audio=4, sensor=5.
- known-good log-mask sequence was ported and Qualcomm log code `0x1586` was captured successfully.
- bounded scripts close qsocket handles and verify host-published :64/:65/:66/:68 are absent afterward.

### Critical discriminator
Windows **does emit real VI and CPS samples through exactly the ADSP taps that are absent on corrected Golden Linux**.

From `windows-lpass-1586-tap-summary.csv`:
- tap 1 during: 113 packets, 109 nonzero, 48 kHz, 192-byte PCM payload, median RMS ~262.64.
- tap 2 during: 34 packets, **34 nonzero**, **8 kHz VI**, 640-byte payload, median RMS ~11,730,526.
- tap 3 during: 38 packets, **38 nonzero**, **24 kHz CPS**, 1920-byte payload, median RMS ~455,456.

Windows capture summary additionally records 195 data QRTR packets / 195 nHDLC packets and clean qsocket teardown.

This closes the old uncertainty about logger semantics or unused feedback branches: Windows tap 2 / tap 3 are real active streams. Corrected Golden Linux tap 2 / tap 3 are absent during a physically proven render.

## Current root-cause box
`WSA8845 VI/CPS producers -> SoundWire DP5/DP6 -> WSA master ports 10/11/13 -> [MISSING OR INCORRECT LINUX LPASS/AFE SOURCE HANDOFF] -> CODEC_DMA_SOURCE 0x4026/0x402b -> tap2/tap3 -> SP/SP_VI`

Windows crosses this box. Linux reaches the left edge but not the right edge.

## Do not redo
Do not restart:
- Dolby causal localization.
- DRE raw-zero / PA31.
- WSA producer transplant.
- basic WSA8845 cold/START/STOP lifecycle.
- static SP/SP_VI GET_CFG comparison.
- reviewed IMCL-link existence.
- generic DP5/DP6 geometry.
- `0x08001259` guessing.
- IPC service-space enumeration / LPASS node rediscovery.
- the question of whether Windows tap2/tap3 are active: they are directly captured and nonzero.

## Exact resume point
Use Windows as the oracle and identify the **first Windows host/AFE lifecycle operation that causes `0x4026` / `0x402b` to begin delivering samples**.

Recommended sequence:
1. Keep the working Windows userspace `0x1586` collector as the data-plane oracle.
2. Force true graph birth with ADCM/qcadcm restart while KDNET is armed.
3. Correlate the first nonzero tap2/tap3 timestamp with qcaucd/qcadcm/WSA/AFE operations.
4. Focus specifically on the host handoff between already-live WSA master source ports and LPASS CODEC_DMA_SOURCE; static SPF configuration is already closed.
5. Compare that exact lifecycle operation against Golden q6apm + WSA macro + machine-driver code.
6. Port only the missing operation to a disposable v31-derived Linux candidate.
7. Linux promotion gate: nonzero tap2 @ 8 kHz and tap3 @ 24 kHz during an acoustically proven render, with no faults/regressions.
8. Then replay the exact Windows digital trajectory and rerun the physical consumer matrix to test whether the missing Windows expansion law appears or materially closes.

## Boot safety at handoff
SP11 returned to Golden v31 after the Windows oracle session. `/proc/cmdline` reports `sp11_entry=7.1.5-sp11-golden-v31-ckv-delta`; GRUB `saved_entry=sp11-audio-golden-v31` with empty `next_entry`.
