# DAX `vlldp-limiter-gain` — bidirectional Get/Set mapping proved

Date: 2026-08-05

## Scope

This closes the control-vs-readback half of the open DAX/VLLDP limiter question
without changing the live audio path.

Exact SP11 control DLL:

```text
Dax3DapControl.dll
SHA-256 5e7844082404b5e66618121af847c80122d950223b5bd225e0a6e763770c5207
PE32+ ARM64
```

## Public DAX descriptor

`FUN_180008868` constructs the public DSP-parameter descriptor map. The exact
sequence around the limiter is:

```text
vlldp-mb-compressor-tuning-info  public ID 0x84F  internal index 0x29
vlldp-limiter-gain               public ID 0x850  internal index 0x2A
vlldp_sliding_bass_enable        public ID 0x851  internal index 0x2E
```

For `vlldp-limiter-gain`, the constructor builds the descriptor and then inserts
it into global map `DAT_18007dc98`.

## Setter and getter proof

The exported API RVAs are:

```text
GetDapParam         0x148A0
SetDapParam         0x13600
SetDapVariantParam  0x138A0
```

`SetDapParam` routes ordinary DSP parameters to `SetDapVariantParam`.
`SetDapVariantParam`:

1. looks up the requested public parameter ID in `DAT_18007dc98`;
2. rejects IDs absent from that map with `DSP module does NOT support this parameter`;
3. for an ordinary integer variant, dispatches through `FUN_180018DD8` using
   the descriptor's internal index;
4. reports success/failure as `DspController::SetParameter`.

Because the `0x850` descriptor is explicitly inserted into that exact map, an
integer `vlldp-limiter-gain` request follows the generic setter path and passes
internal index `0x2A` to the low-level DSP controller.

The generic getter path in `FUN_18001B4A0` performs the same `DAT_18007dc98`
lookup, rejects unsupported IDs, and then calls the low-level getter
`FUN_1800187D0` with the descriptor internal index. Its diagnostic strings
identify this path as `DspController::GetParameter`.

Therefore:

**`vlldp-limiter-gain` is definitely bidirectional Get/Set capable.**

It is neither a decorative name nor a status-only/setter-only API surface in
`Dax3DapControl.dll`.

## Low-level identity

The same exact DLL contains the low-level VLLDP parameter name:

```text
mb_compressor_limiter_gain
```

The low-level name table begins `system_gain` at internal index `0x14`; the
public map independently confirms `vlldp-system-gain -> 0x14` and
`vlldp-post-gain -> 0x15`. In that sequential low-level table,
`mb_compressor_limiter_gain` is entry 22 after `system_gain`, giving
`0x14 + 0x16 = 0x2A` exactly.

So the public and low-level identities close directly:

```text
vlldp-limiter-gain  public ID 0x850
                    internal index 0x2A
                    low-level name mb_compressor_limiter_gain
```

## What is NOT yet proved

Do not infer a magic production value from this result.

Still unresolved:

- the exact units/range/meaning of the value;
- whether the returned live value is a static configured gain, evolving
  runtime state, or a value with mixed semantics;
- the live Windows value under a state-pinned Music run;
- how that live value correlates with the VLLDP compressor/limiter state and
  the remaining 75-Hz parity residual.

No live Linux Dolby parameter was changed during this analysis.
