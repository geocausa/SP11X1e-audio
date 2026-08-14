# Handoff: existing-evidence audit for physical render parity

Date: 2026-08-14 (Europe/London)

## Objective

Explain and then reproduce the remaining physical Windows speaker advantage on
the Surface Pro 11 without adding guessed DSP or repeating completed captures.
The current Linux build is
`7.1.5-sp11-render-parity-v2+` on branch
`agent/render-parity-20260812`.

Fresh operator evidence on this build is:

- the paused-media fragment replayed by a volume notification is gone;
- ordinary live volume tracking behaves correctly;
- a sharp slider/transition spike remains audible;
- ordinary YouTube tonality is much better than two weeks earlier but is still
  not close to Windows parity.

Suspend/resume is explicitly outside this investigation.

## Do not start with another Windows capture

Substantial Windows evidence already exists and must be exhausted first:

- the reviewed July-23 QGPR/KD graph and complete protection-calibration
  startup corpus;
- the July-26 ETL/state set including real YouTube playback;
- the Aug-7/11 state-pinned Dolby and live callback captures;
- the Aug-10 qcaucd command-FIFO session containing **328 decoded Windows
  driver writes** over three playback initializations;
- the Aug-11 semantic qcaucd DP6 trace and static SoundWire port-template
  reconstruction;
- the exact static qcadcm/qcaucd/ACDB 2S, 4-ohm, 18-dB, OCP, sensing and PBR
  threshold evidence;
- the Linux live WSA register and telemetry observations.

The Aug-10 raw files were recorded on the SP7 as:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_DP6_SLAVES_20260810_2007BST_25f4_2026-08-10_20-07-15-660.log
C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_DP6_SLAVES_20260810_2007BST-extract.txt
C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_SWR_RUNTIME_20260810_1909Z_2d7c_2026-08-10_19-09-12-880.log
```

Their hashes and the reviewed DP6 extraction are in
`artifacts/reviewed/2026-08-10-windows-cps-dp6-runtime.json`.

First recover those retained raw logs and decode all non-DP6 records.  Build a
functional Windows-versus-Linux matrix covering render DAC/COMP/BOOST, amp
initialization, gains, compander, softclip, sensing, OCP, PBR/current limit and
teardown.  Cross-check it against the static qcaucd/ACDB profile and current
Linux driver/register state.

The July Gemini report
`Gemini/docs/sp11_audio_parity_captured_telemetry.md` is not a substitute for
the raw corpus.  Its claim of a 100% complete six-boundary capture was audited
and rejected: the returned document preserved only three outbound packets and
one GET_CFG sample, with no raw debugger transcript, selector, SET_CFG, OOB or
scenario markers.  See
`docs/audit/2026-07-26-returned-windows-capture-initial-assessment.md`.

## Why the next boundary is below the Windows loopback

The active Linux userspace chain hosts the original SP11 ARM64 Dolby code in
the evidence-backed `VR -> VLLDP` sample order, applies the recovered Movie
profile, endpoint-volume feedback and exact AudioEngine final limiter.  A
state-matched Windows/Linux deterministic comparison reached correlation
`~0.99999947`, fitted gain `~1.00016`, and residual SNR `~59.8 dB`.

Linux also has the exact Windows endpoint taper, final `VOL_CTRL` ramp and all
30 volume-dependent MSIIR GainStep transactions.  Those facts do not prove
physical parity, but they make an invented userspace EQ, Bass Boost, Virtual
Bass, HRTF/ASAR or DRC stage the wrong next move.

The next discriminator is a matched comparison of the downstream AudioReach,
speaker-protection, SoundWire and WSA8845 state that Windows loopback does not
observe.

## Required reading

Read completely before attaching a debugger:

- `docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md`;
- `docs/architecture/2026-08-11-windows-speaker-audio-path-evidence-bound.md`;
- `docs/runbooks/2026-08-11-kd-cps-runtime-validation-handoff.md`;
- `docs/findings/2026-08-11-windows-render-mode-live-comparison.md`;
- `docs/findings/2026-08-11-qcaucd-cps-static-port-template-origin.md`;
- `docs/findings/2026-08-13-WINDOWS-FINAL-VOLCTRL-RUNTIME-ACTUATOR-GAP.md`;
- `docs/findings/2026-08-13-WINDOWS-GAINSTEP-CALIBRATION-TRANSACTION-BOUNDARY.md`;
- `docs/findings/2026-07-29-wsa884x-sp11-4ohm-profile.md`.

## Fixed evidence and safety boundary

Hash-lock the Windows binaries before using any RVA:

| Binary | Required SHA-256 |
|---|---|
| `qcadcm8380.sys` | `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429` |
| `qcaucd8380.sys` | `bd0c8276c51fc7a020c616e904dd613b6ccf187ec3e1fe6f94c2c811c8adc8bf` |
| `qcaudminiport8380.sys` | `79b26804d05332304c736c4e03e942db6a07ea886a2b07f3a4ff5947d1d05531` |

Use `kd-mcp` as the only debugger owner.  Open a persistent raw log before
setting breakpoints.  Use bounded, read-only execution breakpoints and capture
driver-owned arguments/objects.  Never directly read or write physical MMIO,
SoundWire registers, WSA slave registers, DSP memory or live driver state;
direct physical reads previously caused fatal `0x124` failures on this target.
Before detach, break in, run `bc *`, close the log and use `qd`.

Do not enable PBR DP4, add a DSP module, change a coefficient, or write a WSA
register merely because a static template exists.

## Matched stimulus/state for any proven residual hole

Use ordinary Edge/YouTube stereo playback, which is already runtime-proven to
select DEFAULT / GKV 2.  Pin and record:

- endpoint identity and enhancement/spatial state;
- Dolby profile and all current DAX state relevant to VR/VLLDP;
- endpoint scalar and exact dB value, preferably the already-characterized
  25% / `-20.7474098205566 dB` state;
- cold/idle start time, playback start time and at least 20 seconds of steady
  playback before the state snapshot;
- one bounded volume transition and one seek, with exact timestamps, after the
  steady snapshot.

Do not mix DEFAULT and NOTIFICATION graphs in the steady-state comparison.

## Evidence-audit priorities

1. Decode and classify every retained Aug-10 qcaucd FIFO record, not only the
   already-reviewed DP6 subset.  Compare it with the exact static WSA profile
   and Linux's live state.
2. Reuse the existing AudioReach startup/calibration and selected-CKV corpus to
   confirm final `VOL_CTRL 0x4a63`, the four-frame `MSIIR 0x489e` GainStep
   transaction and root protection calibration.  Do not recapture these
   already-closed boundaries.
3. Determine from the retained qcaucd data and static caller graph whether
   ordinary Windows playback requests the known DP4/PBR
   schedule (slot 7 / slave DP4 / shared master port 7), rather than merely
   carrying the static template.  A runtime request or programming event is
   required for a positive result.
4. Inventory the retained passive SP/SPVI/CPS state, events and calibrated
   measurements before declaring any telemetry field missing.
5. Bind WSA identities `0x0000000402170220` and
   `0x0000000402170221` to physical chassis left/right and normal render channel
   assignment using safe driver-owned evidence plus an operator channel test.
6. Only after this audit, name the exact absent record needed around a bounded
   volume transition or seek.  If the existing corpus cannot answer it, design
   one narrow read-only trace for that record alone.

## Deliverables

Return:

1. recovered untouched timestamped debugger logs and command transcripts,
   stored locally under ignored `artifacts/raw/` until reviewed for secrets;
2. hashes and module identities for the capture;
3. machine-readable per-amp state/transaction records with observation versus
   inference clearly separated;
4. a Windows-versus-Linux diff keyed by functional register/parameter, not just
   raw address;
5. one concise finding that identifies either a proven missing Linux action or
   the next uniquely discriminating read-only capture;
6. no Linux implementation change unless the Windows evidence proves the exact
   behavior to reproduce.

## Acceptance rule

This handoff succeeds when existing evidence produces an evidence-backed downstream mismatch
that plausibly explains the physical tonal or transition difference, or when a
complete matched state comparison proves that boundary equivalent and moves
the investigation elsewhere.  A new Windows capture is justified only after a
specific missing record is demonstrated.  A static capability, module
residency, an INF declaration, or a guessed psychoacoustic explanation is not
sufficient.
