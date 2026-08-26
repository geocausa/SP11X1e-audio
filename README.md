# Surface Pro 11 (X1E) native audio for Linux

Evidence-driven native **speaker output + internal microphone input** for the
Microsoft Surface Pro 11 with Qualcomm X1E80100 AudioReach, SoundWire, dual
WSA884x amplifiers, LPASS VA/TX macros, and the source-owned **UbiG** speaker
engine.

> **Current recommended state — FullIO v19c + UbiG (2026-08-26).**
> FullIO v19c combines the exact Golden-v33 protected speaker graph with the
> accepted native two-channel MicArray path. The kernel/initrd and microphone
> code remain the v18 0072+0078 production delta; the v19c change is the merged
> AudioReach topology and collision-free capture graph-object allocation.
>
> The retained Windows RAW ↔ Linux microphone A/B remains **98.27% overall
> parity**. FullIO additionally restores the exact endpoint volume/mute +
> volume-dependent MSIIR/GainStep path: active playback uses WSA RX84 and DSP
> endpoint mute, while idle returns to Golden RX81. Protected playback and
> MicArray capture run concurrently with clean runtime-PM teardown.
>
> System suspend/resume is deliberately outside this release gate and belongs
> to its separate dedicated RE.

The production desktop output selector exposes only **SP11 UbiG** for the
built-in speakers. The physical ALSA speaker remains an internal hidden backend;
the old transparent UbiG bypass is no longer autoloaded and is retained only as
historical diagnostic material.

Start with [`deploy/native-audio-v19c/`](deploy/native-audio-v19c/),
[`repro/native-audio-v19c/`](repro/native-audio-v19c/),
[`deploy/ubig/`](deploy/ubig/), the
[`FullIO v19c acceptance checkpoint`](docs/checkpoints/2026-08-26-FULLIO-V19C-GOLDEN-MIC-COLLISION-FIX-ACCEPTANCE.md),
the [`current v19c audit`](docs/audit/2026-08-26-SP11-NATIVE-AUDIO-FULLIO-V19C-AUDIT.md),
and the [`CURRENT handoff`](docs/checkpoints/CURRENT-SP11-AUDIO.md).

## Recommended boot set

| Entry | Role | Status |
|---|---|---|
| `sp11-audio-fullio-v19c` | Full protected native input/output daily driver | **Promoted/default** |
| `sp11-audio-dmic-broker-div4-v18` | Native MicArray + generic-render rollback | **Keep** |
| `sp11-audio-golden-v33-topcfg1-physical-vi` | Known-good speaker-only rollback | **Keep** |
| `sp11-audio-v32-feedback-exact-golden` | Fixed-initrd historical rollback | Keep |
| `sp11-audio-golden-v31` | Historical rollback | Keep |
| `sp11-audio-cps-v3` | Conservative rescue | Keep |

Golden v33 remains the output/protection base. Native Audio v18 is the first rollback and Golden v33 is the second protected-output rollback. Diagnostic
mic candidates 0079/0080 and the later unpromoted 0081–0086 endpoint/power
experiments are evidence history, not production runtime.

## Native microphone v18/v19c

The final input root cause had two independent pieces:

1. **VA DMIC divider parity.** Windows uses VA DMIC control `0x3084=0x05`
   (enable + DIV4). Linux previously used `0x01`, producing one broadband-noise
   lane. Patch [`0072`](patches/0072-ASoC-lpass-va-macro-SP11-match-Windows-DMIC-divider.patch)
   derives the selector from the native 19.2 MHz VA MCLK and yields DIV4.
2. **Shared clock ownership.** The physical DMIC clock is owned by the VA macro
   even when capture data flows through TX. Patch
   [`0078`](patches/0078-ASoC-lpass-SP11-share-VA-DMIC-clock-with-TX-capture.patch)
   makes TX DAPM acquire/release that VA-owned clock natively.

The accepted route is `MultiMedia3 → TX_CODEC_DMA_TX_3`, with TX DEC0←DMIC1 and
DEC1←DMIC0 through `MSM_DMIC`. UCM exposes it as **Built-in Audio Internal
microphone array** at 48 kHz, stereo, S16_LE. The physical VA/TX clocks and
`vdd-micb` remain stream/DAPM-driven; they are not permanently pinned on.

See [`deploy/native-audio-v19c/README.md`](deploy/native-audio-v19c/README.md), [`deploy/native-audio-v18/README.md`](deploy/native-audio-v18/README.md) and
[`artifacts/2026-08-26-native-mic-v18-parity/parity-summary.json`](artifacts/2026-08-26-native-mic-v18-parity/parity-summary.json).

## Golden v33 output base

Golden v33 is the accepted speaker/protection baseline. It retains the complete
Golden-v32 AudioReach/WSA8845/SoundWire, mute/volume/CKV and VI/CPS lifecycle and
adds the Windows-proven physical `TOP_CFG1=0x03` producer write after each enabled
VI pair. That makes TAP2 native `V,I,V,I` from the first valid packet while
leaving q6apm/SP_VI unmodified.

Source-identical quiet-room Windows/Linux program A/B is within roughly **0.1
dB** across useful bands at 10% and 50%. A 20-cycle true-cold 50% protection soak
completed with **20 enables, 20 disables, 0 PA faults, 0 `err0=0x20`, and 0
XRUNs**.

See [`deploy/golden-v33/`](deploy/golden-v33/),
[`repro/golden-v33/`](repro/golden-v33/) and the
[`Golden v33 promotion checkpoint`](docs/checkpoints/2026-08-24-GOLDEN-V33-PROMOTED.md).

## UbiG production output

UbiG is the source-owned native SP11 userspace speaker engine. It provides the
accepted 48-kHz stereo Stage A/Stage B behavior, seven Windows profile identities,
Custom/20-band GEQ, endpoint postgain, and realtime control without redistributing
proprietary Windows DSP binaries. `ubig-control` **0.1.3** applies profile changes
immediately from the GTK drop-down, reports whether a live UbiG consumer exists,
and restores the saved per-user profile/Custom curve at GNOME login.

The final two-channel Windows speaker policy intentionally makes **Music and Game
bit-identical** after stereo-virtualizer bypass. The production regression therefore
requires six distinct stereo outputs with Music/Game as the one evidence-backed
alias; it does not invent artificial tuning to make those two labels sound different.

Canonical runtime identities include:

- default/visible processed sink: `effect_input.sp11_ubig`;
- hidden internal hardware backend: `Built-in Audio Speaker playback`;
- hardware UCM source: `Built-in Audio Internal microphone array`;
- historical diagnostic bypass config: tracked in `deploy/pipewire/`, not active
  in production;
- services: `sp11-ubig-volume-sync.service` and
  `sp11-ubig-monitor-link.service`.

See [`ubig/`](ubig/), [`deploy/ubig/`](deploy/ubig/) and
[`ubig/docs/STATUS.md`](ubig/docs/STATUS.md).

## Verification

Verify/rebuild the accepted FullIO v19c topology from any clean clone:

```bash
./repro/native-audio-v19c/build-and-verify.sh
./deploy/native-audio-v19c/verify-native-audio-v19c.sh
```

For the exact kernel/module/initrd reproduction gate:

```bash
JOBS=12 ./repro/native-audio-v19c/build-kernel-initrd-and-verify.sh
```

That heavy gate recreates the promoted common/VA/TX modules and final v19c initrd byte-for-byte from the pristine 7.1.5 -> Golden-v33 -> production 0072+0078 chain.

On the deployed SP11, also verify the loaded boot/module/PipeWire identity:

```bash
./deploy/native-audio-v19c/verify-native-audio-v19c.sh --live
```

Golden v33 can still be independently replayed and verified:

```bash
sudo ./deploy/golden-v33/verify-golden-v33.sh
JOBS=8 ./repro/golden-v33/build-and-verify.sh
```

## Evidence and safety policy

- Bind claims to hashes, addresses, captures, or reproducible live observations.
- Prefer native Windows runtime/binary evidence over generic Qualcomm assumptions
  when they disagree on this board.
- Preserve rejected experiments and corrected conclusions, but keep them out of
  promoted runtime manifests.
- Keep private vendor binaries, ACDB and owner-only tuning payloads out of Git.
- Every risky kernel experiment gets a rollback-safe boot path.
- Direct debugger physical LPASS/WSA MMIO reads are not an accepted method on
  this machine; use driver/runtime evidence or safe Linux-side instrumentation.

## Repository map

| Path | Purpose |
|---|---|
| [`deploy/native-audio-v19c/`](deploy/native-audio-v19c/) | **Current full protected native input/output release identity** |
| [`repro/native-audio-v19c/`](repro/native-audio-v19c/) | Clean FullIO topology + exact kernel/module/initrd reproduction |
| [`deploy/native-audio-v18/`](deploy/native-audio-v18/) | First rollback + microphone parity provenance |
| [`deploy/golden-v33/`](deploy/golden-v33/) | Speaker/protection base + immediate rollback |
| [`repro/golden-v33/`](repro/golden-v33/) | Clean Golden source reproduction |
| [`deploy/ucm2/`](deploy/ucm2/) | UCM Speaker + internal MicArray policy |
| [`deploy/ubig/`](deploy/ubig/) | Production UbiG namespace/helpers |
| [`ubig/`](ubig/) | Native speaker DSP engine/control source |
| [`patches/`](patches/) | Ordered Linux source deltas and diagnostic history |
| [`docs/checkpoints/`](docs/checkpoints/) | Promotion/current-state/evidence records |
| [`docs/findings/`](docs/findings/) | Evidence-backed technical findings |
| [`artifacts/2026-08-26-native-mic-v18-parity/`](artifacts/2026-08-26-native-mic-v18-parity/) | Final microphone parity + deployed smoke records |
| [`deploy/golden-v32/`](deploy/golden-v32/) | Historical fixed-initrd rollback baseline |

## Current scope

Built-in speaker output/protection **and** the internal microphone input are
closed and promoted. Future built-in-audio work should start from FullIO v19c +
UbiG. System suspend/resume remains explicitly external to this audio release and
has its own dedicated RE; Bluetooth/headset/USB/DP integration and upstreaming
should be tracked separately rather than reopening accepted speaker or MicArray
behavior without reproducible counter-evidence.
