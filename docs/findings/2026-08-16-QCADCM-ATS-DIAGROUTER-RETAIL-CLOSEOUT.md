# qcadcm ATS DiagRouter retail-image closeout

Date: 2026-08-16
Status: GREEN negative closure for the installed retail Windows image

## Question

Can the statically recovered qcadcm ADIE realtime-calibration GET service be invoked through Qualcomm's native `\\Device\\DiagRouter` transport on this SP11 Windows installation, without adding new kernel code or using unsafe raw WSA MMIO reads?

## Live Windows result

A fresh one-shot Windows/KDNET boot showed that qcadcm's ATS Diag I/O-target global was NULL in steady runtime. `\\Device\\DiagRouter` was not present in the live object namespace when checked at the debugger.

qcadcm's ordinary live remote WDF targets were instead:

```text
\\Device\\GLINK
\\??\\ACPI#QCOM068F#0#{36079ae4-78e8-452d-af50-0cff78b2f1ca}
```

Neither is the ATS Diag transport.

## Offline installed-image search

The Windows system volume was mounted read-only under Linux and every installed `.sys` file beneath `Windows/System32/drivers` and `Windows/System32/DriverStore/FileRepository` was scanned case-insensitively for both the UTF-16/ASCII `DiagRouter` identity and the exact qcadcm DiagRouter IOCTL constants:

```text
0x80082400  INIT
0x80082404  REG
```

Exactly two installed binaries matched:

```text
qcadcm8380.sys
qcscm.sys
```

No third installed driver contains the router identity or those client IOCTL constants.

## qcscm discriminator

The exact installed `qcscm.sys` is Qualcomm System Manager SCM driver version `1.0.4160.6000` and SHA-256:

```text
4094764C39A93C433A15920C7E6DD339943A8124D71D7BD0DC113CC1DF219FC8
```

Its `DiagTarget.c` path was imported into Ghidra. The sole `\\Device\\DiagRouter` reference is in `FUN_1400044c8`, which:

1. calls `RtlInitUnicodeString(..., L"\\Device\\DiagRouter")`;
2. constructs a WDF I/O-target open-parameters structure;
3. opens the named object as a **remote WDF I/O target**;
4. services it through a worker loop;
5. closes the target afterward.

Therefore qcscm is another **DiagRouter client**, not the provider.

## Conclusion

On the installed retail SP11 Windows image, the Qualcomm ATS DiagRouter provider required by qcadcm's external realtime-calibration transport is not available in normal steady runtime. The recovered ATS packet format and read-only ADIE commands remain valid, but there is no supported native router endpoint currently installed/active to send them through.

Do not fabricate a public IOCTL against qcadcm's ADCM interfaces: those interfaces are kernel query-interface plumbing and no normal WDF user I/O queue was found. ARM64 KD also exposes no supported `.call` command here, so debugger function injection/hand-edited execution context is rejected.

## Consequence for H03

The useful result is not lost: the full qcadcm -> qcaucd read chain, public handle `0x1010`, and encoded WSA register namespace are already known. The next observation mechanism must avoid dependency on the absent lab DiagRouter. Preferred options are passive tracing of qcaucd's own sanctioned platform read/write helpers during native Windows lifecycle, or a narrowly scoped read-only diagnostic client only if its loading/provenance can be made safer than debugger execution injection.

Raw KD/APPS reads of physical `0x06b00000` remain permanently forbidden after the prior WHEA 0x124.
