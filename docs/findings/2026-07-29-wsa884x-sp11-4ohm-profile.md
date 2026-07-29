# Exact SP11 WSA884x load record and coherent 2S profile

Date: 2026-07-29

## Result

The Surface Pro 11 REV_0D ACDB contains an exact WSA codec-driver record whose
six 32-bit words are:

```text
1, 3, 0, 4, 0, 1
```

Windows copies these six words directly into the `qcaucd8380.sys` WSA
configuration globals. Static control-flow analysis proves that the fourth
word is the nominal speaker-load selector: the driver branches on values 4, 6,
8 and 32 ohms. The SP11 value is therefore **4 ohms**. The prior measured
left/right resistance values near 5 ohms were calibration results, not the
nominal-load category.

This closes the load ambiguity left by patch `0021`.

## Reproducible ACDB path

Source:

```text
surface_acsp8380.inf_arm64_f6524e4db745e12a/REV_0D/acdb_cal_0D.acdb
```

Driver-data resolution:

| Structure | Value |
| --- | --- |
| GCLU module ID | `0x08000090` |
| GCLU key-table offset | `0x18` |
| GCLU calibration-table offset | `0x340` |
| GCKT key ID | `0x01000006` |
| key value | `0x1` |
| GCDE definition offset | `0x8c` |
| parameter ID | `0x08000091` |
| GCDO data offset | `0x228` |
| POOL payload offset | `0x219f8` |
| payload size | 24 bytes |
| payload words | `1, 3, 0, 4, 0, 1` |

The decode follows Qualcomm AudioReach GraphServices' definitions:

- GCLU entries are module ID, key-table offset and calibration-table offset;
- GCKT contains key IDs;
- GCDT selects GCDE/GCDO offsets using key values;
- GCDE supplies parameter IDs;
- GCDO supplies POOL offsets.

Earlier ad-hoc extraction treated offsets as lengths and produced false
references. Those results must not be used as ground truth.

## Windows and Qualcomm register convergence

Static analysis of the exact SP11 `qcaucd8380.sys` proves:

- load 4 or 6 ohms selects `OCP_CTL = 0xf6`;
- the 2S codec-object configuration selects the `0x3091 = 0x44` path;
- gain 18 dB is encoded as `0x24` in the half-dB Windows selector;
- 18 dB selects `VSENSE1 = 0x67`;
- the 4-ohm current-sense profile selects `ISENSE2 = 0x07`.

Qualcomm's downstream Linux tables independently converge on the same
18 dB/4-ohm pairing. For `G_18_DB`, `CONFIG_2S`, `WSA_4_OHMS`, they define:

```text
808 839 894 925 973 996 1051 1114
1184 1255 1318 1467 1616 1788 2000
```

After the driver's threshold conversion, registers `0x34e1..0x34ef` become:

```text
54 58 5f 63 69 6c 73 7b 84 8d 95 a8 bb d1 ec
```

## Linux mismatch before correction

After patch `0021`, both live codecs still carried the generic upstream
1S/8-ohm/21-dB values:

| Register | Live value | SP11 profile |
| --- | ---: | ---: |
| `0x3020` | `0x87` | `0x67` |
| `0x3021` | `0x27` | `0x07` |
| `0x304c` | `0xc6` | `0xf6` |
| `0x34e1..0x34ef` | `5b 62 6a 72 79 7e 84 8b 90 9d aa b7 c1 d0 ec` | `54 58 5f 63 69 6c 73 7b 84 8d 95 a8 bb d1 ec` |

The reboot preceding this correction logged five identical left-channel
faults:

```text
PA fault: sta0=0x0 sta1=0x6 err0=0x8 err1=0x0; resetting FSM
```

Each recovered once, proving that bounded recovery worked but that the mixed
profile had not removed the underlying mismatch.

## Implemented candidate

Patch
[`0022-wsa884x-apply-sp11-2s-4ohm-profile.patch`](../../patches/0022-wsa884x-apply-sp11-2s-4ohm-profile.patch)
applies the four coupled parts as one profile:

- 18 dB / 4-ohm current and voltage sensing gains;
- Windows' 4/6-ohm OCP value;
- Qualcomm's exact 2S/4-ohm/18-dB 15-step PBR thresholds;
- the existing exact 2S analogue and current-limit configuration.

Build and pre-boot deployment:

```text
kernel:                  7.1.5-sp11-audio-vi
new module srcversion:   203517BBF9C87B3E6B2210C
compressed module SHA:   56f70402882b4c48bed4411a0350b8e05b5da599766e048e49e5df01e0ff23eb
rollback module SHA:     beaaeaf0a87cee9c6550e70a8e8e67ecb34713e0f8f759e2b0c53470a6e0a5fa
kernel source commit:    c9d74235e4f826a3830f5c073bd9d87d77360ee1
```

The module is signed by the kernel's existing build key and installed. Runtime
register and stress validation requires the next boot; this document does not
claim that validation in advance.
