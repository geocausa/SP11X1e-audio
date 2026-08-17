# W02: WASAPI loopback is a dedicated audiodg branch, not the speaker-device stream — 2026-08-17

## Decision

The historical ~59.8 dB Windows-vs-Linux W02 residual must no longer be treated as a built-in-speaker completion defect.

Fresh raw-loopback capture plus the already preserved state-pinned `audiodg` dump proves that the WASAPI loopback oracle is delivered through a dedicated `audiodg` cross-process branch that is structurally distinct from the real speaker/device branch.

W02 remains useful as a **loopback-branch identity research item**, but it is no longer a blocker for physical built-in-speaker parity.

This finding does **not** claim that every sample of the complete h60 device stream has been captured and compared. It does prove direct Windows/Linux bit identity through the reproduced speaker DSP chain at the loud 75-Hz region where the old W02 residual is strongest, and it proves that the saved WASAPI loopback samples come from another branch.

## Raw WASAPI oracle

A fresh Windows visit reproduced the exact Aug-12 endpoint state and used an `IAudioClient` loopback recorder that preserves packet bytes before any WAV conversion.

Directory:

`C:\Users\Geoca\Documents\SP11-Audio-Audit-20260812\w02-rawmix-20260817`

Raw stream:

- file: `rawmix.bin`
- SHA-256: `08D1211EE4518FCEA18434382A13C31AB6E5D102D6E5F0EA4154B765E1EFF46A`
- bytes: `13,824,000`
- duration: exactly `36.000 s`

`IAudioClient::GetMixFormat` returned:

- `WAVE_FORMAT_EXTENSIBLE` (`0xfffe`)
- stereo
- 48 kHz
- 32 valid bits / 32 container bits
- block alignment 8
- channel mask `0x3`
- subtype `{00000003-0000-0010-8000-00aa00389b71}` = IEEE float32

The raw stream aligns to the deterministic source at `90112` frames (`1.877333... s`).

Raw float Windows versus the canonical Linux Movie replay reproduces the old W02 residual directly:

- fitted scale `1.00018904894`
- correlation `0.999999473405`
- residual SNR `59.77457 dB`

Quantizing this raw float stream with the old PowerShell recorder's clamp + `Math.Round(v*32767)` rule reproduces the retained Aug-12 PCM16 clean oracle **sample-for-sample**. Therefore W02 is real in the Windows WASAPI float tap and is not introduced by WAV writing.

## Raw loopback bytes are frozen in the existing audiodg dump

At the deterministic loud-75-Hz trajectory, a raw-loopback block 20 ms before the captured speaker-limiter block appears byte-for-byte in the existing full-memory dump.

Exactly `432` consecutive stereo float32 frames match at three locations:

- `0x1fe3bb70200`
- `0x1fe3bb74ac0`
- `0x1fe3bc56880`

The second address is exactly `h44` buffer `0x1fe3bb74940 + 48 frames`.

The third address belongs to a shared-memory audio ring at:

`0x1fe3bc50000 .. 0x1fe3bc5b300`

and starts 48 frames into the ring buffer described at `0x1fe3bc56700`.

This is not a spectral or correlation inference: the 432-frame float payload is byte-identical.

## h43 -> h44 is the loopback-side Int16 -> Float32 converter

The fresh graph contains an `AudioFormatConvert` edge:

`h43 -> h44`

with:

- h43: stereo Int16, 48 kHz;
- h44: stereo Float32, 48 kHz;
- live callback: `CAudioFormatConvert::ConvertInt16ToFloat32_NEON`.

The h44 values are exactly on the `1/32768` lattice. The 432-frame raw WASAPI block is exactly `h44[48:480]`.

The consumed h43 Int16 input had already been cleared in the frozen callback state, so no stale h43 source block is promoted into an unsupported claim.

## h42 terminates in audiodg's cross-process WASAPI delivery endpoint

The loopback-side graph after h44 is:

`h44 -> CAudioVolume -> h39 -> AudioMeter -> h40 -> AudioConstrictor -> h41 -> AudioCleanup -> h42`

The `CConnectionNode` object for h42 at `0x1fe39d707c0` contains:

- handle `42`;
- PCM buffer `0x1fe3bb720c0`;
- 528 frames;
- shared-ring descriptor reference at connection `+0x68`;
- endpoint pointer at connection `+0x70` = `0x1fe39d72f40`.

The endpoint at `0x1fe39d72f40` resolves through the matching public `AudioDG.pdb` to:

`CCrossProcessServerOutputEndpoint<StaticControlData_V1, VolatileControlData_V0, ControlData_V1>`

Its interface vtables are:

- `IAudioEndpoint` at audiodg RVA `0x1518b0`;
- `IAudioEndpointRT` at audiodg RVA `0x151840`.

The same object owns `CCrossProcessServerMemory` at `0x1fe3a6fea60`, and that helper points directly to shared-memory base `0x1fe3bc50000` containing the third byte-identical raw-loopback copy.

Thus h42 is the `audiodg -> WASAPI client` audio delivery connection.

## Speaker/device branch is structurally different

The real speaker branch in the same dump is:

`h49 -> VirtualSurround -> h50 -> Dolby SFX -> h51 -> Meter -> h52 -> Volume -> h53 -> Constrictor -> h54 -> mixers -> h55 -> ASAR -> h56 -> Dolby MFX/VLLDP -> h57 -> Surface MFX copy -> h58 -> AudioLimiter -> h59 -> AudioFormatConvert -> h60`

The h59 -> h60 terminal converter is:

- input: stereo Float32 48 kHz;
- output: stereo Int16 48 kHz;
- callback: `CAudioFormatConvert::ConvertFloat32ToInt16Dither_NEON`.

The h60 connection terminates in an endpoint object implemented by `AUDIOKSE.dll`, not the `audiodg` `CCrossProcessServerOutputEndpoint` used by h42.

Current AUDIOKSE module in the dump:

- base `0x7ff846ab0000`;
- h60 endpoint vtable runtime `0x7ff8474469a8` = AUDIOKSE RVA `0x969a8`.

The public symbol server does not publish the matching private AUDIOKSE PDB, so the exact internal C++ endpoint class name is intentionally not guessed. Module ownership and the distinct endpoint object are directly observed.

The h43 connection also has an AUDIOKSE-backed endpoint object (runtime vtable `0x7ff847446c80`, RVA `0x96c80`), consistent with the loopback branch having its own kernel-stream-side source plumbing. Its exact private class name is likewise left unasserted.

## Direct speaker DSP identity at the W02-worst region

From the same fresh state-pinned trajectory, Windows and Linux are already proven bit-exact at matching fill/phase for:

- VR input;
- VR output;
- VLLDP input;
- VLLDP output;
- Surface MFX disabled-copy behavior;
- a captured 480-frame AudioLimiter input block;
- the corresponding 480-frame AudioLimiter output block.

The limiter comparison has float max difference `0` and RMSE `0`.

This proof was taken in the loud 75-Hz region where the full-file WASAPI W02 residual is strongest.

## audiodg loopback architecture agrees with the live structure

The matching public `AudioDG.pdb` exposes explicit loopback graph machinery, including:

- `CAudioDeviceGraph::AddPipeToLoopbackConnection`;
- `CSystemAudioDeviceSharedBase::AddPipeToLoopbackConnection`;
- `CSubmixImpl::AddPipeToLoopbackConnection`;
- `CPipeInstance::GetLoopbackConnection(bool)`;
- `CPipeInstance::GetSecondaryLoopbackConnection()`;
- `CPipeInstance::InitializeLoopbackConstrictorInterface`;
- `CAudioDeviceGraph::UpdateLoopbackConstrictionLevel`;
- literal graph labels `Loopback` and `PostVolumeLoopback`.

`CPipeInstance::GetLoopbackConnection(bool)` selects between two concrete internal GUID-keyed connection variants. Their private semantic GUID names are not published, so this finding does not guess which numeric GUID corresponds to which textual label.

## Ledger consequence

The previous W02 formulation conflated two questions:

1. does the Linux built-in-speaker render chain reproduce the Windows speaker DSP path?;
2. does Linux reproduce the separate Windows WASAPI loopback client branch bit-for-bit?

Fresh evidence separates them.

Question 1 now has direct stage-level identity evidence through AudioLimiter at the worst residual region plus the independent v28 physical static and seek gates.

Question 2 remains an interesting Windows-engine reverse-engineering problem, but it is not the signal path heard by the physical speaker and must not block built-in-speaker parity.

W02 therefore remains AMBER only as a **loopback-specific identity research item** and is removed from the built-in-speaker completion-gate list.
