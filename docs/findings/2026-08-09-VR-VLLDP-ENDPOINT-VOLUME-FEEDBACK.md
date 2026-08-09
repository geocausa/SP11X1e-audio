# VR -> VLLDP endpoint-volume feedback closure — 2026-08-09

## Executive result

Correcting the external Dolby sample dependency to `DolbyApoVr ->
DolbyAPOvlldp150` exposed the one major Windows runtime input that the Linux
bridge still lacked: **DAX endpoint-volume feedback into VLLDP postgain**.

Windows DAX derives this value as:

```text
postgain = round(endpoint_volume_dB * 16)
```

with the recovered SP11 range `-1200..0` (`-75..0 dB`).  This is not a static
profile gain; it follows endpoint master volume at runtime.

## Aug-8 state-pinned proof

The Aug-8 Dynamic Windows dumps have:

```text
endpoint UI volume             17 %
VLLDP postgain applied         -423
VLLDP postgain staged          -423
core+0x65C                     0xBE503F04 = -0.2033653855
system-gain                    0
peak-level                     0
```

The fresh Linux bridge previously forced VLLDP postgain to zero.  Every other
named hot-state scalar checked after actual processing matches the frozen
Windows state, including regulator controls, target power, peak/ceiling, and
the runtime state word that advances from cold zero to `2` naturally.

Calling the **original vendor VLLDP postgain setter** followed by the original
apply routine with `-423` produces the exact Windows `core+0x65C` coefficient
bits and moves the corrected VR->VLLDP transfer strongly toward the frozen
Windows downstream result.

## Linux endpoint-volume representation

The live PipeWire Dolby sink was inspected read-only.  At the observed UI
setting:

```text
wpctl displayed volume         0.16
PipeWire channelVolumes        0.003908, 0.003908
```

`0.003908 ~= 0.1575^3`, confirming the WirePlumber UI is cubic.  Therefore the
Dolby feedback must use PipeWire's **raw linear channel gain**, not
`20*log10(wpctl_display_value)`.

For the observed raw gain:

```text
20*log10(0.003908) ~= -48.161 dB
postgain ~= -771
```

Windows and PipeWire UI percentages need not map to the same dB because their
UI volume curves differ.  The portable parity quantity is actual endpoint
attenuation in dB.

## Realtime control implementation

The existing mapped runtime profile-control page was extended backward-
compatibly from 2 to 12 bytes:

```text
offset 0   u8     profile request
offset 1   u8     profile acknowledgement
offset 4   i32    VLLDP postgain request
offset 8   i32    VLLDP postgain acknowledgement
```

Postgain request/ack use `INT32_MIN` as the no-request sentinel and accept only
`-1200..0`.  The audio callback performs only atomic mapped-memory reads/writes
and original Dolby setter/apply calls; it performs no filesystem I/O and does
not rebuild either Dolby engine.

Dedicated regression:

```text
apply_-423 ... coeff=be503f04 ... identity=YES result=PASS
restore_0 ... result=PASS
preinstantiate_postgain_queue=PASS
POSTGAIN_CONTROL_RESULT PASS
```

VR long-memory state is bit-identical across a zero-audio postgain update and
all VLLDP/VR object identities remain unchanged.

An event-driven user helper, `sp11_dolby_volume_sync.py`, subscribes to
`pw-dump -m`, reads raw `channelVolumes`, converts the actual linear attenuation
to dB/postgain, and writes only the postgain request slot.  Unit tests cover
conversion, mute, balance handling, and preservation of existing profile/ack
bytes.  A live dry-run against the SP11 PipeWire session produced:

```text
linear_gain=0.003908 endpoint_db=-48.161 postgain=-771 muted=no
```

## Historical known-input closure

The preserved May-18 deterministic Windows known-input capture previously
appeared to favor the old `VLLDP -> VR` bridge order.  That comparison was
missing endpoint feedback in the corrected chain.

Re-rendering the same source through the corrected `VR -> VLLDP` production
build with the historically recovered May `postgain=-385` changes the result
decisively:

```text
Movie profile
old VLLDP->VR bridge            correlation ~0.965430
new VR->VLLDP, postgain 0       correlation ~0.875686
new VR->VLLDP, postgain -385    correlation  0.999761
                                residual SNR 33.20 dB
                                fitted gain  1.0018
```

Music/Game also reach ~0.998997 correlation and Dynamic ~0.989875, but Movie is
the decisive historical fit.  Thus the old-order advantage was compensation
for the missing VLLDP endpoint-volume feedback, not evidence for reversed
sample dependency.

## Offline production gate

With the runtime postgain control added, the production artifact built from the
corrected source has SHA-256:

```text
a72eb4349b74519dab56b716483763d8bfe70fba49f86b189ddd8859a7c253de
```

Offline gates:

```text
pytest volume-sync tests                   6 passed
PROFILE_LIFECYCLE_RESULT                   PASS
POSTGAIN_CONTROL_RESULT                    PASS
analyseplugin                              PASS
1,000,000-frame chunk determinism          PASS
all chunk patterns                         bit-identical
```

The runtime control page is preserved across filter-chain process restarts so an event-driven volume monitor can queue endpoint attenuation before a new Dolby instance is created; the per-user runtime directory still removes it at logout.

The installed plugin remains untouched at this checkpoint.  Live deployment
must install the corrected plugin and volume-sync helper/service together so
there is no interval where the corrected stage order runs with postgain stuck
at zero.

## Restart-safe control page follow-up

The mapped control page is now deliberately retained across filter-chain
process cleanup within the same user session. The runtime directory still
clears it at logout. This closes a cold-restart race: the volume monitor may
prequeue postgain before a new plugin instance maps the page, and plugin cleanup
will not erase that request.

Regression:

```text
preinstantiate_postgain_queue=PASS
runtime_page_survives_cleanup=PASS
PROFILE_LIFECYCLE_RESULT PASS
POSTGAIN_CONTROL_RESULT PASS
```

Final staged production artifact after this lifecycle fix:

```text
SHA-256 28192b3a90323b05f8fe1609522b3d7b0f53b8710ba1f7db5f34bb1e02368be3
analyseplugin PASS
1,000,000-frame determinism PASS
```

## Cold-start volume-race hardening

A live no-stream restart exposed the normal WirePlumber creation sequence: a
new virtual sink is briefly announced at unity before its persisted volume is
restored. The first deployment saw the monitor log `postgain=0` followed by the
correct `postgain=-771`; no audio was active, but production must not depend on that
race.

Two defenses now close it:

1. With runtime control enabled, a new plugin instance honors any prequeued
   postgain immediately. If no endpoint-volume request is known yet, it starts
   at the recovered SP11 minimum (`-1200`, -75 dB) rather than 0. Offline
   research with runtime control explicitly disabled retains postgain 0.
2. The PipeWire monitor confirms changed node Props after a 200-ms settle
   interval before writing Dolby. A unit test explicitly feeds a transient
   unity event and verifies the settled raw gain `0.003908` is the value used.

Regression additions:

```text
unknown_endpoint_volume_safe_default=PASS
volume-sync tests: 7 passed
PROFILE_LIFECYCLE_RESULT PASS
POSTGAIN_CONTROL_RESULT PASS
1,000,000-frame determinism PASS
```

Final safety-hardened production artifact:

```text
SHA-256 f8c21ddc66af748310caef86f5087f3c991c6de5899cab571906d77375fe9b3b
```
