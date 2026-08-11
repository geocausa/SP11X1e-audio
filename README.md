# Surface Pro 11 (X1E) audio for Linux

Evidence-driven Linux audio bring-up for the Microsoft Surface Pro 11 with the
Qualcomm X1E80100 AudioReach, SoundWire and dual WSA884x stack.

This project reconstructs the machine's Windows speaker pipeline from captured
transactions, its REV_0D ACDB, the shipped ARM64 Qualcomm drivers and controlled
Linux tests. Kernel/DT, DSP topology, ALSA UCM and PipeWire policy are kept as
separate layers so that an acoustic result is never mistaken for a driver fact.

> **Project state (2026-08-11):** the protected, non-Dolby playback baseline
> remains the accepted lower control. The isolated CPS V3 candidate now boots
> the full platform, carries both per-amplifier CPS DP6 streams, starts the
> VI+CPS graph and completes real playback; it is the operator-selected default.
> A bounded live observer also proves distinct changing raw WSA telemetry and
> the recovered PBR current policy on both active amplifiers. Local nonzero CPS
> limiting action and calibrated physical telemetry remain unproven. The Windows ARM64
> `DolbyAPOvlldp150 -> DolbyApoVr` path also executes on Linux as an experimental
> overlay, but one-to-one Windows parity is **not** yet certified. A state-pinned
> Aug-7 Windows run has now localized the remaining loud-75-Hz drive discrepancy
> to **before the inner VLLDP DSP**: the live VLLDP input is ~3 dB hotter than the
> deterministic source while normal production limiter controls are in force.
> Start current Dolby work with
> [`docs/audit/2026-08-08-DOLBY-INTEGRATION-STATUS.md`](docs/audit/2026-08-08-DOLBY-INTEGRATION-STATUS.md).
> This remains research-quality hardware enablement, not an upstream-ready driver
> or a general-purpose installation package.

## Current baseline

The accepted control is kernel `7.1.5-sp11-audio-clean+`, GRUB ID
`sp11-audio-clean`, built after `mrproper` from source commit
`f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`.

| Area | Accepted state |
|---|---|
| Render | 48 kHz, S16_LE, stereo AudioReach pull endpoint |
| Protection feedback | Both WSA884x VISENSE paths, 8 kHz/S32_LE/stereo |
| Speaker protection | SP/SPVI setup in captured Windows order; graph starts |
| Amplifier operating point | PA 24 (+27 dB), WSA digital 81 (-3 dB), both channels |
| PA_AUX | Normal variant-selected 0 dB (`0xdd`), not the retired 18 dB experiment |
| Dolby | `sp11_dolby_bypass`; no Dolby coefficients or dynamic processing |
| Operator result | Both speakers, usable ceiling, no dropout in the accepted control windows |

The baseline has the complete Ubuntu module catalogue and the required Phase91
Wi-Fi, touch, GPI and SPI overrides. Exact installed hashes and the acceptance
record are in [`deploy/audio-clean/README.md`](deploy/audio-clean/README.md).

The deployed CPS V3 candidate is separately identified and rollback-safe. Its
exact kernel, module, initramfs, DTB and runtime-observer evidence are in
[`deploy/audio-cps-v3/DEPLOYMENT-PROVENANCE.md`](deploy/audio-cps-v3/DEPLOYMENT-PROVENANCE.md)
and
[`2026-08-11-linux-cps-v3-live-wsa-observation.md`](docs/findings/2026-08-11-linux-cps-v3-live-wsa-observation.md).
Every fresh kernel bake must also pass `tools/verify_sp11_kernel_bake.py` with
its final DTB. This guards both halves of the SP11 Wi-Fi continuity fix—the
ath12k OF hook and the WCN7850 `disable-rfkill` board property—before a build
can replace the connected development system.

```mermaid
flowchart LR
    PW["PipeWire volume"] --> DB["Dolby identity boundary"]
    DB --> SRC["AudioReach pull endpoint"]
    SRC --> DSP["Recovered DSP render graph"]
    DSP --> WSA["WSA macro / SoundWire"]
    WSA --> L["Left WSA884x"]
    WSA --> R["Right WSA884x"]
    L --> VI["8 kHz V/I feedback"]
    R --> VI
    VI --> SP["SP + SPVI protection"]
```

The diagram shows functional flow, not a claim that every Windows policy branch
has already been reproduced.

## What is proven

- The Windows-compatible pull transport, OOB mapping, graph lifecycle replies
  and repeated-prepare behavior work under normal desktop playback.
- The reconstructed render graph has 29 recovered modules, 26 data edges,
  three internal control links and seven containers.
- Both amplifier V/I sources reach the render-coupled feedback backend.
- The captured SP/SPVI setup and graph start complete repeatedly.
- The upstream machine-driver PA-volume ceiling was the principal Linux
  loudness restriction. The accepted baseline removes that temporary ceiling
  only while the protected graph and VI path are present.
- Runtime DSP volume control and a narrowly allowlisted MSIIR parameter
  transport are proven. This transport is a prerequisite for later Dolby work;
  it is not Dolby processing by itself.
- Windows requests the full selected 10,464-byte/107-frame graph calibration.
  Qualcomm GSL treats status `3` from that calibration boundary as a warning
  and continues. Linux now follows that narrowly scoped policy.

The calibration conclusion is bound to the captured transaction, the recovered
ACDB and fresh decompilation of the hash-matched Windows `qcadcm8380.sys`; see
[`2026-08-02-windows-graph-calibration-warning-policy.md`](docs/findings/2026-08-02-windows-graph-calibration-warning-policy.md).

## Accepted and rejected candidates

| Candidate | Decision | Reason |
|---|---|---|
| `audio-clean` | **Accepted control** | Both speakers and stable restarts with the full Windows-selected calibration |
| `audio-clean2` | Rejected | Removed one 48-byte calibration frame contrary to Windows; reproduced right-only physical audio |
| `audio-mapdiag` | Rejected as a baseline | Inherited Clean2 and added invasive amplifier diagnostics; developed long starts and a channel failure |

Reading the full WSA884x debugfs register files is not passive: it populates the
regmap cache, which can later be replayed after a SoundWire detach. Even piping
that generated file through a filter still reads the entire map. The normal
collector therefore performs no WSA884x debugfs register walk; a full dump is
explicit opt-in only.

## What remains open

The accepted baseline is not yet a claim of one-to-one Windows parity. Before
closing the non-Dolby phase, the remaining evidence work is:

1. Resolve Windows' separate amplifier-reset lifecycle against Linux's stable
   shared-reset arrangement.
2. Recover calibrated protection telemetry or a passive record of actual
   limiter intervention. Linux now has live per-amplifier raw ADC, temperature
   and VBAT words, while known dynamic DSP-statistics IDs remain unsupported.
3. Recover the exact Windows HLOS CPS payload/threshold semantics. A reviewed
   Windows scalar scan plus every available Linux WSA884x source copy provides
   no evidence that amplifier-local `CPS_CTL=0x00` is a missing HLOS write, so
   that register is no longer a deployment blocker. Linux CPS DP6 transport
   and DSP graph integration are already proven on this machine.
4. Establish whether any genuine per-speaker calibration or hardware
   asymmetry exists. Device-tree left/right labels alone are not proof of the
   physical mapping.

Dolby/Surface APO behavior, coefficient recovery and transition handling are a
separate next phase. No equalizer or guessed dynamics should be added to the
baseline to imitate them.

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
