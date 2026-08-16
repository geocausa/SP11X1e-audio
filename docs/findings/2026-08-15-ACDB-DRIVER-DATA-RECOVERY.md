# REV_0D ACDB GSL driver-data format recovered

Date: 2026-08-15
Status: recovered / evidence-bound

## Scope

The exact installed SP11 Windows qcadcm driver was used to recover the private GSL driver-data lookup used by `AcdbCmdGetDriverData` / `AcdbGetWsaCfg`.

Installed qcadcm SHA-256:

`37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`

REV_0D ACDB:

`acdb_cal_0D.acdb`

SHA-256:

`a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde`

## Exact chunk selectors

The six FourCC selectors used by the driver-data path are read from qcadcm globals and are:

1. `GCKT`
2. `GCLU`
3. `GCDT`
4. `GCDE`
5. `GCDO`
6. `POOL`

The recovered format is:

- `GCKT`: repeated `[key_count, key_id...]`
- `GCLU`: `[record_count, (module_id, gckt_schema_offset, gcdt_offset)...]`
- `GCDT` block: `[key_count, row_count, (key_values..., gcde_offset, gcdo_offset)...]`
- `GCDE`: `[param_count, param_ids...]`
- `GCDO`: `[param_count, pool_offsets...]`
- `POOL` record: `[payload_size, payload_bytes...]`

The decoder is tracked as `tools/acdb_driver_data_inventory.py` and validates record counts, offsets, schemas, param-count symmetry, and POOL bounds.

## Driver key IDs

The seven REV_0D schemas use:

- `0x01000006` render endpoint
- `0x0100000d` capture endpoint
- `0x01000010` channel count
- `0x01000029` DSP-GPIO/resource selector

## REV_0D module inventory

Eleven distinct driver-module IDs are present across fifteen module/schema records:

`0x08000020, 0x08000023, 0x08000027, 0x0800002a, 0x08000030, 0x08000040, 0x08000042, 0x08000050, 0x08000060, 0x08000080, 0x08000090`

## Internal speaker rows

For `render_endpoint=1`, stereo (`channel_count=2`), the material rows are:

### `0x08000090 / 0x08000091`

24-byte six-word payload:

`1, 3, 0, 4, 0, 1`

This is the already-recovered SP11 WSA884x 4-ohm / 2S profile. It is consumed directly by qcaucd and is already represented in the Linux WSA8845 integration. It is not the newly missing lower-hardware policy.

### `0x0800002a`

The 124-byte `0x0800002b` payload is the count-30 Windows endpoint gain-anchor table, followed by the already-recovered small gain-policy records `0x2c/0x2d`. This is qcadcm endpoint-volume infrastructure, not WSA-macro producer calibration.

### `0x08000020 / 0x08000021,22`

Stereo speaker `0x21` payload:

```
version/direction = 0
count = 2
0x00080000, 0x03010005, 0
0x00080001, 0x03010006, 0
```

The paired `0x22` payload is a four-property record:

```
0x10030015 = 5
0x10100013 = 0
0x10100006 = 0x24
0x100c000e = 1
```

Exact qcaucd decompilation proves Windows validates module `0x08000020`, expects parameter `0x08000021`, consumes its per-interface triples, then expects `0x08000022`, copies its property block into a device configuration object, and dispatches command `0x200` through qcaucd's generic hardware-device configuration path.

Cross-endpoint comparison is decisive: the same module describes render and capture interface families with systematic interface IDs and property blocks for speaker, headset and microphone endpoints. Therefore `0x08000020` is the generic codec hardware-interface descriptor/configuration module, not a speaker-only WSA compander coefficient block.

This is important because it closes another tempting hidden-speaker-calibration theory without assuming the Linux implementation is complete.

### `0x08000042`

This large qcadcm-side module is not referenced by qcaucd. Its large tables are qcadcm/AudioReach infrastructure rather than the qcaucd WSA profile request. It should not be promoted to a native WSA-macro theory without caller evidence.

## Consequence for the ugly-noise investigation

The rejected Linux DRE=0 cold candidate cannot currently be explained by:

- missing WSA8845 nominal-load profile (`0x90` is recovered and already implemented),
- missing ordinary DP2/COMP SoundWire lane (full FIFO comparison already closes its mask/timing),
- an undiscovered speaker-only `0x20` ACDB calibration (module `0x20` is generic interface configuration), or
- endpoint-volume driver data (`0x2a` is already recovered and unrelated).

The remaining unproven layer is the **LPASS WSA-macro producer / SoundWire / WSA8845 consumer lifecycle and producer data semantics**. Linux owns this natively while Windows delegates much of the policy through the DSP/codec stack. The next evidence gate remains the passive lifecycle trace; do not re-enable the rejected DRE state until producer readiness and teardown ordering are proven.


## Provenance correction during extraction

An initial working note incorrectly recorded the REV_0D SHA-256 as `c33c2b70...`. A four-copy inventory plus the pre-existing canonical project ledger confirms the exact REV_0D bytes are 440,150 bytes with SHA-256 `a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde`. The decoder output itself was generated from these canonical bytes. There is no second ACDB variant involved in this result.
