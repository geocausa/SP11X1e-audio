# Surface Pro 11 (X1E) audio for Linux

Evidence-driven Linux built-in-speaker enablement and Windows-parity work for
the Microsoft Surface Pro 11 with Qualcomm X1E80100 AudioReach, SoundWire,
dual WSA884x amplifiers, and the native **UbiG** userspace speaker engine.

> **Current recommended state — GOLDEN v33 + UbiG (2026-08-24).** The built-in
> speaker output/protection target is closed at measurement-level Windows parity.
> Golden v33 keeps the complete Golden-v32 AudioReach/WSA8845/SoundWire,
> mute/volume/CKV and VI/CPS lifecycle and adds one Windows-proven producer fix:
> physically materialize WSA `TOP_CFG1=0x03` after each enabled VI pair. This
> corrects pre-SPVI TAP2 from Linux `I,V,I,V` to native-Windows `V,I,V,I` from
> the first valid packet while leaving q6apm/SP_VI unmodified.
>
> UbiG is the production Linux userspace speaker-DSP identity. The visible sink
> is `effect_input.sp11_ubig`; active Linux nodes/services no longer use the
> proprietary Windows vendor name. Historical Windows-oracle material retains
> vendor identifiers only where needed for provenance.
>
> Fresh source-identical quiet-room Windows/Linux program A/B is within roughly
> **0.1 dB** across useful bands at both 10% and 50%. At 50%, the 10-second
> interior differs by **+0.027 dB broadband** and **+0.002 dB over 80 Hz–10 kHz**
> with envelope correlation **0.956**. A 20-cycle true-cold 50% protection soak
> alternating 160/997 Hz completed with **20 enables, 20 disables, 0 PA faults,
> 0 `err0=0x20`, and 0 XRUNs**.

Start with [`deploy/golden-v33/`](deploy/golden-v33/),
[`deploy/ubig/`](deploy/ubig/), the
[`Golden v33 promotion checkpoint`](docs/checkpoints/2026-08-24-GOLDEN-V33-PROMOTED.md),
and the [`CURRENT handoff`](docs/checkpoints/CURRENT-SP11-AUDIO.md).

## Recommended boot set

| Entry | Role | Status |
|---|---|---|
| `sp11-audio-golden-v33-topcfg1-physical-vi` | Daily driver | **Promoted/default** |
| `sp11-audio-v32-feedback-exact-golden` | Immediate fixed-initrd rollback | **Keep** |
| `sp11-audio-golden-v31` | Historical rollback | Keep |
| `sp11-audio-golden-v28` | Historical comparison | Keep |
| `sp11-audio-cps-v3` | Conservative rescue | Keep |

Recent SP_VI/TOP_CFG1 diagnostic candidates are superseded and must not remain
normal boot targets. In particular, the downstream SP_VI `[2,1,4,3]` experiment
is rejected: after the producer was corrected it became a double swap and
reproduced right-amplifier fault/static behavior.

## Golden v33 in one paragraph

Golden v33 is Golden v32 plus patch
[`0072`](patches/0072-ASoC-lpass-wsa-macro-SP11-materialize-Windows-TOP-CFG1.patch).
Native Windows physically writes WSA macro `TOP_CFG1=0x03` after each enabled VI
pair; Linux's regmap default did not guarantee that the physical register was
materialized. The v33 write changes TAP2 itself, before SP_VI, to Windows
`V,I,V,I`. `snd_q6apm` remains the Golden build (`687B16CF9C43B43E90C0746`).

The root module tree is synchronized to the same v33 WSA macro as the fixed
initrd, closing the future-initramfs-regeneration trap. Golden v32 fixed boot
assets remain untouched rollback.

## UbiG production userspace

UbiG is the source-owned native SP11 userspace speaker engine. Its public
48-kHz stereo path implements the accepted Stage A/Stage B behavior, seven
profiles, live Custom/20-band GEQ, endpoint postgain and the realtime control
page without loading proprietary Windows DSP binaries.

Canonical active names:

- stable plugin: `~/.local/lib/ubig/ubig-sp11.so`
- stable ALSA TLV helper: `~/.local/lib/ubig/tlv_write`

- visible/default sink: `effect_input.sp11_ubig`
- hidden engine: `effect_input.sp11_ubig_engine`
- engine output: `effect_output.sp11_ubig`
- diagnostic bypass: `effect_input.sp11_ubig_bypass`
- services: `sp11-ubig-volume-sync.service` and
  `sp11-ubig-monitor-link.service`

See [`ubig/`](ubig/), [`deploy/ubig/`](deploy/ubig/) and
[`ubig/docs/STATUS.md`](ubig/docs/STATUS.md).

## Verification

On the deployed SP11:

```bash
sudo ./deploy/golden-v33/verify-golden-v33.sh
```

For a clean source replay:

```bash
JOBS=8 ./repro/golden-v33/build-and-verify.sh
```

The v33 reproduction first passes the complete v32 clean recipe, then applies
only patch 0072 and requires the exact v33 source SHA, module srcversions and
runtime ELF digest.

## Packages / release

The release publishes:

- `ubig-control` — GTK4 UbiG profile / 20-band GEQ control application;
- `sp11x1e-audio-golden-v33` — hash-pinned v33 WSA root-module hardening,
  initramfs guard and boot identity verifier.

Neither package redistributes private Windows vendor binaries, ACDB, owner packs
or firmware.

## Evidence policy

- Bind claims to hashes, addresses, captures or reproducible live observations.
- Prefer native Windows runtime/binary evidence over generic Qualcomm assumptions
  when they disagree on this board.
- Preserve rejected experiments and corrected conclusions.
- Keep private vendor binaries, ACDB and owner-only tuning payloads out of Git.
- Every risky kernel experiment gets a rollback-safe boot path.
- Direct debugger physical WSA MMIO reads are not an accepted method on this
  machine.

## Repository map

| Path | Purpose |
|---|---|
| [`deploy/golden-v33/`](deploy/golden-v33/) | Current Golden identity / verifier / hardening |
| [`repro/golden-v33/`](repro/golden-v33/) | Clean v32→v33 source reproduction |
| [`deploy/ubig/`](deploy/ubig/) | Production UbiG userspace namespace/helpers |
| [`ubig/`](ubig/) | Native UbiG DSP engine/control source |
| [`packaging/debian/`](packaging/debian/) | Debian release builders |
| [`patches/`](patches/) | Ordered Linux source deltas |
| [`docs/checkpoints/`](docs/checkpoints/) | Promotion and current-state records |
| [`docs/findings/`](docs/findings/) | Evidence-backed technical findings |
| [`artifacts/reviewed/`](artifacts/reviewed/) | Small reviewed evidence records |
| [`deploy/golden-v32/`](deploy/golden-v32/) | Immediate fixed-initrd rollback baseline |

## Remaining scope

The built-in speaker output/protection target is closed. New work should begin
from Golden v33 + UbiG and be tracked as a separate subsystem target (for
example microphone/input, suspend/resume or Bluetooth) rather than reopening
the accepted speaker path without reproducible evidence.
