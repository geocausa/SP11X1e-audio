# Windows endpoint taper parity — 2026-08-12

## Windows reference

Fresh live `IAudioEndpointVolume` measurement of SP11 built-in endpoint
`{0.0.0.00000000}.{5bb689e6-2c6b-4357-b4c1-beb815638f88}` captured 201 scalar
points from 0.000 through 1.000 in increments of 0.005. The original endpoint
state was scalar `0.1000000164`, `-34.0460205 dB`, unmuted and was restored
exactly after capture.

Endpoint metadata:

- dB range: `-75 .. 0 dB`
- reported hardware increment: `0.5 dB`
- software volume steps: `51`
- hardware support flags: `0x00000007`
- original Windows CSV SHA256: `cbaa8bf2149becf82d6eeac2613ba1acb3d7244fe101ff82070b15f591a471fe`
- normalized SP7 mirror SHA256: `c4a0e0d93ffc40765ea2c9c861ac201ad60be0620f2675a1879608b48a143faf`

Reference scalar points include 10% `-34.0460205 dB`, 25%
`-20.7474098 dB`, 50% `-10.4270468 dB`, and 100% `0 dB`. The curve is not a
stable power law, especially at low volume, so production pins the measured
SP11 curve rather than fitting an exponent.

## Linux implementation

The default virtual Dolby sink remains the user-facing volume control. Its raw
PipeWire `channelVolumes` are the cube of the visible WirePlumber scalar. The
volume-sync service now:

1. recovers the visible scalar by cube root;
2. maps it through the pinned Windows scalar->dB curve;
3. writes `round(endpoint_dB * 16)` to VLLDP postgain;
4. independently sets the hidden downstream ALSA sink so its actual soft gain
   equals `10^(endpoint_dB/20)`; and
5. lets `sp11-msiir-volume-sync` consume the same postgain state for Windows CKV
   selection.

This keeps endpoint attenuation after Dolby/AudioEngine while preserving the
visible slider value.

## Live validation before persistence reboot

All tests were performed under a silent protected graph unless otherwise noted.

- 19/19 volume/MSIIR unit tests passed.
- 10% -> hardware `-34.0457 dB`, postgain `-545`, CKV1.
- 25% -> hardware `-20.7474 dB`, postgain `-332`, CKV2.
- 50% -> hardware `-10.4273 dB`, postgain `-167`, CKV16.
- 100% -> hardware `0 dB`, postgain `0`, CKV30.
- visible 25% remained 25% while hidden hardware scalar became ~0.451035.
- mute drove VLLDP to -75 dB/CKV1; a -60 dBFS 997-Hz test measured digital zero
  at the hardware monitor while muted, then restored 25% unmuted.
- filter-chain destruction/recreation preserved visible 25%, restored hidden
  hardware `-20.7474 dB`, and first playback acknowledged postgain -332 and CKV2.

The persistence gate exposed two startup timing cases before sign-off. On cold
boot, WirePlumber could restore the hidden hardware gain before the virtual
filter node existed, and `pw-dump -m` did not guarantee a complete initial Props
replay. A one-shot 200-ms snapshot was also too early on one boot. The daemon
now subscribes first, then retries complete snapshots for up to five seconds
until both virtual and hardware nodes exist; steady-state remains event-driven.
A service-order simulation that stopped filter-chain + volume-sync, deleted the
control page and started both together recreated the page at postgain -332 and
restored the exact 25% Windows attenuation without playback. The dedicated bootstrap regression is included in the volume/MSIIR suite. A
later persistence boot also showed `pw-dump -m` replaying an old transient-unity
Props event after the settled snapshot (`25% -> 100% -> 25%`) while idle. A
one-second bootstrap guard now resolves queued volume deltas against a fresh
full snapshot, preserving genuine user changes while suppressing stale replay.
The guarded service-order simulation emits only the settled 25% state. The
combined suite has 22 passing tests. The final full reboot passed: before any
playback the shared control page existed at request -332, visible volume was 25%,
and the hidden endpoint was -20.7474 dB with no transient-unity rewrite. First
playback then produced Dolby request/ack -332 and automatic MSIIR CKV2. V01/V02
are therefore GREEN in the canonical render ledger.
