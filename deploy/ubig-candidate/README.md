# SP11 UbiG rollback-safe staging deployment

This directory is retained as the rollback-safe build/staging wrapper for the now-promoted source-owned UbiG engine. The canonical production namespace and helpers live under `deploy/ubig/`; this wrapper rebuilds/stages the same native engine without mutating the currently active graph until `switch.sh activate` is requested.

Nothing here is installed by the normal UbiG build. `prepare.sh` is safe to run while the Golden graph is active: it rebuilds the tracked candidate, executes `candidate-control-check`, verifies the pinned corrected-v4 private pack, audits dynamic dependencies/vendor-loader strings, and stages a per-user plugin/config plus matching endpoint-volume/transaction/MSIIR helper copies under `~/.local`. It does **not** restart PipeWire or replace the active graph.

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

The candidate pack is private owner-supplied data and is never copied into Git. The current disposable checkpoint pins the v4 SHA-256 `30b9b8ce8dace4a9f5dee2c2defa7da2d9b8431cf68fb323f8d2c3e4e3c942df`; override both the pack path and expected hash deliberately if a later reviewed pack supersedes it. The tracked `ubig/tools/build_stageb_v4_pack.py` reproducibly rebuilds that private v4 pack from the owner's v3 pack plus local `DolbyAPOVR.dll`; no owner payload is committed.

The objective M6 gates and operator promotion verdict are closed. This wrapper remains useful for controlled rebuild/rollback testing, but `deploy/ubig/` is the production identity.
