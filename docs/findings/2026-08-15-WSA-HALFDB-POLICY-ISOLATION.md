# WSA-macro half-dB producer policy isolation

Date: 2026-08-15  
Status: SAFE isolated candidate; Windows state remains AMBER / not directly observed

## Background

The exact CPS-v3 active-runtime trace proved that whenever Linux activates the WSA-macro compander producer it also enables four half-dB PGA bits:

```text
0x0428 RX0_RX_PATH_SEC1         mask 0x01 -> 0x01
0x0444 RX0_RX_PATH_MIX_SEC0     mask 0x01 -> 0x01
0x04a8 RX1_RX_PATH_SEC1         mask 0x01 -> 0x01
0x04c4 RX1_RX_PATH_MIX_SEC0     mask 0x01 -> 0x01
```

The native driver associates that behavior with the unconditional internal policy:

```text
spkr_gain_offset = WSA_MACRO_GAIN_OFFSET_M1P5_DB
```

This is generic driver policy, not a Surface/Denali ACDB selection. Windows equivalence has never been directly observed because the Windows WSA-macro aperture is ADSP-owned and not readable from APPS KD.

This candidate was designed only to answer whether that half-dB producer policy can be removed safely and independently. It does **not** claim Windows disables it.

## Provenance-clean exact-binary patch

The accepted original CPS-v3 WSA-macro binary remains available even though the reconstructed source has drifted:

```text
/home/geoca/Documents/SP11-PROJECT/02-kernel/build-cps-v3-20260811/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko
SHA-256 dd5d18c650610137b2e2d2f1f56661592d879489f51c94ae2d00b6f98822844e
srcversion F681186BB3D24B32621905D
```

`wsa_macro_enable_interpolator()` is at module text offset `0x2638`. In the POST_PMU path, the exact production binary contains:

```text
2a4c  ldr   w1,[x20,#0x94]     ; spkr_gain_offset
2a50  ldr   w0,[x20,#0x8]      ; COMP state
2a54  cbnz  w1,2ab8             ; skip half-dB block when gain offset != M1P5
...
2a64..2ab0                      ; four half-dB update_bits(...,1) calls
2ab8                            ; continue into ear-speaker gain logic
```

The candidate replaces only the `cbnz` at `0x2a54` with an unconditional branch to the existing continuation at `0x2ab8`:

```text
before  0x35000321  cbnz w1,0x2ab8
after   0x14000019  b    0x2ab8
```

The module `.text` starts at file offset `0x1c8`, so the patch lives at file offset `0x2c1c`.

Only three bytes actually differ because one byte is `0x00` in both encodings:

```text
0x2c1c  21 -> 19
0x2c1d  03 -> 00
0x2c1f  35 -> 14
```

Hashes:

```text
baseline unsigned  dd5d18c650610137b2e2d2f1f56661592d879489f51c94ae2d00b6f98822844e
candidate unsigned 1d15dac5960bbd53dda7a0a2c09292dc2d5248e5e094097ccc2181c161adb600
candidate signed   8e612fac32979fba2f027f5dd8cb9f49bf8488d9dc7e4f989fdc25bd9f7a0d75
candidate ko.zst   da72d2b63fec7807893fb974ea1143c044714b5d24ec21153e432ef5ea6cf862
```

The signed candidate retains the exact production srcversion `F681186BB3D24B32621905D`, exact CPS-v3 vermagic, and the same CPS-v3 signer/key ID. The private signing key is not stored in the project repository.

## Isolated boot

The candidate was embedded only in a private initramfs and early-loaded over the rollback root module. Kernel and DTB were byte-identical to CPS-v3:

```text
kernel  164bc92d88c724ac4e7872212405c8149cd52a8ee1553d54e11d56581751fc48
DTB     7cd5fdd8ef59c46ca9a3661adacce0444893a6c26fca71c97eaa3070a88aab84
initrd  9e2789b9802c23f69002455183114ff29d0c953954efbffa13a51119334c74ec
```

One-shot GRUB ID: `sp11-audio-wsa-halfdb-off`. Persistent saved entry remained `sp11-audio-cps-v3`. Endpoint was pinned to 5% before reboot. Macro Digital Volume stayed at baseline 81/-3 dB. WSA8845 CSR/DRE state was not changed.

Loaded identities passed:

```text
WSA macro   F681186BB3D24B32621905D
machine     13326073E27DFA035180C56
WSA884x     E084BC31719EE85BB8DEABD
```

## Exact live-write proof

The same passive ASoC runtime-write tracer used for baseline was armed for the candidate boot.

Candidate trace:

- `artifacts/reviewed/2026-08-15-wsa-halfdb-off-runtime-write.trace`
- SHA-256 `a04c5f2092a76df2ae863dbe7778ff6616e23d2968e161db99483848f821b444`
- 175142 bytes
- metadata SHA-256 `67a9f6344086b13ad3b7c8aa7e7dd951b362fb23f1ca0a57fd3ad2662d962b73`

Baseline contained 190 filtered WSA-macro `update_bits` events; candidate contained 166. A multiset diff of `(reg,mask,value)` operations produced exactly four removed operation types and **no added operations**:

```text
removed 0x0428 mask 0x01 value 0x01  x6
removed 0x0444 mask 0x01 value 0x01  x6
removed 0x04a8 mask 0x01 value 0x01  x6
removed 0x04c4 mask 0x01 value 0x01  x6
added: none
```

Compander enable/reset sequences, RX0/RX1 COMP enable, BOOST0/1 path enable and the rest of the traced WSA-macro activation are operation-for-operation unchanged. POST_PMD `value=0` clears remain; when the bits were never enabled they are harmless idempotent clears.

This is strong proof that the binary candidate isolates exactly the intended producer gain-offset policy.

## Bounded playback gates

### Endpoint 5%, baseline macro Digital Volume -3 dB

Exact reference MP3 ran for four seconds. No WSA/PA/SoundWire/XRUN speaker fault was recorded.

Read-only WSA observer:

```text
left:  PA=1, current-limit code=17, PA error=0
right: PA=1, current-limit code=17, PA error=0
```

Observer SHA-256: `69f6c14d0b7ab9c45b2839bbfcdc7b910bf5f84472431173401ea55766e3275c`.

### Endpoint 12%, baseline macro Digital Volume -3 dB

Exact reference MP3 ran for four seconds:

```text
PLAY_START 2026-08-15T15:56:25.467846417+01:00
PLAY_END   2026-08-15T15:56:29.509215327+01:00
```

No WSA/PA/SoundWire/XRUN speaker fault was recorded. Macro Digital Volume remained 81/-3 dB throughout.

## Result

**Operational result:** disabling only the generic half-dB PGA enable policy is safe in the current protected CSR-assisted path at both 5% and 12% endpoint levels. The exact live trace proves no other WSA-macro update operation changed.

**Parity result:** still AMBER. This experiment does not tell us whether Windows runs those four bits clear or set. It does establish that the Linux generic `M1P5_DB` behavior is not required for basic safe operation and can be independently auditioned/compared later.

**DRE result:** unchanged. This candidate never disables CSR and therefore does not by itself close the unsafe Windows-style COMP-only mode. Do not combine this change with macro 0 dB and DRE/CSR changes in a single experiment without a new evidence gate.
