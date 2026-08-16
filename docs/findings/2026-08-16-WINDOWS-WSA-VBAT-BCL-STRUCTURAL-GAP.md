# Windows WSA VBAT/BCL lifecycle missing from upstream Linux

Date: 2026-08-16  
Status: GREEN structural discriminator; Linux restoration candidate not yet evaluated

## Breakthrough

The passive Windows qcaucd WSA trace contains a coherent producer lifecycle that current upstream `lpass-wsa-macro.c` does not model at all. It is the Qualcomm WSA **VBAT/BCL attenuation path**, not miscellaneous register noise.

The preserved older Qualcomm `wsa_macro_enable_vbat()` implementation matches the Windows qcaucd lifecycle almost transaction-for-transaction, while the current Linux SP11 source no longer contains the VBAT DAPM widgets/routes or enable callback.

Current SP11 Linux source also caps the WSA regmap at:

```text
CDC_WSA_MAX_OFFSET = 0x0760
```

whereas the older Qualcomm WSA macro driver exposed a `0x1000` register aperture. The Windows ADSP Hardware Device Description independently reports the current SP11 WSA macro as `size=0x1000`.

## Windows / vendor lifecycle match

For each active speaker channel Windows performs the same state machine represented by the old Qualcomm `wsa_macro_enable_vbat()` path.

### Bring-up

1. `0x0180` — VBAT/BCL path control: set bit `0x10` (VBAT block clock).
2. `0x0184` — VBAT config: set bit `0x01` (enable VBAT block).
3. channel RX `RX_PATH_CFG1` (`0x0408` or `0x0488`): set bit `0x80` (384-kHz/interpolator VBAT path).
4. `0x0184`: clear bit `0x02` (attenuation mode).
5. enable softclip clock/mux dependency:
   - `0x0640` / `0x0660` CRC clock `0 -> 1`
   - `0x0118` softclip mux clock bits advance accordingly.
6. channel RX `RX_PATH_CFG1`: set bit `0x02` (VBAT at channel level).
7. program the nine BCL attack-gain bytes at `0x01dc..0x01fc`:

```text
ff 03 00   ff 03 00   ff 03 00
```

### Teardown

Windows mirrors the vendor path:

- clear RX CFG1 bit `0x80`;
- set VBAT config bit `0x02`;
- clear RX CFG1 bit `0x02`;
- zero all nine BCL attack-gain bytes;
- release the softclip clock/mux dependency;
- clear VBAT config bit `0x01`;
- clear VBAT path-control bit `0x10`.

The exact Windows RX CFG1 transitions are:

```text
RX0 0x0408: 0x6d -> 0xed -> 0xef ... 0x6f -> 0x6d
RX1 0x0488: 0x6d -> 0xed -> 0xef ... 0x6f -> 0x6d
```

## v2.5 CB_DECODE extension

The Windows ADSP descriptor map identifies:

```text
LPASS_WSA_CDC_CB_DECODE... base=0xb00900 size=0x60
```

followed by:

```text
LPASS_WSA_CDC_VBAT_TEMP base=0xb00980 size=0xc0
LPASS_WSA_CDC_PBR      base=0xb00b00 size=0x100
```

The passive qcaucd trace shows the same VBAT enable/disable routine additionally touching the first three `CDC_CB_DECODE` registers:

```text
0x0900: write 1 on enable, 0 on disable
0x0904: write 1 on enable, 0 on disable
0x0908: write 1 on enable, 0 on disable
```

`0x0908` behaves like a command/pulse-style register in the qcaucd trace: the write is observed while the following native read remains zero. Exact bit-field names are not recovered, so preserve these only as `CDC_CB_DECODE +0x0/+0x4/+0x8` rather than inventing semantics.

## Why this matters for H03

The prior producer experiments changed Windows-proven RX gain, compander coefficients and primary half-dB policy while leaving Linux on a producer topology that **omits Windows's active VBAT/BCL attenuation stage**. That makes the negative isolated curve/half-dB results unsurprising: the coefficients were evaluated against a materially different signal-conditioning path.

This is also a credible explanation for why forcing the Windows amp-side `DRE_CTL_1=0` state into Linux was unsafe. Windows's COMP stream reaches the WSA8845 after VBAT/BCL processing that current Linux never enables.

## Next safe candidate

The first restoration test should isolate VBAT/BCL itself on the current best RX84 / generic-curve / CSR-assisted baseline:

- retain RX84 / 0 dB;
- retain current generic compander curve and current half-dB policy;
- retain safe CSR-assisted WSA8845 lifecycle;
- restore only the directly Windows-proven VBAT/BCL producer lifecycle, including the v2.5 CB_DECODE trio;
- do not enable the softclip effect; only reproduce its clock/mux dependency;
- mirror teardown exactly.

If this is safe and acoustically improves the Windows residual, then layer the recovered Windows compander/half-dB state on top in subsequent bounded steps. `DRE_CTL_1=0` remains forbidden until producer parity is stable.

## Evidence

Windows native trace:

```text
artifacts/reviewed/2026-08-16-windows-qcaucd-wsa-passive.log
```

Windows ADSP hardware descriptor map:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\qcadsp-wsa-hw-descriptors-20260816.txt
```

Older Qualcomm reference driver retained on SP7:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\vendor-wsa-macro-48696dd.c
```
