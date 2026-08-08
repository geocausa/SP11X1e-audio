# DAX `vlldp-limiter-gain` — public routing and low-level identity

Date: 2026-08-05

## Scope

This proves the public descriptor identity plus generic getter/setter routing for
the open DAX/VLLDP limiter question without changing the live audio path. Later
backend analysis (preserved in
`2026-08-05-VLLDP-POSTGAIN-AND-LIMITER-STATE-CORRECTION.md`) narrows the intended
semantics toward runtime/readback information; generic setter routing alone must
not be promoted to proof that the backend accepts a writable production control.

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

Therefore the **public control layer exposes both generic Get and generic Set
routing** for `vlldp-limiter-gain`. It is not a decorative string. This statement
is deliberately about the front-end routing: later analysis shows `0x2A` grouped
with telemetry/info parameters inside `CDolbyDspVlldp::GetModuleParam`, so backend
semantic writability is not yet proved.

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

## Aug-6 semantic closure

Do not infer a magic production value from the generic public Set route.

Later exact software-VLLDP analysis found a separate live multiband-compressor
telemetry path: `FUN_180021E80` emits a 20-element integer gain vector copied to
`core+0xB60..0xBAC`, alongside a compressor-information matrix. The combined
getter `FUN_18001E868` exports those datasets with values clamped to
`[-2080,0]`; internal production scaling for the 20-vector is `4160.0`.

A complete 2,945-block A/B then changed only `peak-level` from `0` to `-48`.
The separate final VLLDP limiter went from unity to active attenuation, while
the entire 20-element compressor gain vector remained **bit-identical on every
block**. Therefore `mb_compressor_limiter_gain` must not be interpreted as the
nested scalar final-limiter `+0x78` gain or as the missing final-limiter ceiling.

The exact DAX identity/routing remains:

```text
public 0x850 -> internal 0x2A -> mb_compressor_limiter_gain
```

and `0x2A` remains in the special extended readback/info family. There is no
direct cross-DLL pointer edge proving that DAX `0x2A` reads the software APO's
specific `B60` buffer, so the exact public exported field/units should not be
overstated. Backend semantic writability also remains unproved. Neither gap is
a production-parity reason to write `0x850` without source-of-truth Windows
evidence.

See `2026-08-06-MAY-RUNTIME-STATE-AND-VLLDP-TELEMETRY-CLOSURE.md`.

No live Linux Dolby parameter was changed during this analysis.
