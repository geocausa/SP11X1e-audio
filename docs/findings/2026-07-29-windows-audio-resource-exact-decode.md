# Exact decode of SP11 Windows endpoint-component and DSP-GPIO resources

Date: 2026-07-29

## Scope

This note replaces the earlier heuristic interpretation of the Surface ACDB
`0x08000040/41` and `0x08000060/61/62` driver-data records. It also records the
actual contents of the buried Windows `ADCMResources.bin` and separates them
from the codec-side `ACDResources.bin` mechanism.

No Linux or Windows hardware state was changed while producing this result.

## Sources

The exact lookup semantics were reconstructed from Qualcomm's recovered ACDB
source under:

```text
00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/
  Research_Hub_Audio/SOURCE/audioreach_src/
  audioreach-graphservices/acdb/
```

The four Surface extension databases are:

```text
00-RE-archive/sp11-driverdump/
  surface_acsp8380.inf_arm64_f6524e4db745e12a/
  REV_0A/acdb_cal_0A.acdb
  REV_0B/acdb_cal_0B.acdb
  REV_0C/acdb_cal_0C.acdb
  REV_0D/acdb_cal_0D.acdb
```

A reproducible exact decoder is retained at:

```text
01-audio/artifacts/offline-audit-20260729/
  decode_driver_data_exact.py
  acdb-driver-data-exact/rev-0A.json
  acdb-driver-data-exact/rev-0B.json
  acdb-driver-data-exact/rev-0C.json
  acdb-driver-data-exact/rev-0D.json
```

The Windows driver was independently analysed in a fresh Ghidra project:

```text
01-audio/artifacts/offline-audit-20260729/ghidra-qcadcm/
```

Canonical `qcadcm8380.sys` SHA-256:

```text
37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429
```

## Why the previous extraction was wrong

The older `acdb_driver_data_extract.py` treated every plausible word in a
GCDT neighbourhood as a possible pool offset. That can find bytes which look
structurally valid but are not selected by the ACDB lookup tree.

Qualcomm's source establishes the exact chain:

```text
GCLU row
  -> GCKT key schema
  -> GCDT exact key-value row
  -> GCDE parameter-ID group
  -> GCDO matching data-offset group
  -> POOL length-prefixed payload
```

Following that chain eliminates the false positive which had been interpreted
as two endpoint components or regulators.

## Exact endpoint-component result

Every Surface ACDB revision has one record for:

```text
MODULE_ID_ENDPOINT_COMPONENTS_CONFIG = 0x08000040
PARAM_ID_ENDPOINT_COMPONENTS_CONFIG  = 0x08000041
```

It has no key vector and selects one eight-byte payload:

```text
00 00 00 00 00 00 00 00
```

The leading component count is zero. There is therefore no active Surface
endpoint-component list in these ACDB files.

The recovered OSAL memory image:

```text
osal_ffff808361423000_0x6c000.bin
```

is byte-for-byte the REV_0D ACDB followed by 2,218 zero bytes. It contains no
runtime replacement table and no hidden endpoint-component payload.

### Consequence

The generic Windows code for endpoint components is real, but the exact SP11
extension ACDB selects zero components. The prior claim that components 4 and
5 represented active endpoint regulators is not supported and must not guide a
Linux hardware change.

## Exact DSP-GPIO result

Every Surface ACDB revision has:

```text
MODULE_ID_DSP_GPIO_CONFIG = 0x08000060
PARAM_ID_DSP_GPIO_ENABLE  = 0x08000061
PARAM_ID_DSP_GPIO_DISABLE = 0x08000062
key ID                    = 0x01000029
key values                = 0 through 7
```

Ghidra confirms that qcadcm fills key `0x01000029` from the endpoint's codec
interface type. Case 7 of the audio-hardware-resource dispatcher selects
`0x08000061` for enable and `0x08000062` for disable, then sends the resulting
custom configuration to the DSP.

Each payload is:

```text
u32 entry_count
entry_count * {
    u32 direction;
    u32 drive;
    u32 function_select;
    u32 gpio_pin;
    u32 gpio_type;
    u32 pull;
}
```

The exact eight keyed variants are preserved in the JSON decodes. The mapping
from codec-interface enum value to a named interface such as SoundWire remains
unresolved. It is therefore not safe to choose a row by appearance or to copy
one into Linux yet.

## Buried PEP resource binaries

The deep filesystem/INF scan located the previously overlooked files:

```text
ADCMResources.bin
ACDResources.bin
RADS.bin
ADC1.bin
```

Their hashes and every duplicate location are recorded in:

```text
01-audio/artifacts/offline-audit-20260729/resource-binaries/
  all-copies.sha256
```

`ADCMResources.bin` is an AeoB v1 PEP resource package for `\_SB.ADSP.ADCM`.
It declares twenty numbered components, but their transitions contain only the
generic F-state/P-state scaffolding. The exact decode contains no PMIC voltage
vote, TLMM GPIO, clock, bus-arbitration or equivalent physical resource action.

`ACDResources.bin` belongs to the separate AUCD codec-side resource mechanism
and does contain real codec PMIC/GPIO resources. It must not be conflated with
qcadcm endpoint components or assumed to be an omitted WSA884x amplifier rail.

Reproducible decodes are retained at:

```text
01-audio/artifacts/offline-audit-20260729/resource-binaries/
  ADCM-decode.txt
  ACD-decode.txt
  RADS-decode.txt
```

## Revised conclusion

The earlier broad statement "Linux lacks the Windows endpoint-component,
DSP-GPIO and PEP lane" needs qualification:

1. The generic Windows qcadcm mechanisms exist.
2. The exact Surface ACDB selects zero endpoint components.
3. The matching ADCM PEP package has no concrete resource operations.
4. DSP-GPIO has eight real variants, but the SP11 speaker endpoint's selected
   codec-interface key and runtime invocation remain unproved.
5. The separate AUCD codec-resource mechanism is real but is not evidence of a
   missing WSA amplifier power rail.

The endpoint-component/PEP theory is therefore downgraded from a leading audio
cause to a mostly closed false lead. DSP-GPIO remains a bounded parity question,
not an actionable correction.
