# SP11 MicArray qcaucd command-0x200 first/second pairing closed — 2026-08-25

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` remains untouched.
Evidence: qcaucd8380 ARM64 static RE + canonical REV_0D `0x08000021` rows already recovered for EP16 and EP2.

## Result

The previously missing command-`0x200` `first[i]` values are now recovered without another runtime initialization capture.

Exact command pairs are:

```text
EP16 / MicArray normal visible capture row:
  slot 0: first=0x00020001   second=0x01040000
  slot 1: first=0x00020000   second=0x01040001

EP2 / alternate physical capture row:
  slot 0: first=0x00020002   second=0x01020000
  slot 1: first=0x00020000   second=0x01020001
```

These are not inferred from sequence numbers. qcaucd's command builder copies the first and second words of each `0x08000021` interface triple into the two command-`0x200` vectors slot-for-slot.

## Exact qcaucd source-to-command proof

`FUN_14004cba0` first validates the backend driver-data blob against constants:

```text
qcaucd+0x4d740 = 0x08000020
qcaucd+0x4d744 = 0x08000021
qcaucd+0x4d748 = 0x08000022
```

The ARM64 around `qcaucd+0x4d370` then exposes the payload layout and the command construction directly.

With `x27` initially pointing to the returned `0x08000021` blob:

```asm
14004d370  add   x8,x27,#0xc
14004d374  ldr   w5,[x8,#0x4]          ; count from blob +0x10
14004d378  str   x8,[sp,#0x40]         ; working base = blob +0x0c
14004d37c  str   w5,[sp,#0xa4]         ; command request +0x04 = count
...
14004d3d4  umaddl x20,w23,w26,x27      ; working base + 12*i
14004d3d8  add   x8,sp,#0xc8
14004d3dc  ldr   w6,[x20,#0xc]         ; blob +0x18 + 12*i = triple.second
14004d3e0  str   w6,[x8,w23,UXTW #2]   ; request +0x28 + 4*i = second[i]
...
14004d408  ldr   w6,[x20,#0x8]         ; blob +0x14 + 12*i = triple.first
14004d40c  add   x8,sp,#0xa8
14004d410  str   w6,[x8,w23,UXTW #2]   ; request +0x08 + 4*i = first[i]
...
14004d438  ldr   w5,[x20,#0x10]        ; blob +0x1c + 12*i = triple.third
```

The request object begins at `sp+0xa0`, confirmed when the complete `0x150` bytes are copied and dispatched:

```asm
14004d4e8  mov   w8,#0x150
14004d4f4  add   x2,sp,#0xa0
14004d500  bl    FUN_140005450          ; copy request
...
14004d510  mov   w1,#0x200
14004d518  bl    FUN_1400266c8          ; dispatch command 0x200
```

Therefore the actual relationship is exact:

```text
0x08000021 triple.first  -> command 0x200 first[i]
0x08000021 triple.second -> command 0x200 second[i]
```

No transient command object is needed to recover the pair identity.

## Applying the already recovered REV_0D rows

The canonical EP16 row is:

```text
version/direction = 1
count = 2

triple 0 = (0x00020001, 0x01040000, 0)
triple 1 = (0x00020000, 0x01040001, 0)
```

The canonical EP2 row is:

```text
count = 2

triple 0 = (0x00020002, 0x01020000, 0)
triple 1 = (0x00020000, 0x01020001, 0)
```

The first/second command vectors are consequently the exact pair list at the top of this checkpoint.

## Resource identities after the already-proven translation maps

The first-vector callback map is:

```text
0x00020000..0x00020007 -> type3A IDs 8..15
```

The second-vector provider map is:

```text
0x01040000..07 -> provider indices 0..7
0x01020000..03 -> provider indices 8..11
```

So the two microphone rows reduce to:

```text
EP16 slot 0: type3A ID 9  + provider 0
EP16 slot 1: type3A ID 8  + provider 1

EP2  slot 0: type3A ID 10 + provider 8
EP2  slot 1: type3A ID 8  + provider 9
```

Important consequence: these microphone rows use the type3A family, not type39. They share type3A resource ID 8 on slot 1, while EP16 has unique ID 9 on slot 0 and EP2 has unique ID 10 on slot 0.

The provider is then paired with a compatible/free type15 hardware lane by the already-closed `FUN_14002ab28 -> type13 -> FUN_14002bbf0 -> type15` path.

## ACDResources.bin negative result

The Surface `ACDResources.bin` was also checked read-only before promoting the result:

```text
size   = 5423 bytes
SHA256 = 23c4beab4aabd229af86ca5fa4807e45bb2ecb7365445484aa5cf27414fe1779
```

None of the known first-vector or second-vector IDs occur as raw little-endian u32 values in that file. Its payload is the separate AeoB/AUCD platform-resource configuration (`COMPONENT`, `FSTATE`, `PSTATE`, PMIC votes, GPIO resources, etc.), not the `0x08000021` endpoint driver-data row.

Thus the correct offline source for this pairing is the ACDB/qcadcm driver-data payload that qcaucd receives and materializes, not `ACDResources.bin` itself.

## What is now closed

The complete capture-side chain is now:

```text
EpType/format descriptor
 -> resolve module 0x08000020 row
 -> fetch param 0x08000021 interface triples
 -> FUN_14004cba0 copies triple.first/triple.second slot-for-slot
 -> command 0x200 first[] / second[]
 -> first[i] -> type3A resource
 -> second[i] -> type13 provider -> compatible type15 lane
 -> associate resource + lane + provider for slot i
```

The prior need to capture another transient command-`0x200` packet solely to learn the first-vector values is eliminated.

## Next discriminator

The highest-value static target is now the concrete meaning of type3A resource IDs `8`, `9`, and `10` and their relationship to provider groups `0/1` versus `8/9`.

That should tell us what is shared between EP16 and EP2 (ID 8), what is EP16-specific (ID 9), and what is EP2-specific (ID 10), before any microphone topology is implemented on Linux.

A later runtime A/B between normal Windows MicArray and Voice Focus/Studio Effects remains useful for locating the optional NPU processing stage, but it is no longer needed to close the qcaucd first/second pairing.

## Safety / state

- Read-only static analysis only.
- No Windows setting change.
- No PnP restart.
- No ACDB modification.
- No register write.
- No Linux topology mutation.
- No reboot.
- SP11 remains stopped in KD at `nt!DbgBreakPoint` while this checkpoint is written.
