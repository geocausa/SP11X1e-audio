# Surface Pro 11 (X1E) audio for Linux

Evidence-driven Linux built-in-speaker enablement and Windows-parity work for
the Microsoft Surface Pro 11 with Qualcomm X1E80100 AudioReach, SoundWire and
dual WSA884x amplifiers.

> **Current recommended state — GOLDEN v32 (2026-08-21).** v32 is the
> promoted/default SP11 Linux built-in-speaker stack. It keeps Golden v31's
> kernel, DTB, q6apm, canonical topology, exact mute/volume/CKV semantics and
> accepted WSA8845 transport fixes, then closes the remaining VI/CPS feedback
> dataplane with three Windows-proven deltas: protection TX clocks only after
> both PAs are active, active feedback `Offset2=0` on SoundWire ports 10/11/13,
> and the CPS controller wake/packetization state. On the **canonical topology**
> Linux now emits native VI TAP2 at 8 kHz / 640-byte payload and native CPS TAP3
> at 24 kHz / 1920-byte payload with Windows-range magnitudes. Repeated
> silence/tone stress, a -12/-6/-3/0 dB 997-Hz staircase, >8 h idle, and normal
> reboot gates completed with zero PA faults/recoveries and zero canonical
> GLINK timeouts. Golden v31 remains an explicit fixed-initrd fallback.
>
> Start with [`deploy/golden-v32/`](deploy/golden-v32/), the
> [`Golden v32 promotion checkpoint`](docs/checkpoints/2026-08-21-GOLDEN-V32-PROMOTED.md),
> the [`CURRENT handoff`](docs/checkpoints/CURRENT-SP11-AUDIO.md), and the canonical
> [`render-parity ledger`](docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md).

## Recommended boot set

| Entry | Role | Status |
|---|---|---|
| `sp11-audio-v32-feedback-exact-golden` | Daily driver | **Promoted/default** |
| `sp11-audio-golden-v31` | Fixed-initrd rollback | **Keep** |
| `sp11-audio-golden-v28` | Historical comparison | **Keep** |
| `sp11-audio-cps-v3` | Conservative rescue | **Keep** |

Historical one-off and forced-TAP candidates are diagnostic artifacts, not
normal boot targets. In particular, forced TAP2/TAP3 topology boots can stall
ADSP/GLINK teardown at reboot; Golden v32 does **not** require them because VI
and CPS are visible on the canonical topology.

## What Golden v32 reproduces

Golden v32 includes the accepted v31 baseline plus the now-closed feedback path:

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
- exact recovered WSA8845 63-write cold init, 10-write START and 6-write STOP;
- resident SoundWire clock-stop retention without replaying cold codec state;
- DP1/DAC `BlockCtrl3=0x00`, DP2/COMP `OffsetCtrl2=0x07`, and
  DP3/BOOST `OffsetCtrl2=0x1f`;
- Windows PA-before-protection-clock ordering, eliminating the early-candidate
  PA fault/static-ghost loop;
- active SoundWire feedback `Offset2=0` on ports 10/11/13;
- CPS wake/packetization state `0x105c=0x0005000f` + DP13 `0x1d54=3`;
- **native canonical VI TAP2: 8 kHz / 640-byte payload, nonzero/unique,
  Windows-range magnitude**;
- **native canonical CPS TAP3: 24 kHz / 1920-byte payload, nonzero,
  ~449–452k RMS versus native Windows ~455k**;
- demand-driven PA idle teardown and clean canonical reboot behavior;
- deterministic SP7 external-mic seek closure.

The promoted identity is hash-pinned in
[`deploy/golden-v32/manifest.json`](deploy/golden-v32/manifest.json). Golden v31
remains preserved at [`deploy/golden-v31/`](deploy/golden-v31/) as the immediate
rollback baseline.

## Current status / what remains open

The built-in-speaker **VI/CPS feedback dataplane and daily-driver lifecycle are
GREEN** on Golden v32. Remaining work is non-blocking relative to that closure:

1. **Speaker-quality characterization.** Continue RAW L/R / upper-bass and
   psychoacoustic comparison only with the fixed SP7 measurement fixture. The
   RX84 evidence/tooling is now in `main`, but subjective tuning remains separate
   from the v32 VI/CPS promotion gate.
2. **Pristine-source packaging.** Normalize the historical Phase91 platform
   baseline into a clean public replayable patch series; do not imply private
   vendor firmware/ACDB bytes are redistributable.
3. **Diagnostic teardown.** Forced TAP2/TAP3 topology is diagnostic-only because
   it can leave ADSP/GLINK teardown waiting during reboot. Canonical v32 feedback
   does not depend on it.
4. **Out-of-scope hardware.** Suspend/resume, microphone/input and Bluetooth are
   outside the current built-in-speaker completion gate.

## Reproducing the current recipe

The repository deliberately separates redistributable integration from private
vendor material.

```bash
git clone git@github.com:geocausa/SP11X1e-audio.git
cd SP11X1e-audio
./deploy/golden-v32/verify-golden-v32.sh
```

For Dolby, place your own matching SP11 vendor DLLs in
`~/.local/lib/sp11-dolby/`; `deploy/dolby/build-production.sh` verifies their
pinned SHA-256 values before building and refuses mismatches. Private ACDB,
firmware and dumps are likewise not redistributed.

Once the exact Golden v32 boot artifacts are present at the manifest path:

```bash
sudo ./deploy/golden-v32/install-grub-entry.sh
```

That command verifies the image, recreates the hash-pinned Golden v32 GRUB entry
and selects it as the saved default. It **does not reboot**.

The complete pristine kernel build is intentionally not advertised as solved
until the old Phase91 platform baseline has been replayed into a clean patch
series. The current manifest + findings + patch history make the deployed state
auditable from the known SP11 7.1.5 platform baseline without pretending vendor
bytes or unnormalized history are public source.

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
| [`deploy/golden-v32/`](deploy/golden-v32/) | Promoted Golden v32 manifest, verifier and GRUB installer |
| [`deploy/golden-v31/`](deploy/golden-v31/) | Golden v31 fixed-initrd rollback recipe |
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
