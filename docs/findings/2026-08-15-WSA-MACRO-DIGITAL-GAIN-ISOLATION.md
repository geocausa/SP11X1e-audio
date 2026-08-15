# WSA-macro Digital Volume isolation: Linux -3 dB safety cap

Date: 2026-08-15  
Status: SAFE isolated candidate; Windows value remains AMBER / not directly observed

## Why this was reopened

The exact CPS-v3 active-runtime trace showed that Linux writes both LPASS WSA-macro receive volumes to raw `0xfd`, which ALSA exposes as value 81 / **-3 dB**:

```text
WSA WSA_RX0 Digital Volume = 81
WSA WSA_RX1 Digital Volume = 81
```

The generic X1E machine driver itself documents the limit as a temporary speaker-damage precaution "until we have active speaker protection in place." SP11 now has the reconstructed Windows protection path, so that historical cap is not by itself Windows-parity evidence.

The common Microsoft qcadsp/silicon first-MCLK state has `RX0_RX_VOL_CTL=RX1_RX_VOL_CTL=0x00`, and fresh Windows qcadcm resource tracing found no host-sent WSA-macro register table during native speaker start. That makes 0 dB a meaningful parity hypothesis, but not a direct Windows register observation because the Windows WSA macro is secure/ADSP-owned.

## Exact-production provenance problem

The current reconstructed `x1e80100.c` source no longer rebuilds to the accepted CPS-v3 machine-driver identity:

- accepted/original srcversion: `13326073E27DFA035180C56`;
- current reconstructed-source rebuild: `DC79B20269420B884EA56B1`.

A scan of preserved source snapshots found no source copy that rebuilds to the accepted srcversion. Therefore this experiment did **not** deploy a source-rebuilt machine driver.

The exact original unsigned CPS-v3 module remains available:

```text
/home/geoca/Documents/SP11-PROJECT/02-kernel/build-cps-v3-20260811/sound/soc/qcom/snd-soc-x1e80100.ko
SHA-256 9b9f4fda692c68807e19161e65eb596df288d25d2878a37623ffa7ce848f2b45
srcversion 13326073E27DFA035180C56
```

## Four-byte binary patch

Disassembly proves the accepted binary contains exactly four `mov w2,#81` instructions immediately before the four `snd_soc_limit_volume()` calls for:

- `WSA WSA_RX0 Digital Volume`;
- `WSA WSA_RX1 Digital Volume`;
- `WSA2 WSA_RX0 Digital Volume`;
- `WSA2 WSA_RX1 Digital Volume`.

The unsigned module's `.text` starts at file offset `0x70`. The candidate changes exactly four bytes:

| file offset | before | after | instruction |
|---|---:|---:|---|
| `0x3e4` | `0x22` | `0x82` | `mov w2,#81` -> `mov w2,#84` |
| `0x400` | `0x22` | `0x82` | same |
| `0x414` | `0x22` | `0x82` | same |
| `0x428` | `0x22` | `0x82` | same |

A byte-for-byte delta gate confirmed those are the **only four changed bytes**. The patch raises only the ALSA ceiling from 81/-3 dB to 84/0 dB. It does not change the boot-time actual value; UCM still requests 81 until the experiment explicitly changes it.

Candidate hashes:

```text
unsigned  040a01da48538042f03eb49382c08b5d2a720dbe0d8fe3e8305ae1311b993cf3
signed    3e9b6f09a0f341f18677e5272de92c5129c998fd591e36f68528f711959d7b7d
ko.zst    3d24e52addf86d3285e3cab6370ace01ce9ed914eb1bc26521ab5d987187956e
```

It retains srcversion `13326073E27DFA035180C56`, exact CPS-v3 vermagic, and was signed with the same CPS-v3 build certificate/key ID as the accepted modules. The private signing key is not stored in this repository.

## Isolated one-shot boot

The candidate was embedded only in a private initramfs and force-loaded before the rollback root module. The kernel and DTB are byte-identical to CPS-v3:

```text
kernel  164bc92d88c724ac4e7872212405c8149cd52a8ee1553d54e11d56581751fc48
DTB     7cd5fdd8ef59c46ca9a3661adacce0444893a6c26fca71c97eaa3070a88aab84
initrd  0badbceb8f4574e1209657cc9483e815f8c860293027f034705eabde9c5faa39
```

GRUB ID was `sp11-audio-wsa-macro-gain0db`. Persistent saved entry remained `sp11-audio-cps-v3` and the one-shot was consumed normally.

Neutral boot gate passed:

```text
kernel                     7.1.5-sp11-cps-v3+
machine-driver srcversion  13326073E27DFA035180C56
WSA-macro srcversion        F681186BB3D24B32621905D
RX0 max/value               84 / 81
RX1 max/value               84 / 81
```

Thus the candidate was behavior-neutral until the explicit 81 -> 84 test.

## Bounded 0 dB tests

### Endpoint 5%

Both macro controls were changed to 84 / 0 dB with no media playing. No speaker-focused kernel fault followed. The exact reference MP3 (SHA-256 `951a65cc63fee17622485c1d94708614005524c7e20f86d3d815327f6bd0e8b3`) then ran for four seconds through the normal Dolby sink.

The read-only WSA observer sampled both amps:

```text
left:  PA=1, current-limit code=17, PA error=0
right: PA=1, current-limit code=17, PA error=0
```

Observer SHA-256: `b70dfe18424ddb950a6079097e88f697c08495d0263bc3b2f7fffdb1b36ea602`.

### Endpoint 12%

The same one-variable test was repeated at the normal 12% endpoint:

```text
SET84      2026-08-15T15:44:58.209386606+01:00
PLAY_START 2026-08-15T15:44:58.228610694+01:00
PLAY_END   2026-08-15T15:45:02.252257049+01:00
```

The playback was intentionally stopped by `timeout` (`gst_rc=124`). No WSA/PA/SoundWire/XRUN speaker fault was recorded.

After each pass the macro controls were restored to 81/-3 dB. Endpoint was restored to 12%. `DRE_CTL_1` / CSR mode was never changed.

## Result and limits

**Operational result:** 0 dB WSA-macro Digital Volume is safe in the current protected CSR-assisted Linux path at both 5% and 12% endpoint levels. The historical -3 dB cap is not required merely to avoid an immediate PA/protection failure on the current stack.

**Parity result:** still AMBER. This does not directly prove Windows runs the WSA macro at 0 dB, and it does not explain or fix the rejected COMP-only/DRE-disabled noise by itself.

The next lower producer variable is the generic WSA-macro `spkr_gain_offset = WSA_MACRO_GAIN_OFFSET_M1P5_DB` policy, which enables four half-dB PGA bits whenever COMP is active. That state is proven on Linux but not yet proven on Windows. It must be isolated separately rather than combined with Digital Volume or DRE/CSR changes.
