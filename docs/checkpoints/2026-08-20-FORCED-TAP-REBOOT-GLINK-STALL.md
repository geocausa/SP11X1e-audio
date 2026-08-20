# 2026-08-20 — forced feedback-tap boots can stall during reboot/ADSP teardown

## Observation
The apparent multi-minute "slow boots" after the forced TAP2/TAP3 diagnostic runs are primarily long gaps after the previous kernel has already entered shutdown, not ordinary Linux userspace startup latency.

### Forced TAP2 clean-VI boot
- forced TAP2 topology via `/run/sp11-fw`
- POST-PA PROTCLK + active Offset2
- successful VI capture, 0 PA faults
- shutdown journal contained 4 x `32300000.remoteproc:glink-edge: intent request timed out`
- boot-to-next-boot wall-clock gap was ~3.5 minutes, but the next candidate eventually appeared without manual reset.

### Forced TAP3 CPS boot
- forced TAP3 topology via `/run/sp11-fw`
- POST-PA PROTCLK + active Offset2 + CPS wake (`0x105c`, DP13 `0x1d54`)
- successful CPS capture, 0 PA faults
- shutdown again contained 4 x ADSP GLINK intent-request timeout
- PiSlave could not exit cleanly (`final-sigterm` timeout / final SIGKILL)
- systemd reached `reboot.target`, synced filesystems and sent SIGTERM; journal then stopped
- machine did not complete the hardware reset and required a user physical force-reboot
- no pstore panic record was produced.

## Control comparison
Recent non-forced candidate boots with the same POST-PA protection-clock code but no forced feedback topology showed no GLINK intent timeout and rebooted in the normal ~30-40 s class.

The Golden control likewise had no GLINK timeout in the reviewed normal reboot.

Therefore do not interpret a few minutes of PiMaster unreachability after these diagnostic boots as simple boot latency. The forced feedback graph / ADSP diagnostic teardown can stall the previous kernel's reboot path.

## Consequence
This is a diagnostic-lifecycle issue separate from the VI/CPS dataplane result. Do not block the dataplane conclusion on it, but do not use forced TAP2/TAP3 boots as a stability/promotion environment.

Promotion/stability testing must use:
- canonical topology,
- no DIAG router,
- no forced logger override,
- explicit normal playback/teardown cycles,
- zero PA faults,
- normal reboot back to Golden.

If another forced tap boot is required, preserve Golden as one-shot fallback and expect that a physical power-cycle may be required after evidence is captured.
