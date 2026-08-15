# QCADSP private codec-register resource is not sourced by REV_0D ACDB

Date: 2026-08-15  
Status: GREEN negative closure for the recovered REV_0D ACDB/private-driver-data path; external runtime use of the service remains OPEN

## Private service recovered from qcadsp8380.mbn

Static Hexagon reverse engineering recovered Qualcomm's private `audio_hw_cdc_reg_cfg` service. The firmware registers:

- resource ID: `0x0800131b`
- request handler: `FUN_b0282f94`

The handler accepts one or more register descriptions containing a register ID, physical address, and operation/value list. Client operation IDs are constrained to `1..3`; the internal list helper additionally uses an internal operation 4.

This is a real ADSP codec-register hardware-resource service, but the existence of the service does not prove that the SP11 internal-speaker graph uses it.

## ACDB correlation test

The complete recovered Microsoft Surface REV_0D ACDB driver-data inventory was searched, including all 189 extracted payload blobs.

Source ACDB SHA-256:

`a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde`

Search targets:

- private service parameter family `0x08001347..0x08001351` recovered from the firmware handler/profile logic;
- WSA macro physical base `0x06b00000`;
- COMPANDER0 `0x06b00580`;
- COMPANDER1 `0x06b005e0`;
- every aligned 32-bit value in the WSA macro physical range `0x06b00000..0x06b00fff`.

Result:

- exact private-ID payload hits: **0**;
- exact WSA-address payload hits: **0**;
- aligned WSA-range payload hits: **0**;
- ACDB parameter IDs in `0x08001300..0x08001400`: **none**.

## Conclusion

The normal REV_0D speaker ACDB/private-driver-data pool is **not** carrying a hidden WSA-macro codec-register program for resource `0x0800131b`.

That removes another plausible source of the Windows-only producer behavior. The service may still be requested dynamically by another graph/runtime component, so the next proof target is a true boot-zero qcadcm hardware-resource trace plus graph-manager static analysis—not synthetic invocation of the private resource.

Reviewed evidence:

- `artifacts/reviewed/2026-08-15-qcadsp-codec-resource-vs-acdb.json`

No production audio state was changed by this analysis.
