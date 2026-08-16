# WSA8845 CSR-EN0 v4: delayed crackle rejects unconditional CSR-off lifecycle

Date: 2026-08-16  
Status: **REJECTED for promotion; delayed lifecycle/acoustic safety failure**

## Candidate identity

The one-shot Linux boot from 18:33:08 to 19:07:06 BST was:

```text
sp11-audio-rpv4-macro84-winproducer-nohd2-csren0-v4
```

with kernel marker:

```text
sp11_wsa_csren0_v4=1
```

The candidate was intentionally one-variable relative to the Windows-aligned producer/no-HD2 v3 state. Its complete source delta is:

```diff
 snd_soc_component_write_field(component, WSA884X_DRE_CTL_1,
                                WSA884X_DRE_CTL_1_CSR_GAIN_EN_MASK,
-                               0x1);
+                               wsa884x->supply_config == WSA884X_SUPPLY_2S ?
+                               0x0 : 0x1);
```

It does **not** force `DRE_CTL_1=0x00`, does not change PA Volume/UCM, and does not change the WSA-macro producer. The stored CSR gain field therefore remains untouched; only CSR fallback enable is kept clear on SP11 2S unmute before the existing `GLOBAL_PA_EN=1` write.

The exact retained patch is outside the Git tree at:

```text
/home/geoca/Documents/SP11-PROJECT/02-kernel/candidates/rpv4-macro84-winproducer-nohd2-csren0-v4-20260816/csren0-v4-one-variable.patch
```

## Short-program safety evidence

SP7 captured a deterministic low-level 1% endpoint test early in the boot:

```text
acoustic-safety-linux-rpv4-csren0-v4-1pct-20260816/external-mic-20260816-183453.wav
```

A coarse absolute-ridge comparison put its 1--5 kHz level about 24.8 dB below the retained v3 12% captures, reasonably near the expected 21.6 dB endpoint ratio given the weak low-level alignment. The test did not exhibit the immediate catastrophic behavior of the old forced-`DRE_CTL_1=0` candidate. This only establishes that CSR-EN0 can survive short program playback; it is not an acoustic parity verdict.

A second 39.99 s capture exists at:

```text
acoustic-safety-linux-rpv4-csren0-v4-1pct-full-a-20260816/external-mic-20260816-183824.wav
```

and contains substantially more high-RMS content. It is not sufficient by itself to attribute that content to the candidate because room/environmental activity was not independently gated.

## Delayed failure

At approximately **19:02 BST**, while the same csren0-v4 boot was still active, the user reported that the SP11 had been **crackling for a while**.

At investigation time there were no active application playback streams in the PipeWire graph and no kernel ADSP-crash/XRUN storm. Muting from the first remote shell initially missed the desktop session; the subsequent desktop-session mute plus a complete userspace audio-stack shutdown was used to quiesce the machine.

The first stop attempt at about 19:03:10 stopped PipeWire/WirePlumber but left their sockets active. systemd immediately recreated the stack, and the producer graph cycled repeatedly. The retained `SP11VBAT` markers show both producer channels disabling and re-enabling during those restarts. The final stop at about 19:03:23 disabled the PipeWire sockets plus the SP11 Dolby/volume helpers and left:

- PipeWire/WirePlumber inactive;
- no `/dev/snd` holder;
- hardware PCM `closed`.

The machine was later rebooted through Windows and returned to the saved CPS-v3 Linux fallback. `csren0-v4` has **not** been re-armed.

## Interpretation

The delayed crackle changes the safety verdict materially. A one-variable `CSR_GAIN_EN=0` unmute can carry controlled program audio for a short interval, but is not safe across the current Linux idle/reopen/teardown lifecycle.

This is consistent with a producer/consumer ordering problem rather than a simple gain-value problem: Windows keeps CSR fallback disabled while maintaining a coherent WSA producer/COMP and PA lifecycle; Linux may expose an interval in which the WSA8845 remains effectively audible while the COMP/VBAT producer is being removed or recreated. The repeated producer graph cycling during userspace audio restart is a concrete stressor, but it is not yet proof of the precise offending transition.

## Decision

- **Do not re-arm `csren0-v4` for an acoustic oracle.**
- Keep the old forced-`DRE_CTL_1=0` candidate rejected as before.
- Preserve the Windows-aligned producer/no-HD2 structural corrections independently.
- Next work is **trace-only lifecycle reconstruction** of PA/CSR versus COMP/VBAT/SoundWire teardown and reopen ordering.
- Before another CSR-disabled behavior candidate, prove on every cycle that PA is disabled before COMP/VBAT producer removal and that the producer is fully ready before PA enable, symmetrically on both amps.
