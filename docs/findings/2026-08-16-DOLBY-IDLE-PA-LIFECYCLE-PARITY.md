# SP11 Dolby idle PA lifecycle: passive hidden input restores Windows stop behavior

Date: 2026-08-16
Status: **validated on safe CPS-v3; production userspace fix**

## Result

The August 14 split-control Dolby topology introduced a lifecycle regression even though it fixed the pre-Dolby volume boundary. With no application streams, the persistent visible-sink-monitor -> hidden-Dolby-engine links kept the complete PipeWire graph demanded continuously. The physical ALSA speaker PCM therefore remained `RUNNING` and the WSA8845 PA remained unmuted indefinitely at desktop idle.

Windows does not keep the SP11 internal-speaker PA in that state. The retained native qcaucd lifecycle trace for an idle -> six-second MP3 -> stop cycle shows an explicit stop sequence after playback:

```text
PA_FSM_EN GLOBAL_PA_EN = 0
CLSH_CTL_0              = 0
```

That distinction became safety-critical once the Windows-proven `CSR_GAIN_EN=0` state was isolated: `csren0-v4` carried controlled short program audio, but later produced prolonged crackling while the Linux graph was idle/open. The kernel candidate is therefore rejected independently; this finding fixes the host lifecycle that exposed the non-Windows idle state.

## Pre-fix Linux proof

A read-only trace on safe `7.1.5-sp11-cps-v3+` showed the normal boot graph reaching:

```text
13.127019 / 13.127027  WSA macro POST_PMU
13.127033 / 13.127043  WSA8845 speaker POST_PMU
13.156886               first playback DAI unmute
```

No later playback `mute_stream(mute=1)` occurred until a controlled userspace teardown at `329.799459`, roughly 316.6 seconds later. Immediately before that teardown, `wpctl` contained **no application streams**.

Live production state reproduced the problem directly:

```text
alsa_output.platform-sound.HiFi__Speaker__sink  running
effect_input.sp11_windows_dolby                 running
effect_input.sp11_windows_dolby_engine          running
effect_output.sp11_windows_dolby                running
```

and:

```text
/proc/asound/card0/pcm0p/sub0/status
state: RUNNING
owner_pid: <main PipeWire PID>
```

The physical-engine links were already passive because the engine playback stream has `node.passive=true`. The missing passive boundary was the **hidden engine capture/input stream** fed by the visible sink's unity monitor ports.

## Safe teardown ordering was already correct

A controlled stop/reopen on CPS-v3 proved that ordinary Linux teardown itself is correctly ordered:

```text
329.799459  wsa884x_mute_stream(mute=1)
329.799685  first WSA8845 speaker PRE_PMD
329.799703  first WSA macro POST_PMD
329.799765  SoundWire disable begins
```

So the root problem was not “producer removed before PA mute” on a normal stop. The problem was that the split-control graph did not enter a normal stop at idle at all.

## Fix

Add one property to the hidden Dolby engine capture stream:

```text
capture.props = {
    node.name    = "effect_input.sp11_windows_dolby_engine"
    node.passive = true
    ...
}
```

PipeWire propagates node passivity to the stream ports. The existing persistent monitor links can remain ordinary linger links; because the engine input ports are passive, those links no longer make the graph runnable by themselves. An application connected to the visible sink remains the non-passive demand source and wakes the chain normally.

## Idle validation

After restarting only `filter-chain.service` with the new property and no application stream:

```text
physical ALSA speaker sink       suspended
visible Dolby sink               suspended
hidden Dolby engine input        suspended
hidden Dolby engine output       suspended
ALSA playback PCM                closed
```

The transition trace begins with playback mute, then speaker/producer teardown and SoundWire disable. No behavior-changing kernel candidate was used.

## Demand wake / suspend validation

The fixed local sample:

```text
/home/geoca/Documents/The White Stripes - Seven Nation Army (Official Music Video).mp3
```

was played for six seconds through `effect_input.sp11_windows_dolby` at a 1% visible endpoint setting on safe CPS-v3.

Wake sequence:

```text
713.404499  SoundWire runtime resume
713.421230  first stream prepare
713.426058  first stream enable
713.429491  WSA macro POST_PMU
713.429515  WSA8845 speaker POST_PMU
713.489047  first WSA8845 playback unmute / PA enable path
```

After playback stopped, the normal PipeWire suspend delay expired and the inverse sequence occurred:

```text
724.392318  first WSA8845 playback mute / PA disable path
724.392835  WSA8845 speaker PRE_PMD
724.392870  WSA macro POST_PMD
724.392958  first SoundWire disable
724.408207  final SoundWire deprepare
```

Final state was again:

```text
physical ALSA speaker sink  suspended
ALSA PCM                    closed
```

Thus the speaker PA is no longer held active at desktop idle, and real playback still wakes the complete protected Dolby/COMP/SoundWire path on demand.

## Consequences

1. This closes a real Windows/Linux **host lifecycle** mismatch introduced by the split-control volume correction.
2. It provides a concrete explanation for why the delayed `csren0-v4` crackle was possible even though short program playback was controlled: Linux had been holding the Windows active-playback amp state across idle silence.
3. It does **not** by itself make `CSR_GAIN_EN=0` safe. That kernel candidate remains rejected until re-evaluated under the corrected host lifecycle with fresh bounded safety gates.
4. Dolby adaptive state preservation remains handled by the existing no-reset-on-PipeWire-activate correction; suspending the stream does not intentionally reconstruct the plugin instance.

## Evidence

Reviewed traces:

- `artifacts/reviewed/2026-08-16-cpsv3-wsa-lifecycle-safe.trace`
- `artifacts/reviewed/2026-08-16-cpsv3-controlled-stop-reopen.trace`
- `artifacts/reviewed/2026-08-16-engine-capture-passive-idle.trace`
- `artifacts/reviewed/2026-08-16-engine-passive-mp3-wake-suspend.trace`
