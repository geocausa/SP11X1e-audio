# CPS-v3 CSR enable one-bit causal boundary

Date: 2026-08-17  
Status: **GREEN causal discriminator / root prerequisite still open**

## Question

Does the shipping-Windows WSA8845 `DRE_CTL_1.CSR_GAIN_EN=0` state itself expose the Linux broadband static, or was the earlier noise caused by unrelated render-parity changes?

## Exact baseline

The accepted quiet baseline is the production CPS-v3 boot:

- kernel `7.1.5-sp11-cps-v3+`;
- stock WSA8845 srcversion `E084BC31719EE85BB8DEABD`;
- exact retained WSA source SHA-256 `1f3a72c33c97b41c760784cea60e78669f2de8fcb704957c8e348b4fe49e64b5`;
- persistent GRUB fallback remains `sp11-audio-cps-v3`.

A fresh 2026-08-17 SP7-external-mic control on normal CPS-v3 measured active-zero diff-RMS about `4.679263132194288e-05`; historical CPS-v3 is `1.8615030398551657e-05` and the Windows active reference is `1.8253227918889202e-05`.

## A1: one authored behavior change

Candidate A1 changes only the final playback-unmute write:

```diff
 snd_soc_component_write_field(component, WSA884X_DRE_CTL_1,
                                WSA884X_DRE_CTL_1_CSR_GAIN_EN_MASK,
-                               0x1);
+                               0x0);
```

The retained A1 source differs from baseline only by that value and its explanatory comment. The rebuilt candidate was loaded under a temporary alternate module container name to avoid the boot-time stock-module blacklist; driver behavior and device binding were unchanged.

A1 module:

- internal name `snd_soc_wsa884x_a1`;
- srcversion `F79155E08F639AF76F72039`;
- vermagic `7.1.5-sp11-cps-v3+`;
- SHA-256 `1feeef1c9193c5f4840552616d7b60a0cb92dbe8754ec225829d715a0fe99ab4`;
- signed with the CPS-v3 build key using SHA-512.

Both physical SoundWire WSA8845 devices bound to the usual `wsa884x-codec` driver. ALSA card0 and playback PCM appeared normally.

## Same-boot acoustic result

Stimulus for both A1 and its control:

- SP7 microphone externally recording SP11 speakers;
- SP11 microphone path not used;
- visible Windows-Dolby sink 1%;
- visible sink muted;
- exact stereo S16_LE 48 kHz digital zero;
- physical ALSA PCM independently observed `RUNNING`;
- conservative steady scoring window 14--23 s of the 30 s capture.

### A1 / CSR fallback disabled

SP7 WAV:

- `external-mic-20260817-192948.wav`;
- SHA-256 `2EE49C3F00DBD9640705F35FE5F6BEC0764E28DDF46204D6977E980E09E05391`.

Median steady diff-RMS:

`0.0027757949639826114`

The 0.5 s bins jump from roughly `3e-5 .. 8e-5` before playback to roughly `2.6e-3 .. 3.0e-3` at playback onset and stay there during the active window.

### Same-harness exact-source control / CSR fallback enabled

To exclude late card registration, manual module load, and the boot harness as causes, A1 was unbound/removed on the same boot. A second alternate-name module was built from the **byte-identical CPS-v3 source** (`1f3a72...`) and loaded through the same late-bind path.

Control module:

- internal name `snd_soc_wsa884x_ctrl`;
- srcversion `B7F5D7D97DD31C77EFB6F01`;
- SHA-256 `b6fc6126d7eefa711c03ff73cd713db1736c345f1eb987ff9a2678a3ebeab55a`;
- source SHA-256 exactly matches CPS-v3 baseline.

SP7 WAV:

- `external-mic-20260817-193301.wav`;
- SHA-256 `47F15A21BCD9044F5141D901A31866CB3B82D86F04867BC0B1AB7BA571D805EE`.

Median steady diff-RMS:

`1.9755842393131636e-05`

This is ~1.08x the retained Windows active reference and ~1.06x the historical CPS-v3 quiet reference.

## Causal result

Same boot, same kernel, same DTB, same userspace/Dolby graph, same late-load/hotplug mechanics, same SP7 recorder and same zero stimulus give:

- CSR enabled: `1.9755842393131636e-05`;
- CSR disabled: `0.0027757949639826114`.

That is about **140.5x** more active broadband diff-RMS when only `CSR_GAIN_EN` is cleared.

Therefore `DRE_CTL_1.CSR_GAIN_EN=0` is a genuine causal switch that exposes the Linux static. This does **not** mean Windows is wrong to keep CSR disabled; native Windows is quiet with CSR disabled. It means Linux is missing a prerequisite that Windows satisfies.

## Updated search boundary

DP2/COMP presence and scheduling, producer-before-PA ordering, and the VA-macro wake are not sufficient explanations: quiet CPS-v3 shares them. The next question is now narrowly:

> What COMP/WSA-macro/consumer prerequisite makes CSR-off safe on Windows?

Existing evidence is consistent with this coupling: the old Windows-producer/no-HD2 CSR-off v5 state measured about `6.7653e-4`, roughly 4.1x quieter than A1 but still far above Windows. The next discriminator should transplant the complete retained Windows-producer/no-HD2 macro state onto CPS-v3 while holding CSR off, then isolate whichever producer/consumer sub-block further suppresses the floor.
