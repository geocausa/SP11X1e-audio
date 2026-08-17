# SP11 DP2 OffsetCtrl2 closes the CSR-off broadband-static prerequisite

Date: 2026-08-17  
Status: **GREEN causal prerequisite on CPS-v3 / carry forward to full render-parity stack**

## Question

A same-boot one-bit A/B already proved that clearing WSA8845 `DRE_CTL_1.CSR_GAIN_EN` on otherwise exact quiet CPS-v3 raises active digital-zero broadband diff-RMS from about `1.976e-5` to `2.776e-3` (~140x). Native Windows is nevertheless quiet with CSR fallback disabled.

The remaining question was therefore not whether CSR-off is the trigger, but which Windows prerequisite makes the COMP/WSA8845 contract safe with CSR off.

The highest-value transport mismatch was WSA8845 DP2/COMP `OffsetCtrl2`:

- Windows qcaucd oracle: `0x07` on both amplifiers;
- Linux CPS-v3/v27: `0x00` because WSA8845 DP2 is a SIMPLE SoundWire port and Linux did not expose/program SIMPLE `OffsetCtrl2`.

Quiet CPS-v3 also has `0x00`, so the field could not be a standalone noise cause. After the CSR one-bit proof, however, it became a coupled-prerequisite candidate: CSR fallback could mask a malformed/misaligned COMP sideband.

## B1 implementation

B1 changes only the functionality necessary to program DP2 `OffsetCtrl2` from the already-correct qcom master transport parameters:

1. add `SDW_DPN_SIMPLE_TRANSPORT_OFFSETCTRL2` as an optional SIMPLE-port capability;
2. teach `sdw_program_simple_ext()` to write the banked `DPN_OffsetCtrl2` register from `t_params->offset2`;
3. advertise that capability **only on WSA8845 DP2/COMP**.

DP1 BlockCtrl3 and DP3 OffsetCtrl2 remain unchanged in B1 so this experiment isolates DP2.

The live experimental `stream.c` also emitted a bounded proof marker:

`SP11B1 SIMPLE OffsetCtrl2 port=2 val=0x7`

### Exact CPS-v3 SoundWire provenance

A fresh whole-module rebuild was rejected because evolved build metadata did not reproduce the loaded CPS-v3 SoundWire srcversion. Instead B1 was constructed surgically from the preserved Aug-11 CPS-v3 build output:

- original live/preserved `soundwire-bus.ko` srcversion `31EA655550AE70F3DF2951E`;
- original module SHA-256 `85d8f4f9614a7486292dd9ab8a834ba70e017a54dbd93bd4bcd6b37f52fe5c6f`;
- original `stream.o` SHA-256 `999d1925d2c1b902f47e92f5e41a5a31ec6327f50ecfbe7d8cede4eafb7689ec`;
- original ELF build-id `e8ef3048edb3dea77b01d50abbde7c7e1d0157a7`.

Only `stream.o` was recompiled with the B1 change. The exact original CPS-v3 object list, `soundwire-bus.mod.o` and module-common object were relinked around it, then signed with the CPS-v3 build key using SHA-512.

B1 SoundWire:

- SHA-256 `323cd5d218e2e0772f71f1d0f65650416dee0704c6858a62b692d0323153c835`;
- preserved srcversion `31EA655550AE70F3DF2951E`;
- ELF build-id `d9f37e4e1f159172216c7dd86e1f2242c1dfd26e`;
- live sysfs build-id note exactly matched `d9f37e4e...` after boot.

The B1 initrd round-tripped with 4390/4390 entries. Relative to pristine CPS-v3, exactly one existing entry differed: `soundwire-bus.ko.zst`; file mode remained `0664`.

Persistent GRUB fallback remained `sp11-audio-cps-v3`; B1 was one-shot only.

## WSA consumer pair

Two alternate-name WSA modules were built against the same CPS-v3 ABI so stock WSA could remain blacklisted on the B1 boot:

### B1-control

Exact quiet CPS-v3 WSA behavior plus only the DP2 SIMPLE OffsetCtrl2 capability:

- internal name `snd_soc_wsa884x_b1ctrl`;
- srcversion `F1D685E4855D199472982FF`;
- SHA-256 `663f018c75719e9f74bcbb990286ea73ca82fdc1f06578ea268d772fd1fca505`;
- CSR fallback remains enabled on final playback unmute.

### B1-A1

Same DP2 capability plus the already-isolated single consumer change:

- final unmute `DRE_CTL_1.CSR_GAIN_EN: 1 -> 0`;
- internal name `snd_soc_wsa884x_b1a1`;
- srcversion `E682EAB12E27A6FF292EEC9`;
- SHA-256 `6b2670ea445a485fe0794a5870b30440151bd84e430e5e82c039d3fb776c8768`.

## Structural live gate

On B1, both physical amplifiers repeatedly logged:

`SP11B1 SIMPLE OffsetCtrl2 port=2 val=0x7`

including during the measured playback cycles. Physical ALSA PCM was independently observed `RUNNING`.

Thus the experiment does not infer the field from source only; the SoundWire core actually issued Windows `0x07` for DP2 on both slaves.

## SP7 external-mic acoustic results

All captures use the **SP7 microphone externally recording the SP11 speakers**. The SP11 microphone/capture path is not used.

Stimulus:

- visible Windows-Dolby endpoint 1%;
- endpoint muted;
- 48 kHz stereo S16_LE digital zero;
- physical PCM independently observed RUNNING;
- teardown returned PCM closed.

### B1-control: DP2=0x07, CSR ON

WAV SHA-256:

`C81C7F923FDAFF74E54E82AA137E9E8F1B1CC40851595FCB8C1ACBED86929E91`

Median steady diff-RMS:

`2.6375173846883765e-05`

This is room-floor class. Therefore programming DP2 OffsetCtrl2 does not itself create a new acoustic defect.

### B1-A1 first run: DP2=0x07, CSR OFF

WAV SHA-256:

`85B844D3D9C4A0D8C818BFCE5F5585A9D97A0D6A4648B8CDCE805B9D670D9D42`

Median steady diff-RMS:

`1.9225640338104176e-05`

This is only ~1.05x the retained Windows active reference and is **144.38x quieter** than the matched CPS-v3 CSR-off A1 with DP2 OffsetCtrl2 left at Linux `0x00`.

The 0.5 s active bins remain around the room floor instead of showing the A1 playback-onset jump to `~2.6e-3 .. 3.0e-3`.

### B1-A1 repeat after idle/clock-stop

After the first B1-A1 cycle:

- PCM returned closed;
- WSA macro, VA macro and SoundWire all runtime-suspended;
- no WSA/SoundWire/XRUN fault evidence was found;
- the machine remained idle for 20 s before a second wake.

Repeat WAV SHA-256:

`415E635A6CF220BB972EC5218C77A039EA3B8BCA080BEA494A6A9171DD536663`

Median steady diff-RMS:

`2.5808327779179254e-05`

A few isolated room impulses lift the aggregate, but stable 0.5 s bins remain overwhelmingly around `1.7e-5 .. 2.3e-5`; there is no sustained PA broadband floor. This is still **107.55x quieter** than the matched generic-DP2 CSR-off A1.

## Causal conclusion

The matched matrix is now:

- CPS-v3 producer + DP2 Offset2=`0x00` + CSR ON -> quiet;
- CPS-v3 producer + DP2 Offset2=`0x00` + CSR OFF -> broadband-noisy (`2.776e-3`);
- CPS-v3 producer + DP2 Offset2=`0x07` + CSR ON -> quiet (`2.638e-5`);
- CPS-v3 producer + DP2 Offset2=`0x07` + CSR OFF -> quiet (`1.923e-5`, repeat `2.581e-5`).

Therefore Windows WSA8845 DP2/COMP `OffsetCtrl2=0x07` is the missing prerequisite that makes CSR fallback-off operation quiet in this CPS-v3 causal experiment.

The earlier statement “quiet CPS-v3 has the same DP2 mismatch, therefore DP2 is innocent” was incomplete: CPS-v3 keeps CSR fallback enabled, which masks the bad COMP-side transport contract. The mismatch becomes acoustically decisive only when the Windows consumer policy disables CSR fallback.

## Next step

Carry **only this proven DP2 prerequisite** onto the full v27 Windows-WSA lifecycle as v28. Do not simultaneously fix DP1 BlockCtrl3 or DP3 OffsetCtrl2 yet. If v27 + DP2 OffsetCtrl2 becomes room-floor quiet, the broadband-static root cause is closed on the intended Windows-lifecycle stack and the remaining transport mismatches can return to structural-fidelity work rather than noise debugging.
