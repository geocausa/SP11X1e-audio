# Surface Pro 11 (X1E) audio for Linux

Evidence-driven Linux built-in-speaker enablement and Windows-parity work for
the Microsoft Surface Pro 11 with Qualcomm X1E80100 AudioReach, SoundWire and
dual WSA884x amplifiers.

> **Current recommended state — GOLDEN v31 (2026-08-18).** v31 is the
> promoted/default SP11 Linux built-in-speaker stack. It inherits Golden v28's
> static/lifecycle/seek breakthrough, v30's exact Windows endpoint DSP mute and
> DP1/DP3 transport completion, and adds Qualcomm prior/new GainStep CKV delta
> semantics. That last fix collapsed the reproducible 40-Hz Volume-Up transient
> from v30 `2.7855e-3` HP500 p95 to `6.6466e-5` and `6.4095e-5` on two
> independent v31 captures; native Windows measured `6.1937e-5`. Golden v28 is
> retained as rollback/comparison and CPS-v3 as rescue. A userspace-only
> **active-RX84 / 0 dB producer policy candidate** is now objectively GREEN on
> `agent/psycho-bass-20260818`: native Windows directly proves RX0/RX1 = 0 dB,
> the candidate applies that state only after protected producer wake, and it
> preserves the v31 40-Hz CKV, exact-mute and deterministic-seek closures. The
> candidate is deliberately **not yet merged into the Golden/main recipe**;
> normal-listening/bass audition is its promotion gate. The project remains
> **AMBER overall** for that operator verdict, remaining RAW L/R/upper-bass
> characterization, and clean-source packaging.
>
> Start with [`deploy/golden-v31/`](deploy/golden-v31/), the
> [`Golden v31 promotion`](docs/deployment/2026-08-18-GOLDEN-V31-PROMOTION.md),
> the [`CURRENT handoff`](docs/checkpoints/CURRENT-SP11-AUDIO.md), and the canonical
> [`render-parity ledger`](docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md).

## Recommended boot set

| Entry | Role | Status |
|---|---|---|
| `sp11-audio-golden-v31` | Daily driver | **Promoted/default** |
| `sp11-audio-golden-v28` | Rollback / comparison | **Keep** |
| `sp11-audio-cps-v3` | Conservative rescue | **Keep** |

Historical one-off candidates are retained in findings/patches/candidate
archives, not as dozens of active GRUB entries.

## What Golden v31 reproduces

Golden v31 includes the full accepted v28 baseline plus the promoted v30/v31
parity deltas:

- protected AudioReach speaker graph with SP/SPVI protection and CPS feedback;
- original matching SP11 Dolby VR/VLLDP code on Linux, effective Movie profile;
- correct pre-Dolby PCM boundary and frozen-per-generation VLLDP postgain lifecycle;
- measured Windows endpoint taper and 2% media-key step;
- final AudioReach `VOL_CTRL` Q28 with Windows left-new/right-old -> both-new ordering;
- all 30 recovered volume-dependent GainStep/MSIIR rows;
- Qualcomm prior-CKV -> new-CKV changed-key calibration semantics (`vol->vol`,
  `cal->vol`, `vol->cal`), closing the pathological 40-Hz Volume-Up transient;
- exact final endpoint DSP mute at `0x4a63 / 0x08001039`, with hardware mute
  retained only as fail-closed fallback;
- Windows SOFT_PAUSE and delayed-audio drain behavior;
- Windows-proven LPASS WSA producer implementation, with the active 0-dB lifecycle
  policy objectively validated on the current psycho-bass candidate branch;
- exact recovered WSA8845 63-write cold init, 10-write START and 6-write STOP;
- resident SoundWire clock-stop retention without replaying cold codec state;
- DP1/DAC `BlockCtrl3=0x00`, DP2/COMP `OffsetCtrl2=0x07`, and
  DP3/BOOST `OffsetCtrl2=0x1f`;
- demand-driven PA idle teardown;
- deterministic SP7 external-mic seek closure.

The canonical promoted identity is hash-pinned in
[`deploy/golden-v31/manifest.json`](deploy/golden-v31/manifest.json). Golden v28
remains preserved at [`deploy/golden-v28/`](deploy/golden-v28/) as rollback and
comparison.

## What remains open

1. **Active-RX84 operator listening verdict.** Objective 40-Hz, program,
   exact-mute, seek and lifecycle gates are GREEN on the psycho-bass branch.
   Audition normal music/YouTube, bass balance, mute and volume before merging
   that userspace policy into Golden/main. v31/v28/CPS rollback remains intact.
2. **Residual RAW L/R / upper-bass characterization.** The main low-bass deficit
   is now localized to the old Linux -3 dB WSA producer policy and corrected by
   Windows-proven active RX84. Any remaining channel/upper-bass residual must be
   measured with the tracked RAW recorder at a pinned endpoint gain; do not add
   guessed EQ or named Dolby bass effects. Older shared-mode absolute L/R dB
   figures remain provisional.
3. **Non-blocking research.** W02 is a Windows WASAPI-loopback-only branch
   question; P09 is protection telemetry observability. Effective CPS HLOS
   semantics are closed as P10 GREEN.
4. **Pristine-source packaging.** The historical Phase91 platform baseline still
   needs normalization into a clean public replayable patch series.
5. Suspend/resume, microphone/input and Bluetooth remain outside the current
   built-in-speaker completion gate.

## Reproducing the current recipe

The repository deliberately separates redistributable integration from private
vendor material.

```bash
git clone git@github.com:geocausa/SP11X1e-audio.git
cd SP11X1e-audio
./deploy/golden-v31/verify-golden-v31.sh
```

For Dolby, place your own matching SP11 vendor DLLs in
`~/.local/lib/sp11-dolby/`; `deploy/dolby/build-production.sh` verifies their
pinned SHA-256 values before building and refuses mismatches. Private ACDB,
firmware and dumps are likewise not redistributed.

Once an exact Golden boot image has been built/placed at the manifest path:

```bash
sudo ./deploy/golden-v31/install-grub-entry.sh
```

That command verifies the image, creates the clean Golden GRUB entry and selects
it as the saved default. It **does not reboot**.

The complete pristine kernel build is intentionally not advertised as solved
until the old Phase91 platform baseline has been replayed into a clean patch
series. The current manifest + findings + patch history make the deployed state
auditable and reproducible from a known SP11 7.1.5 platform baseline without
pretending vendor bytes or unnormalized history are public source.

## Proven rollback baseline

`sp11-audio-cps-v3` remains the conservative rescue image. Its provenance is in
[`deploy/audio-cps-v3/DEPLOYMENT-PROVENANCE.md`](deploy/audio-cps-v3/DEPLOYMENT-PROVENANCE.md).
Every new kernel bake must still pass `tools/verify_sp11_kernel_bake.py` with its
final DTB before it can replace a connected development machine.

## Evidence policy

The recovered corpus contains strong evidence, incomplete fragments and prior
AI-generated analysis. This repository follows these rules:

- Bind claims to hashes, addresses, captures or reproducible live observations.
- Prefer the shipped Windows binaries and dynamic QGPR/KD captures over prose
  produced in earlier sessions.
- Record hypotheses as hypotheses and preserve corrected conclusions.
- Keep raw vendor firmware, ACDB and dumps untracked unless redistribution
  rights are known.
- Never overwrite a working kernel, DTB, topology or UCM installation; every
  candidate gets a separate rollback-safe boot entry.
- Do not treat a successful PCM open or graph start as proof that speaker
  protection is actively limiting the hardware.

## Repository map

| Path | Purpose |
|---|---|
| [`deploy/audio-clean/`](deploy/audio-clean/) | Accepted boot identity, hashes and Phase91 boot assets |
| [`deploy/ucm2/`](deploy/ucm2/) | SP11 ALSA UCM policy |
| [`deploy/pipewire/98-sp11-dolby-bypass.conf`](deploy/pipewire/98-sp11-dolby-bypass.conf) | Identity boundary reserved for Dolby work |
| [`patches/`](patches/) | Ordered Linux changes and their status |
| [`docs/audit/`](docs/audit/) | Windows/Linux transaction and provenance audits |
| [`docs/findings/`](docs/findings/) | Evidence-backed technical findings and corrections |
| [`docs/deployment/`](docs/deployment/) | Candidate identities, validation gates and outcomes |
| [`artifacts/reviewed/`](artifacts/reviewed/) | Small reviewed records safe to version |
| [`tools/`](tools/) | Capture, topology, ACDB and QGPR analysis utilities |
| [`tools/dolby/`](tools/dolby/) | State-pinned Dolby oracle parser and discriminating matrix-stimulus generator |

The accepted decision is documented in
[`2026-08-01-audio-clean-baseline.md`](docs/deployment/2026-08-01-audio-clean-baseline.md).
The Windows/Linux start sequence is in
[`2026-07-28-windows-linux-start-transaction-ledger.md`](docs/audit/2026-07-28-windows-linux-start-transaction-ledger.md).

## Reproducing the tracked analysis

Analyze a local state-pinned Dolby capture without committing the raw WAV/DMP:

```sh
python tools/dolby/analyze_state_pinned_oracle.py \
  --source-wav sp11-known-input-stimulus-48k.wav \
  --loopback-wav windows-loopback-20260807-075900.wav \
  --dump audiodg-2800-source27p5.dmp
```

Generate the next in-phase / left-only / anti-phase discriminator locally:

```sh
python tools/dolby/generate_stereo_matrix_probe.py diagnostic-stereo-matrix-75hz.wav
```


Lint and inventory a decoded or binary AudioReach topology:

```sh
./tools/ar_topology_lint.py topology.bin
./tools/ar_topology_inventory.py topology.bin --json inventory.json --markdown inventory.md
```

Decode Windows ACDB graph objects and their selector/connection metadata:

```sh
./tools/ar_graph_open_inventory.py 01e842_POOL.bin --offset 0x35d84
./tools/acdb_gkv_inventory.py GKVT.bin GKVL.bin --pool 01e842_POOL.bin --json windows-gkv.json
./tools/acdb_sclu_inventory.py 00ea12_SCLU.bin \
  --scde 00f4be_SCDE.bin --scdo 00f4f2_SCDO.bin \
  --pool 01e842_POOL.bin --json windows-sclu.json
```

Reconstruct the typed graph closure and decode the captured activation lists:

```sh
./tools/windows_graph_closure.py windows-bundle.json windows-sclu.json \
  --json windows-closure.json
./tools/qgpr_activation_inventory.py qgpr.decoded.csv windows-gkv.json \
  --json activations.json
```

Decode an armed CPS V3 live-observer journal without assigning physical units
to the raw register words:

```sh
journalctl -b -o short-monotonic -g 'SP11 WSA live sample=' \
  | ./tools/sp11_wsa_live_decode.py /dev/stdin --output wsa-live.json
```

The repository intentionally does not offer a blind one-command installer for
unknown machines. Deployment is tied to the exact SP11 hardware, hashes and
rollback procedure recorded under `deploy/` and `docs/deployment/`.

## Scope boundary

The archived PipeWire EQ is disabled and is not part of parity work. The Dolby
identity boundary must remain bit-transparent until the vendor processing is
understood well enough to implement and test it without hiding defects in the
kernel, topology or protection path.
