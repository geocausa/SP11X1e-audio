# August KDNET bass-runtime recheck — 2026-08-05

## Why this recheck was necessary

The 2026-08-04 YouTube/music KDNET session sounded subjectively very bass-rich.
An earlier summary said the bass/virtual-bass chain "never runs" on the SP11,
primarily from OEM XML tuning and earlier captures. That wording was too broad
for the exact August runtime because the live `DolbyApoVr` bass-enable field was
not explicitly sampled then, and one older modern-DAPVR non-hit used an
unreliable software breakpoint.

This note separates the two different Dolby bass mechanisms and records what is
actually proven for the August internal-speaker graph.

## 1. Modern ASAR spectral harmonic / "fake bass" path

The decoded modern `DolbyAudioProcessing.dll` harmonic dispatcher is
`FUN_180075E80`. Fresh caller tracing in the exact 7.3.7 DLL gives:

```text
FUN_180075E80
 <- FUN_18005D090
 <- FUN_18004A7A0
 <- either:
      FUN_180060CE8 <- FUN_18004E7B0
    or
      FUN_180062448 <- FUN_180061698 <- FUN_18004EA20
```

The parent `FUN_18004E5F0` selects the two branches using DAPVR state `+0xC0`:

```text
render_mode +0xC0 == 0  -> FUN_18004E7B0  DABS / speaker
render_mode +0xC0 != 0  -> FUN_18004EA20  DAHP / headphone-or-alternate
```

This selector meaning is independently preserved in the older static RE
journal (`CDapVRModule::Process` dispatches speaker vs headphone by `+0xC0`).

During the August active stereo/music speaker stream, **hardware execution** at
`FUN_18004E7B0` recorded zero hits while the persistent DAX3/VLLDP150/VR path
was simultaneously hot. Therefore the modern spectral harmonic synthesizer's
**internal-speaker route was not executing in that tested condition**.

An earlier non-hit at `FUN_180061698` must not be used as evidence: the raw KD
log confirms that test used a software `bp`, which is known to produce false
negative user-mode results in this KD setup. This correction does not reopen
the internal-speaker conclusion because `FUN_180061698` belongs to the
nonzero-render-mode/headphone-or-alternate branch, not the DABS speaker branch.

## 2. Persistent DolbyApoVr bass enhancer is a separate mechanism

`DolbyApoVr.dll` has its own ordinary bass-enhancer/extraction controls. Fresh
handler decoding identifies:

```text
bass-enhancer-enable       handler 0x180032A00 -> core+0xC90
bass-enhancer-boost        handler 0x180032A60 -> core+0xC98
bass-enhancer-cutoff       handler 0x180032AD0 -> core+0xCA0
bass-enhancer-width        handler 0x180032B50 -> core+0xCA8
bass-extraction-enable     handler 0x180032BE0 -> core+0xD60
bass-extraction-cutoff     handler 0x180032C70 -> core+0xD68
```

Running Dolby's original constructor/initializer on Linux gives the fresh
state:

```text
bass enhancer enable = 0
boost                 = 192
cutoff                = 200
width                 = 16
bass extraction       = 0
virtual-bass gate     = 0
virtual-bass mode     = 0
```

The current Linux replica does not override those bass fields, so the live
Ubuntu production chain leaves this side path disabled.

### Earlier real Windows control evidence

Multiple June Dynamic DAX RPC captures report:

```text
active profile          Dynamic (5)
bass_enhancer_enable    0
```

while surround/dialog/volume-leveler controls were enabled. The OEM XML also
sets bass enhancer, extraction, virtual bass and sliding bass to zero in all
studied internal-speaker profiles.

### August profile identity

The real Windows Dolby Access log from 2026-08-04 shows the final profile
selection before the later KD period was:

```text
19:58:16  Selected profile: Dynamic
```

No later profile-selection event appears in the retained log; subsequent
activity consists mainly of repeated Dolby off/on experiments. Thus the later
YouTube/KD steady state was very likely Dynamic.

The August KD session did **not**, however, read `GetBassEnhancerEnable` or dump
`DolbyApoVr` core `+0xC90`. Therefore the exact August persistent-VR bass-enable
bit is not directly observed and should not be called hardware-proven OFF.

## 3. Could DAX silently turn Bass Enhancer on for YouTube?

Fresh DAX3API caller analysis shows its internal bass-enable setter
`FUN_1401267B0` is called only by the explicit RPC handler
`FUN_140131900` (`RpcServer::DAXRPC::SetBassEnhancerEnable`). No ordinary
internal/CaptureStreamMonitor caller was found that autonomously invokes this
setter based on content.

This makes a hidden YouTube-only Bass Enhancer toggle unlikely. It is not the
same as proving the August bit value directly.

## 4. Transfer-function diagnostic

As a diagnostic only, an offline copy of the exact current Dynamic chain was
modified to call the original `DolbyApoVr` bass-enable handler. No live plugin
was changed.

Against the old May Windows known-input loopback:

```text
75 Hz step       Windows      Dynamic BE off     Dynamic BE forced on
-30 dBFS         +16.82 dB       +14.34 dB          +23.82 dB
-24 dBFS         +14.76 dB       +15.47 dB          +22.13 dB
-18 dBFS         +13.47 dB       +11.33 dB          +16.34 dB
-12 dBFS         +10.25 dB        +7.84 dB          +10.78 dB
```

Across the selected comparison segments, mean absolute gain error changed from
about `1.73 dB` with Bass Enhancer OFF to about `3.06 dB` with it forced ON.
The old May recording is not an exact August oracle, so this is corroborating
rather than decisive evidence. It does demonstrate that the already-recovered
leveler/regulator/VR dynamics can create large perceived bass without enabling
the separate bass-enhancer switch.

## Revised conclusion

Do **not** say simply "the bass chain never runs".

For the August internal-speaker condition:

- the modern ASAR spectral harmonic / virtual-bass generator is strongly ruled
  out by its speaker call graph plus a hardware-cold DABS speaker entry;
- the persistent `DolbyApoVr` Bass Enhancer is strongly supported as OFF by
  profile/XML/default/June-live/transfer evidence, but its exact August enable
  byte was never sampled;
- the subjectively strong bass is therefore most consistently explained by the
  persistent hot Dolby dynamics already reproduced on Linux: leveler,
  regulator/protection/history and VR profile processing.

## One-line future proof

On the next Windows runtime session, while Dynamic YouTube playback is steady,
record either:

```text
DAX GetBassEnhancerEnable   (opnum 15)
```

or the live persistent `DolbyApoVr` inner-core value at `core+0xC90`.

That single read closes the only remaining historical uncertainty about the
ordinary persistent VR Bass Enhancer. It is separate from the modern ASAR
spectral harmonic generator, whose internal-speaker path is already constrained
by the hardware-cold DABS entry.
