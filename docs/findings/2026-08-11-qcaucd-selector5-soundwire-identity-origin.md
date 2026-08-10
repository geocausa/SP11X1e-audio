# qcaucd selector-5 origin: SoundWire identity classification

Date: 2026-08-11 (Europe/London)

## Result

The remaining strict-Windows question above the qcaucd CPS port templates is
closed statically.

The selector value `5` used by `FUN_14003ec58` to choose the Surface Pro 11
WSA CPS template table is not supplied by the searched qcadcm/CPS public
parameter paths and is not selected dynamically by the playback dataport
programmer. It is created locally by qcaucd's SoundWire slave constructor
`FUN_140031430` (RVA `0x31430`) after classifying the slave's six identity
bytes.

For the WSA8845 family, the constructor compares the six identity bytes against
an initialized `.rdata` mask at RVA `0x12ef8`:

```text
20 02 17 02 04 00
```

Both Surface Pro 11 speaker identities match that mask:

- left `0x0000000402170220` -> low six bytes, little-endian:
  `20 02 17 02 04 00`;
- right `0x0000000402170221` -> low six bytes, little-endian:
  `21 02 17 02 04 00`.

The comparison is `(identity_byte & mask_byte) == mask_byte`. Therefore the
right speaker's first byte `0x21` intentionally still matches the first mask
byte `0x20`; the low identity bit distinguishing the two slaves is ignored for
family classification.

When this WSA8845-family branch matches, `FUN_140031430`:

1. allocates/initializes the qcaucd slave descriptor;
2. changes the registration object's class field at `+0x18` to `0x50000`;
3. stores the descriptor at registration-object `+0x10`;
4. reads SoundWire slave registers `0x3401..0x3404` into that descriptor;
5. writes the initialized 64-bit constant from RVA `0x31b30` at descriptor
   `+0x0c`.

The constant is:

```text
05 00 00 00 01 00 00 00
```

or `0x0000000100000005` little-endian. Consequently:

- descriptor `+0x0c` dword = **5**;
- adjacent descriptor `+0x10` dword = `1`.

`FUN_14003ec58` later receives the registered object, follows its secondary
descriptor pointer, reads descriptor `+0x0c`, and selects:

- value `4` -> table base RVA `0x15c60`;
- value `5` -> table base RVA `0x15b70`.

The selector-5 table is the one already proven to contain the exact SP11 CPS
port-13/port-14 -> slave-DP6 templates with OffsetCtrl1 `0x00` / `0x19`.

Thus the full immediate Windows HLOS chain is now:

`SoundWire WSA8845 identity`
-> `FUN_140031430` identity-family classifier
-> descriptor selector `5`
-> `FUN_140021a40` runtime registration
-> lookup objects `(2,5..8)`
-> `FUN_14003ec58` selector-5 static port templates
-> `FUN_14003df18`
-> `FUN_14003bf40` dataport programmer
-> `FUN_14003ac60` per-slave command primitive
-> observed DP6 writes.

No additional KD session was required.

## Binary/hash gate

Reviewed image:

- `qcaucd8380.sys`
- SHA-256 `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`
- source:
  `C:\Users\SurfacePro7\Documents\blobs\sp11-driverdump\qcaucd8380.inf_arm64_53bcc309a68aba55\qcaucd8380.sys`.

All constants used here are in initialized image-backed sections, unlike the
runtime registration tables described below.

## Constructor classification branches

`FUN_140031430` first checks the incoming SoundWire identity family before the
registration object has a final class.

### Selector-4 family

Identity mask at RVA `0x12ef0`:

```text
20 02 17 02 02 00
```

When all six bytes pass `(id & mask) == mask`, the constructor creates class
`0x40000` and writes selector dword `4` at descriptor `+0x0c`.

Neither SP11 WSA8845 identity matches this mask.

### Selector-5 / WSA8845 family

If the selector-4 mask does not match, the constructor tests RVA `0x12ef8`:

```text
20 02 17 02 04 00
```

Both known SP11 WSA8845 identities match this mask. The branch creates class
`0x50000` and writes the qword at RVA `0x31b30`:

```text
05 00 00 00 01 00 00 00
```

The selector consumed later by `FUN_14003ec58` is therefore deterministically
`5` for both speakers.

## Runtime registration bridge

The BSS xref pass identified `FUN_140021a40` (RVA `0x21a40`) as the routine
that populates the runtime registration/lookup arrays used later by
`FUN_140022e80`.

Important distinction: the arrays around RVAs `0x177xx..0x17cxx` reside in the
uninitialized tail of `.data` (BSS). They must **not** be interpreted by reading
on-disk bytes at those RVAs. Their values are written at runtime.

Initialization path:

- `FUN_140022c20(8)` zeroes eight 0x30-byte registration slots and sets their
  default type field;
- `FUN_140022b68(8)` zeroes eight parallel 0x18-byte lookup slots;
- `FUN_140022cf8` seeds slot 0 from a 24-byte platform descriptor obtained via
  `FUN_140028d90`;
- `FUN_140022df0` publishes the 0x184-byte registration block through an
  indirect callback.

For SoundWire slave objects, `FUN_140021a40` populates later slots. In its
`0x40000/0x50000` branch it reads the constructor-created descriptor and stores:

- lookup class `2`;
- subtype `5`, `6`, `7`, or `8` from another descriptor field;
- the associated object pointer `param_1 + 8`;
- additional controller/port metadata.

`FUN_140036510` later resolves `(2,5)`, `(2,6)`, `(2,7)`, `(2,8)` through
`FUN_140022e80` and feeds those objects to the port-configuration callback.

The lookup subtype `5..8` and the template selector `5` are separate fields.
The crucial point for CPS is that the template selector is already present in
the secondary descriptor created by `FUN_140031430` from SoundWire identity.

## Evidence outside Git

Selector-classifier evidence summary:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-selector5-identity-classifier.txt`
- size 1,044 bytes
- SHA-256 `FFD82125DC2AB015D15FC166F5BF61E1B497E4A797FCEBE60166601BE59EFAE5`.

Constructor decompile:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-slave-constructor-selector.txt`
- size 12,355 bytes
- SHA-256 `325FF4DC7CF08D1C838021EE2907B86F08DD7AC58FA4770DE18F9A4D99F9A8B3`.

BSS registration writers:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-bss-registration-writers.txt`
- size 68,530 bytes
- SHA-256 `3BC90A000C9263A045393A313ABB39B7CC1176ADD833F79E0920BDA1F7438712`.

Registration callback callers:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-registration-callback-callers.txt`
- size 10,821 bytes
- SHA-256 `59777E3509CA81068971F0D2AB1B00E5C30742EC51E478A4D908E959048B7630`.

Registration initialization path:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-registration-init-path.txt`
- size 14,127 bytes
- SHA-256 `BC51557C6F213667F433F60CEBEB6282AA6FDA9BA1719A65CA504012CEB51667`.

## What this closes

The previously open question, "what chooses selector 5?", is closed: qcaucd
chooses it locally from the SoundWire slave identity family. The exact CPS DP6
geometry is then taken from qcaucd's selector-5 static templates.

There is therefore no present evidence that the missing public
`PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) is needed to supply this
per-speaker HLOS geometry on the reviewed Windows driver build. This statement
is limited to the immediate Windows HLOS SoundWire configuration chain; it does
not claim the public Qualcomm parameter is globally unused in every firmware or
platform.

## Updated decision

Do not reopen KD merely to read selector 5, and do not repeat qcadcm,
registration, template-copy, dataport, slave-command, or FIFO traces.

The Windows reverse-engineering chain needed for SP11 Linux audio parity is now
sufficiently closed at the HLOS level. The productive next decision is to move
to the Linux parity implementation/review using the proven values and normal
SoundWire/WSA configuration paths:

- WSA slave DP6 on both speakers;
- mask `0x03` on both;
- left OffsetCtrl1 `0`;
- right OffsetCtrl1 `25`;
- 24 kHz / 800-clock timing;
- master-port/template ordering consistent with port 13 left and port 14 right;
- no physical-MMIO programming workaround.
