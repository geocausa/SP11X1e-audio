# WSA8845 DRE_CTL_1 write-history reconstruction

Date: 2026-08-16
Status: **new lifecycle closure; explains why v6 was the wrong exact-value experiment**

## Question

`csren0-v5-idlegated` is stable with `CSR_GAIN_EN=0` while the stored CSR gain code remains 7 (`DRE_CTL_1 ~= 0x0e`). `csrgain0-v6-idlegated` additionally forced that stored code to zero immediately before PA enable and became wildly non-repeatable acoustically, even though its final active register value matched the Windows `0x00` snapshot.

The key missing distinction was **final value versus write history**.

## Read-only Linux register-write trace

A temporary kprobe was placed on `regmap_update_bits_base()` and filtered strictly to register `0x34b1` (`WSA884X_DRE_CTL_1`). It records function arguments only; it does not read the codec and does not modify behavior.

### Profile/audio-stack reconstruction

During a controlled PipeWire/WirePlumber stop/restart on safe CPS-v3, the first DRE_CTL_1 writes were:

```text
539.955504  amp A  mask=0x3e val=0x0e
539.955540  amp B  mask=0x3e val=0x0e
539.956600          SoundWire runtime resume
...
539.976378          first WSA8845 speaker POST_PMU
539.976382  amp A  mask=0x01 val=0x00
539.976390          second WSA8845 speaker POST_PMU
539.976393  amp B  mask=0x01 val=0x00
```

The gain-field programming therefore occurs as **route/profile control setup**, before SoundWire resume and roughly 21 ms before speaker POST_PMU. The driver then separately clears the enable bit once COMP is active.

The same profile reconstruction performed a second route cycle and repeated the same order:

```text
540.037966 / 540.038038  mask=0x3e val=0x0e
...
540.052655 / 540.052682  speaker POST_PMU
540.052665 / 540.052690  mask=0x01 val=0x00
```

A later profile-state operation reasserted `mask=0x3e val=0x0e` again at `540.205212/540.205297`.

Evidence:

```text
artifacts/reviewed/2026-08-16-cpsv3-drectl1-runtime-write-history.trace
```

### Ordinary demand playback after profile construction

A separate trace started from an already-configured, PCM-closed desktop and played two seconds of the fixed MP3. **No `mask=0x3e` gain-field write occurred anywhere in the wake/play/suspend cycle.**

Observed DRE_CTL_1 sequence:

```text
635.840938 / 635.840972  speaker POST_PMU: mask=0x01 val=0x00
635.947005                 first playback unmute entry
635.947446 / 635.949346  CPS-v3 unmute: mask=0x01 val=0x01
642.788112                 first playback mute entry
642.788129 / 642.788161  mute: mask=0x01 val=0x00
```

The gain code established by UCM/profile setup is simply carried through normal playback. It is **not** normally rewritten at PA unmute.

Evidence:

```text
artifacts/reviewed/2026-08-16-cpsv3-drectl1-demand-write-history.trace
```

## Qualcomm downstream corroboration

Qualcomm's public CodeLinaro audio-kernel tree for the related WSA883x family also treats the DRE_CTL_1 gain field and enable bit as distinct lifecycle state rather than a single static register value.

Primary source provenance:

```text
repo:   https://git.codelinaro.org/clo/la/platform/vendor/opensource/audio-kernel.git
branch: audio-kernel.lnx.6.0.r66-rel
commit: 67eab4849dac034732f8bf9b9251861dad5e2fd1
file:   asoc/codecs/wsa883x/wsa883x.c
```

Relevant behavior in that driver:

- initialization table programs `DRE_CTL_1 mask 0x3e` separately (`value 0x20` on WSA883x);
- datapath enable is followed by a hardware-sequence 250--300 us delay before `DRE_CTL_1 bit0` is set;
- after PA FSM enable, Qualcomm waits 3.0--3.1 ms, clears `DRE_CTL_1 bit0`, then waits another 5.0--5.05 ms.

The exact numerical WSA883x gain code is not assumed to apply to WSA8845. What matters is the lifecycle evidence: Qualcomm explicitly sequences the stored gain field separately from the CSR enable bit and documents timing around enable transitions.

## Windows contrast

The retained SP11 Windows qcaucd/KD observations remain:

```text
fresh init: DRE_CTL_1 = 0x00 on both WSA8845s
ordinary PA start/stop: zero observed DRE_CTL_1 runtime writes
```

Windows therefore reaches its exact `0x00` state by initialization/history, not by forcing the gain field to zero at each PA unmute.

## v6 reinterpretation

v6's final active register value was correct, but its lifecycle was not. It inserted:

```text
mask=0x3e val=0x00
```

immediately before every already-proven v5 `CSR_GAIN_EN=0` PA enable. That transition does not exist in the normal Linux demand cycle and has no Windows evidence. The resulting 5--12 dB run-to-run acoustic instability is therefore consistent with a DRE state/latch disturbance caused by **when the field is rewritten**, not proof that the Windows gain code zero is intrinsically wrong.

## Decision / next experiment

- Keep v6 rejected; never re-arm it as an acoustic oracle.
- Do not sweep arbitrary CSR gain values.
- Test the next isolation at the **profile/route programming boundary**, where Linux already programs the field naturally:
  - use the proven v5 CSR-enable-off kernel semantics;
  - change only UCM/profile `SpkrLeft/Right PA Volume` from 24 (raw code 7) to 31 (raw code 0);
  - retain the corrected passive Dolby idle lifecycle;
  - make no gain-field write in `mute_stream()`/PA unmute.
- Start at 1%, prove PCM demand/close and DRE write order, then idle dwell before any 12% oracle.

This tests write timing/history directly and is materially different from both the rejected old always-running cold candidate and rejected per-unmute v6.
