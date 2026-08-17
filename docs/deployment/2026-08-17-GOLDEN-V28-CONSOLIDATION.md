# Golden v28 consolidation — 2026-08-17

## Decision

The project now has a product-like daily-driver baseline: **SP11 Audio GOLDEN
v28**. New hardware/DSP experiments must not mutate this entry in place.

The user manually auditioned normal YouTube playback and reported the strongest
Linux result of the project: stable/coherent music, subjectively excellent
volume leveling, and no audible forward/reverse seek loudness spike in the
manual test. The earlier objective SP7-external-microphone three-seek gate had
already passed. L03 can therefore close for the current stack.

Low-bass / psychoacoustic-bass equivalence remains intentionally open until a
matched Windows-vs-Linux A/B. The existing volume-dependent MSIIR rows already
provide a concrete Windows-like low-volume loudness contour, but the user's
uncertainty is not converted into a parity claim.

## Why the improvement may have arrived in more than one symptom

The best current causal interpretation is that several previously separate
symptoms shared endpoint/PA-state dependencies. The full v28 stack combines:

- correct pre-Dolby vs endpoint-volume boundary;
- channel-ordered final VOL_CTRL + GainStep/MSIIR selection;
- exact WSA8845 three-state lifecycle and retained resident state;
- Windows-proven WSA producer gain and PA ordering;
- the DP2/COMP OffsetCtrl2 prerequisite that closes the CSR-off static.

It is plausible that correcting the PA/endpoint state removed both the strange
leveling behavior and the seek/re-entry loudness spike. This is a **working
causal hypothesis**, not yet a single-component proof. Do not attribute the
breakthrough to one PA write without a controlled regression.

## Desktop mute closure

The visible PipeWire node already exposed the correct mute state, but the live
volume-transaction synchronizer ignored that bit after it had handed endpoint
gain to the running DSP graph. Windows has a dedicated final VOL_CTRL
multichannel-mute parameter `0x4a63/0x08001039`; the current kernel transaction
control carries gain + GainStep only.

For Golden v28, the user-facing defect is fixed fail-safe in userspace: after
establishing the correct hidden hardware gain, the synchronizer mirrors the
visible mute state to the hidden downstream ALSA/PipeWire sink. Unmute restores
gain before opening the mute switch. This does not touch Dolby coefficients,
AudioReach gain selection, WSA programming or SoundWire state.

Focused regression: 29/29 tests. Broader volume/Dolby suite: 48 tests + 6
subtests. Live silent control-plane verification proved visible mute -> hidden
hardware `[MUTED]`, then restored both to their prior state.

Exact runtime DSP mute `0x08001039` remains a separate parity enhancement, not a
reason to destabilize the daily driver.

## Supported boot policy

Keep three SP11 audio entries:

1. `sp11-audio-golden-v28` — daily driver / saved default.
2. `sp11-audio-cps-v3` — safe rollback.
3. `sp11-audio-v29-structural-test` — comparison-only DP3 structural candidate.

Historical boot copies and GRUB scripts are archived by name/hash and removed
from the active menu. Their source candidates, findings and patches remain in
the project tree/Git history.

## Public reproducibility policy

`deploy/golden-v28/manifest.json` is the immutable deployment oracle.
`verify-golden-v28.sh` validates boot assets and runtime binaries.
`install-grub-entry.sh` registers only a hash-matching Golden image and never
reboots automatically.

The repo does not redistribute vendor Dolby DLLs, private ACDB/calibration or
firmware. The exact Dolby build helper verifies user-supplied DLL hashes. The
kernel lineage is official Linux 7.1.5 plus the SP11 Phase91 platform port and
the reviewed integration patches. A pristine upstream one-command kernel build
is not claimed until the historical Phase91 port is normalized and replayed as
a clean patch series.

## Completed boot cleanup

The consolidation was applied live without rebooting the running v29 comparison
session. Before pruning, every top-level artifact under the 52 historical
`/boot/sp11-*` directories was SHA-256 inventoried and all old SP11 audio GRUB
generators were archived. The reviewed copies are:

- `artifacts/reviewed/2026-08-17-pre-golden-boot-artifacts.sha256` — 188 file hashes;
- `artifacts/reviewed/2026-08-17-pre-golden-boot-dirs.txt` — 52 old boot directories;
- `artifacts/reviewed/2026-08-17-golden-v28-consolidation.json` — cleanup decision summary.

The active system now retains exactly three custom SP11 audio boot directories
and three custom audio GRUB generators. SP11 candidate usage under `/boot`
dropped from roughly 11 GiB to 1.1 GiB. `grubenv` now contains only:

```text
saved_entry=sp11-audio-golden-v28
```

There is no queued `next_entry`. The normal Ubuntu and Windows boot entries are
not part of this pruning and remain generated normally.
