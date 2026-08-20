# Windows WSA protection-clock / PA ordering — 2026-08-20

## Result

A cold native-Windows KDNET trace proves that the WSA protection TX clocks are
not enabled during early transport preparation.  On the Surface Pro 11 speaker
path the ordering is:

1. qcaucd `WSA_START_OWNER` (`qcaucd8380+0x36510`)
2. both WSA8845 instances write `PA_FSM_EN.GLOBAL_PA_EN = 1` (`reg 0x3430`)
3. qcaucd protection resources 7/8 enable the WSA TX protection clocks via
   `FUN_140039388`, observed as resource kinds 5 and 6

Cold run A:

```
EV WSA_START_OWNER
EV PROTCLK_START kind=5
EV PROTCLK_START kind=6
```

Cold run B:

```
EV PA3430 data=1 obj=fffff801b9957e70
EV PA3430 data=1 obj=fffff801b9957e38
EV PROTCLK_START kind=5
EV PROTCLK_START kind=6
EV PA3430 data=0 obj=fffff801b9957e70
```

The final `PA3430=0` is normal teardown after the render and is not part of the
startup sequence.

## How the cold trace was forced

The normal shared-mode audio engine kept the low-level QCAUD hardware session
warm, so ordinary renders did not re-run the owner path.  A deterministic cold
open was produced by stopping `Audiosrv`, restarting only the Qualcomm Aqstic
AUCD PnP device, re-arming KD breakpoints against the newly loaded qcaucd image,
starting `Audiosrv`, and then invoking the validated 48-kHz stereo shared-mode
WASAPI renderer.

AUCD device:

```
Qualcomm(R) Aqstic(TM) Audio Adapter Device
AUCD\VEN_QCOM&DEV_0C29&SUBSYS_MSHW0486&REV_0D\5&33A202C0&0&0
qcaudminiport8380.sys / QCAUD
```

The live qcaucd image matched the preserved reverse-engineered RVAs exactly.
ARM64 KD exposed only two hardware execution breakpoint slots, therefore the
ordering proof was split across two equivalent cold starts.

No raw WSA/LPASS MMIO was read from Windows.

## Linux fault bisect

The earlier machine-`prepare()` protection-clock candidate is now rejected as a
lifecycle placement even though its register values are proven correct.

Retained Linux boot-journal bisect:

| Candidate | PROTCLK | Offset2 | PA faults |
|---|---:|---:|---:|
| Golden v31 | off | off | 0 |
| Offset2 only | off | on | 0 |
| PROTCLK only | on | off | 80 |
| PROTCLK + Offset2 | on | on | 41 |

A fresh Golden-v31 four-second digital-silence render also produced zero PA
fault/recovery messages.

The bad candidate enabled PROTCLK from `x1e80100_snd_prepare()` after
`qcom_snd_sdw_prepare()`.  Qualcomm's helper already performs both
`sdw_prepare_stream()` and `sdw_enable_stream()`, so this is not a SoundWire
port-enable ordering bug.  It is specifically too early relative to WSA8845 PA
activation.

## ASoC ordering consequence

The WSA884x DAI sets `.mute_unmute_on_trigger = true`.  Its
`wsa884x_mute_stream(..., mute=false)` performs the SP11 PA start sequence and
writes `WSA884X_PA_FSM_EN.GLOBAL_PA_EN = 1` during the DAI-trigger phase.

A machine-link `.trigger()` is therefore also too early: in the default ASoC
trigger order the link callback runs before the DAI trigger, and even LDC order
still places the link callback before the DAI trigger.  Do not move the old
machine helper merely from `.prepare()` to `.trigger()`.

## Next candidate constraint

Keep the proven Windows register sequence unchanged:

```
RESET=1 -> rate=8 kHz -> CLK_EN=1 -> RESET=0
```

for all four WSA protection TX paths.  Move only its lifecycle position so the
enable occurs after both SP11 WSA8845 instances have completed their
`GLOBAL_PA_EN=1` transition.  Prefer WSA884x-to-WSA-macro ownership rather than
a machine-driver timing approximation.

The next candidate must first pass a digital-silence PA-fault test.  Only then
run DIAG/tap payload tests.  Promotion still requires real nonzero feedback and
zero PA faults.
