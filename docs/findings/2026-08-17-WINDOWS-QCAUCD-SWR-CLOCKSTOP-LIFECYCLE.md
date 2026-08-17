# Windows qcaucd SoundWire clock-stop lifecycle — 2026-08-17

## Why this was measured

v13 proved that replaying the coherent Windows WSA8845 analog tail materially improves the first CSR-off PA cycle, but the same boot becomes catastrophically noisy on cycle 2. v14 proved that preserving a single Windows-latched amp register while Linux still tears down the lower transport is not coherent. The next unresolved boundary was therefore SoundWire/producer lifetime across ordinary PA-off idle.

## Surface policy is active and consumed by qcaucd

The live SP11 Windows AUCD device registry and the Surface extension INF both establish:

- `SwrSleep = 1`;
- `SwrClockStopTimerMS = 500`.

Static recovery from the exact SP11 `qcaucd8380.sys` shows `SwrSleep` consumed by `FUN_1400497b0`; `SwrClockStopTimerMS` is consumed by `FUN_14004a418` and copied to the runtime timer global used by the SoundWire idle state machine.

## qcaucd WPP channel recovered

The ordinary manifest Qualcomm provider does not emit these lifecycle events. qcaucd instead registers a classic WPP control GUID:

`{D3D7C968-4842-4C61-994E-2AAD4DBA2C18}`

The SoundWire lifecycle functions all trace under message-component GUID:

`{90ACBF0B-3924-3169-10C3-949A4505D136}`

The static WPP ID map is:

- IDs `26..31`: `FUN_14003b230`, actual SoundWire clock-stop handshake;
- IDs `32..37`: `FUN_14003b450`, delayed timer callback;
- IDs `38..44`: `FUN_14003b678`, zero-user / sleep decision;
- IDs `55..62`: `FUN_14003bf40`, dataport programmer;
- IDs `63..67`: higher dataport apply path.

## Live two-cycle Windows proof

Controlled two-cycle ETL on SP11 Windows:

`C:\Users\Geoca\Documents\SP11-Windows-SWR-Trace-20260817\qcaucd-wpp-two-cycle.etl`

SHA-256:

`FE72BDA84CF383C811F6521AA17C68D80602F20D9EC7F54BD9408EA50F57F0DC`

Each cycle used the known-safe WASAPI marker/tail renderer at endpoint 1%; the endpoint was restored to 1% muted after capture.

After each stream close, the SoundWire zero-user sequence occurs first. A second burst then occurs almost exactly one Surface timer interval later:

- cycle 1: `+0.485060 .. +0.496683 s`;
- cycle 2: `+0.485568 .. +0.499054 s`.

That delayed burst contains the timer-callback IDs plus the actual clock-stop-handshake IDs. This is live proof that the configured **500-ms SoundWire clock-stop policy is active** on ordinary Windows speaker cycles.

Windows does **not** simply leave dataports enabled. IDs `55..67` recur at both cycle starts and both cycle stops, proving qcaucd reapplies dataport hardware state around every cycle.

## Linux comparison

Read-only CPS-v3 two-cycle trace:

`artifacts/reviewed/2026-08-17-cpsv3-sdw-clockstop-two-cycle.trace`

SHA-256:

`1b694d8fadfa821a062577af6ba387fa3d2a2d1eb66f861b3bb764b93ae0a097`

Linux ordinary idle does:

`PA mute -> speaker PRE_PMD -> producer down -> sdw_disable_stream -> sdw_deprepare_stream -> sdw_stream_remove_slave -> ... -> runtime suspend -> bus clock-stop`

The important distinction is object lifetime. Exact source ownership shows:

- `qcom_snd_sdw_startup()` allocates a new `sdw_stream_runtime` on every ALSA startup;
- WSA8845 `hw_params()` adds each physical amp with `sdw_stream_add_slave()`;
- WSA8845 `hw_free()` calls `sdw_stream_remove_slave()`;
- the SoundWire core documents that `sdw_stream_remove_slave()` frees the slave port runtime and slave runtime;
- `qcom_snd_sdw_shutdown()` then calls `sdw_release_stream()`, freeing the stream runtime itself.

The next wake therefore reconstructs the SoundWire topology object before reprogramming ports.

## Decision / next isolation

Do **not** preserve active dataports or keep the PA on. Windows itself reprograms dataports and performs a real delayed clock-stop.

The next SP11-only candidate should preserve the **SoundWire topology runtime objects** across ordinary desktop idle while retaining all safe shutdown behavior:

- PA still mutes/off;
- producer/interpolators still shut down;
- SoundWire streams still disable and deprepare;
- bus still enters clock stop;
- retained slave/port/stream runtime objects are reconfigured/reused on the next `hw_params()` rather than freed/reallocated.

This is a narrower and more evidence-backed hypothesis than further DRE/current-limit/register changes. It directly tests whether Linux's topology destruction is what loses the beneficial v13 cold-initialized state.

Machine-readable summary: `artifacts/reviewed/2026-08-17-windows-qcaucd-swr-clockstop-wpp-summary.json`.
