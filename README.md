# Surface Pro 11 (X1E) audio for Linux

Evidence-driven Linux built-in-speaker enablement and Windows-parity work for
the Microsoft Surface Pro 11 with Qualcomm X1E80100 AudioReach, SoundWire and
dual WSA884x amplifiers.

> **Current recommended state — GOLDEN v28 (2026-08-17).** The project now has
> a preserved daily-driver baseline rather than a moving experiment. v28 closes
> the confirmed CSR-off broadband static at the Windows/room floor, carries the
> recovered Windows WSA8845 lifecycle and endpoint/GainStep behavior, passes the
> objective SP7-external seek gate, and the user reports the best Linux sound of
> the project so far: stable/coherent music, excellent volume leveling and no
> audible forward/reverse seek spike during normal YouTube use. Desktop mute is
> now propagated fail-safe to the hidden hardware sink. Low-bass / psychoacoustic
> bass parity still awaits a direct Windows A/B, so the overall parity project
> remains **AMBER**, not “finished”.
>
> Start with [`deploy/golden-v28/`](deploy/golden-v28/), the
> [`Golden v28 consolidation`](docs/deployment/2026-08-17-GOLDEN-V28-CONSOLIDATION.md)
> and the canonical
> [`render-parity ledger`](docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md).

## Recommended boot set

| Entry | Role | Status |
|---|---|---|
| `sp11-audio-golden-v28` | Daily driver | **Recommended/default** |
| `sp11-audio-cps-v3` | Conservative rescue | **Keep** |
| `sp11-audio-v29-structural-test` | DP3 OffsetCtrl2 structural comparison | **Test only** |

Historical one-off candidates are retained in findings/patches/candidate
archives, not as dozens of active GRUB entries.

## What Golden v28 reproduces

- protected AudioReach speaker graph with SP/SPVI protection and native-width VI feedback;
- original matching SP11 Dolby VR/VLLDP code on Linux, effective Movie profile;
- correct pre-Dolby volume boundary and frozen-per-generation VLLDP postgain lifecycle;
- measured Windows endpoint taper;
- final AudioReach `VOL_CTRL` Q28 with Windows left/right update ordering;
- all 30 recovered volume-dependent GainStep/MSIIR rows, including the strong low-volume bass/loudness contour;
- Windows SOFT_PAUSE and delayed-audio drain behavior;
- Windows-proven LPASS WSA producer at 0 dB;
- exact recovered WSA8845 63-write cold init, 10-write START and 6-write STOP;
- resident SoundWire clock-stop retention without replaying cold codec state;
- DP2/COMP `OffsetCtrl2=0x07`, the causal prerequisite that removed the CSR-off broadband static;
- demand-driven PA idle teardown;
- user-facing mute propagation to the downstream hidden sink.

The exact runtime and boot identities are hash-pinned in
[`deploy/golden-v28/manifest.json`](deploy/golden-v28/manifest.json).

## What remains open

The remaining work is much narrower than the historical README implied:

1. **Bass / psychoacoustic-bass A/B.** Compare Golden v28 directly with Windows
   at matched level using the same material and SP7 external capture where useful.
2. **Exact runtime DSP mute.** User-facing mute works; Windows' exact
   `0x4a63/0x08001039` runtime transaction is still to be promoted separately.
3. **Non-blocking research items.** W02 is a dedicated Windows WASAPI-loopback
   branch identity question, not a speaker-quality blocker. Protection telemetry
   naming and HLOS CPS private-field semantics remain incomplete but are not
   known audible defects.
4. **Pristine-source packaging.** The integration starts from official Linux
   7.1.5 plus the SP11 Phase91 platform port. The historical kernel working tree
   is not itself Git-backed, so a one-command pristine-upstream kernel rebuild is
   not yet claimed. Normalizing that platform base is the next reproducibility job.
5. System suspend/resume, microphone and Bluetooth remain explicitly outside the
   current built-in-speaker completion gate.

## Reproducing the current recipe

The repository deliberately separates redistributable integration from private
vendor material.

```bash
git clone git@github.com:geocausa/SP11X1e-audio.git
cd SP11X1e-audio
./deploy/golden-v28/verify-golden-v28.sh
```

For Dolby, place your own matching SP11 vendor DLLs in
`~/.local/lib/sp11-dolby/`; `deploy/dolby/build-production.sh` verifies their
pinned SHA-256 values before building and refuses mismatches. Private ACDB,
firmware and dumps are likewise not redistributed.

Once an exact Golden boot image has been built/placed at the manifest path:

```bash
sudo ./deploy/golden-v28/install-grub-entry.sh
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
