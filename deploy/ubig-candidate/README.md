# SP11 UbiG disposable candidate deployment

This directory is the first **non-Golden** deployment wrapper for the source-owned UbiG LADSPA candidate. It deliberately preserves the existing SP11 visible-sink / hidden-engine PipeWire graph, monitor-link names, final AudioReach volume transaction, MSIIR/CKV service and rollback Windows bridge. Only the hidden LADSPA implementation changes.

Nothing here is installed by the normal UbiG build. `prepare.sh` is safe to run while the Golden graph is active: it rebuilds the tracked candidate, executes `candidate-control-check`, verifies the pinned corrected-v3 private pack, audits dynamic dependencies/vendor-loader strings, and stages a per-user plugin/config plus matching endpoint-volume/transaction/MSIIR helper copies under `~/.local`. It does **not** restart PipeWire or replace the active graph.

```sh
deploy/ubig-candidate/prepare.sh
deploy/ubig-candidate/switch.sh status
```

The candidate uses the public `ubig-control-v2` page at `$XDG_RUNTIME_DIR/ubig-control-v2`. Candidate drop-ins set `UBIG_CONTROL_FORMAT=ubig-v2` and override `ExecStart` to the reviewed helper copies staged alongside the candidate, while the installed Golden bridge and its `~/.local/bin` helpers remain untouched for rollback. The filter-chain candidate drop-in also restores `MemoryDenyWriteExecute=yes`; the old `MemoryDenyWriteExecute=no` exception remains on disk solely for rollback and becomes effective again when the candidate drop-in is removed.

Activation is explicit and always snapshots the current active PipeWire fragment first:

```sh
deploy/ubig-candidate/switch.sh activate
```

Rollback restores that exact saved fragment, removes only the three candidate-specific drop-ins/control page, and restarts the established services:

```sh
deploy/ubig-candidate/switch.sh rollback
```

The candidate pack is private owner-supplied data and is never copied into Git. The current disposable checkpoint pins SHA-256 `c993c123f2cb3b92776754da2383217e00b5f290664571f12cfb62b9afb3a175`; override both the pack path and expected hash deliberately if a later reviewed pack supersedes it.

Activation is not a promotion to Golden. M6 still requires waveform/profile/Custom transitions, volume/mute/seek, idle/wake, repeated playback, xrun/NaN monitoring, >8-hour stability, PA/protection telemetry and the physical Windows acoustic matrix before the Windows userspace bridge may be removed.
