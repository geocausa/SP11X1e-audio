# MicArray qcaucd type-0x1D RPMH_COMMIT closure

## Result

The previously unnamed qcaucd global resource type `0x1D` is now statically closed as **`RPMH_COMMIT` / `RpmhCommit`**.

This is the same live resource consumed by qcaucd's DMIC executor at `FUN_140033828` / RVA `0x33828`:

```text
+0x00 type  = 0x1D
+0x04 id    = 0xFFFFFFFF
+0x08 value = 8
+0x0C       = 0
```

The earlier mode mapping remains valid:

```text
4  -> mode 0
6  -> mode 1
8  -> mode 2   <- current SP11
16 -> mode 4
32 -> mode 5
```

Thus the current MicArray path is:

```text
RPMH_COMMIT value 8
  -> qcaucd mode 2
  -> DMIC group0 register 0x3084 bits[3:1] = 010
  -> byte 0x04 configured
  -> byte 0x05 while run bit0 is enabled
```

## Provider/source closure

qcaucd does not synthesize this resource at capture time. Its initialization path:

```text
AUCD\ASLResourceFile\BinaryPath
  -> ZwOpenFile / ZwReadFile
  -> parser callback registered dynamically
```

A live KD read of qcaucd's resolved parser callback showed:

```text
qcaucd DAT_140017ea0
  -> qcpep8380.sys + 0x39b20
```

The exact SP11 qcpep image used for static verification is:

```text
C:\Windows\System32\DriverStore\FileRepository\qcpep.wd8380.inf_arm64_eb671871f8fef501\qcpep8380.sys
SHA-256 e5bdeb8c0fb8823990f7520a19c00807711cf909f91f8d67db78ffb18c1b2277
```

`qcpep8380+0x39b20` calls the bundled ACPI/resource binary parser at RVA `0x40bc08`.

## Resource factory proof

qcpep's parsed-resource factory at `FUN_140400f20` maps the literal resource name `RPMH_COMMIT` to resource code `0x1D` and constructs:

```text
FUN_14000f298(obj, 0x1D)
```

That constructor installs vtable RVA `0x288660`.

The vtable's virtual name method at RVA `0x0f300` returns exactly:

```text
"RpmhCommit"
```

Therefore:

```text
qcaucd type 0x1D
  == qcpep resource code 0x1D
  == factory literal RPMH_COMMIT
  == virtual class name RpmhCommit
```

This removes the earlier ambiguity that `0x1D` might merely be an unidentified clock/rate/divider resource.

## Remaining open semantic

The **resource type** is closed, but the meaning of its current payload value `8` is not yet closed. It must not yet be called a sample rate, clock divider, or RPMh command-set ID without additional evidence.

The next target is the `RpmhCommit` parser/executor field flow: determine what ACPI/ASL field produces the live value `8`, then identify why qcaucd maps its allowed values `{4,6,8,16,32}` to DMIC modes `{0,1,2,4,5}`.

## Evidence

Machine-readable oracle:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-qcaucd-type1d-rpmh-commit.json`

Static scratch evidence used during closure remains outside the release branch workspace; the qcpep binary itself is deliberately not committed.

## Safety/state

The live callback/list inspection was read-only. A malformed temporary WinDbg loop left SP11 stopped at the probe once; the live resource list was extracted, all breakpoints were cleared, and SP11 was resumed. No reboot, device restart, service restart, registry mutation, or audio-setting mutation was performed.
