# Windows DEFAULT-owner overlap policy — 2026-08-12

## Result

A controlled SP11 Windows one-shot boot closes one direction of the outstanding render-family overlap question.

When a DEFAULT-class render stream owns the internal-speaker endpoint graph and an explicit Alerts/NOTIFICATION stream starts on top of it, Windows **does not perform a fresh qcadcm render-family selection for the alert**. The miniport still classifies the new stream as NOTIFICATION (`flag 0x0a`), but the already-live lower DEFAULT family remains the selected Qualcomm graph for that audio-engine lifetime.

The reciprocal NOTIFICATION-first → DEFAULT case is not yet captured and remains a separate runtime gate. This finding therefore does **not** promote a universal “first stream always wins” rule.

## Controlled overlap

The endpoint was clamped and verified at **10%** before playback.

Target stimulus timeline:

```text
MEDIA_START  2026-08-12T08:15:56.0720107+01:00
ALERT_START  2026-08-12T08:16:10.6548568+01:00
ALERT_STOP   2026-08-12T08:16:14.6859996+01:00
MEDIA_STOP   2026-08-12T08:16:19.3003982+01:00
```

The working ARM64 hardware execution trap at the live qcaudminiport processing-mode translator recorded:

```text
[MODE_LIVE 1] flag=1
...
[MODE_LIVE 2] flag=a
```

Thus the base stream requested DEFAULT and the overlapping Alerts stream independently reached the NOTIFICATION translator path.

Before the NOTIFICATION mode hit, qcadcm’s reviewed ACDB selector produced ten calls. The valid render-vector result began:

```text
01000001 00000002
01000002 00000002
01000003 00000001
```

which is the reviewed DEFAULT render vector prefix.

From the NOTIFICATION mode hit through Alert stop and final Media stop, **no additional qcadcm selector call occurred**.

This is the important lifecycle observation: qcaudminiport sees the second stream’s NOTIFICATION mode, but qcadcm does not select a second render family while the DEFAULT endpoint graph is already established.

## Falsification: Alerts from a cold audio-engine lifetime

To exclude “the qcadcm selector trap simply missed Alerts,” Windows AudioSrv/AudioEndpointBuilder were cold-restarted and an explicit Alerts stream was then started without an already-active DEFAULT stream.

That run produced a fresh qcadcm selector sequence. A bounded kernel-VM read of its valid six-key result buffer recovered:

```text
01000001 00000002
01000002 00000007
01000003 00000001
01000004 00000002
01000005 00000007
01000006 00000001
```

This is exactly the independently reviewed GKV `7` / NOTIFICATION speaker vector.

Therefore the difference between the two runs is real:

```text
idle -> Alerts
    => qcadcm selects NOTIFICATION / GKV 7

DEFAULT already active -> Alerts overlaps
    => miniport sees NOTIFICATION flag 0x0a
    => no fresh qcadcm family selection
    => existing DEFAULT lower graph remains owner
```

## Architectural consequence

The evidence now favors the Windows speaker processing mode being an **endpoint/audio-engine graph-lifetime choice**, not a separate AudioReach family instantiated independently for every simultaneously mixed client stream.

For Linux this means we must not naïvely route simultaneous DEFAULT and NOTIFICATION clients into two independently active protected AudioReach families merely because both families exist in ACDB. At minimum, a DEFAULT-owned speaker lifetime must continue to mix later NOTIFICATION audio through the existing family until that ownership lifetime ends or Windows evidence proves a deliberate switch boundary.

The reciprocal NOTIFICATION-first → DEFAULT case still matters before finalizing the owner/switchover policy. It remains explicitly open.

## Debugger/address provenance

On this boot the Qualcomm PnP images did not enumerate through normal `lm`. Their live bases were recovered from `!drvobj` DriverEntry plus the hash-matched PE `AddressOfEntryPoint` RVA and validated by live disassembly:

```text
QCAUD/qcaudminiport base   fffff800`4c6d0000
mode translator RVA        0x94080
mode translator VA         fffff800`4c764080

qcadcm base                 fffff800`505b0000
selector RVA                0x307a8
selector VA                 fffff800`505e07a8
```

The reliable ARM64 capture mechanism is `ba e1`, matching the earlier successful mode-selection session. Software `bp`/deferred `bu` was not an equivalent mechanism for these PnP images in this session.

Repeated qcadcm selector hardware trapping also exposed a tooling limitation: unrelated ACDB calls can leave an unmatched ARM64 single-step stop. Future selector work should therefore be cold, bounded, and should log pointer values without automatic dereference; any selected buffer should be inspected manually with a bounded kernel-VM read.

A qualifying qcadcm `+0x5aa34` OOB hook did not fire in these lifecycle runs. That absence is not used as the primary lifecycle conclusion.

## Evidence

Machine-readable reviewed record:

`artifacts/reviewed/2026-08-12-windows-default-notification-overlap-policy.json`

SP7 raw KD logs remain outside the reviewed corpus and are bound by SHA-256 in that record.

## Safety / teardown

- Windows speaker endpoint maximum used: **10%**.
- No physical MMIO access.
- No debugger MMIO write.
- No DSP, SoundWire or arbitrary driver-state write.
- Hardware execution breakpoints only, with immediate continue on successful logging hits.
- An intermediate PTY failure left an orphaned host `kd.exe`; because the target was running and the PTY was no longer controllable, that host process was terminated before any new debugger was started. This is recorded as a host-tooling incident, not a clean `qd`.
- The final Windows transition cleared breakpoints and closed its KD log before reboot.
- The one-shot Windows entry was consumed; SP11 returned to persistent Linux default `sp11-audio-cps-v3`.
- Final SP7 debugger-owner check: zero `kd.exe`, zero WinDbg.
