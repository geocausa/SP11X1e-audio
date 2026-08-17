# v28 seek/discontinuity SP7 external physical gate

Date: 2026-08-17  
Status: **objective physical gate passed / user listening verdict still required for L03 closure**

## Purpose

The remaining L03 completion gate after v28 was the physical behavior of warm in-stream seeks. Windows KDNET already proved that seeks do not issue a hidden host `SetVolume`, `GetGraphCkv`, SOFT_PAUSE, POPLESS_EQ or adjacent qcad SET_CFG transaction. Linux already carries the recovered graph calibration and control-link behavior. The unresolved question was therefore whether the full v28 speaker path produces an abnormal physical click/pop on the same deterministic seeks.

## Live stack

The SP11 was one-shot booted into the committed v28 full render-parity candidate:

- kernel `7.1.5-sp11-render-parity-v4+`;
- cmdline `sp11_wsa_dp2_offsetctrl2_v28=1`;
- WSA8845 srcversion `EB74C0F5E4405EEE429136C`;
- SoundWire srcversion `31EA655550AE70F3DF2951E`;
- SoundWire build-id `481d2bae6f96852d33f03d3dbfd6c81caebd5fc6`;
- LPASS WSA macro srcversion `4AF6F542C17BA6DD46586DA`;
- persistent GRUB fallback remained `sp11-audio-cps-v3`.

The v28 kernel, DTB and initrd hashes were revalidated against the committed v28 finding before boot.

## Deterministic seek stimulus

Source:

`/home/geoca/Documents/The White Stripes - Seven Nation Army (Official Music Video).mp3`

SHA-256:

`951a65cc63fee17622485c1d94708614005524c7e20f86d3d815327f6bd0e8b3`

The visible/default Windows-Dolby endpoint was set to 25% and unmuted for the test, then restored to 1% afterward. GStreamer `playbin` was preroll-seeked to 19 s and played continuously. Three `FLUSH|ACCURATE` seeks reproduced the retained Linux/Windows discriminator closely:

- seek 1: `25.752 -> 55 s`;
- seek 2: `58.914 -> 12 s`;
- seek 3: `15.999 -> 90 s`.

Exact SP11 event timestamps:

```text
2026-08-17T20:50:59.666515+01:00 PLAY pos=19.000
2026-08-17T20:51:06.467581+01:00 SEEK1_BEGIN from=25.752 to=55
2026-08-17T20:51:06.669882+01:00 SEEK1_AFTER pos=55.128
2026-08-17T20:51:10.470634+01:00 SEEK2_BEGIN from=58.914 to=12
2026-08-17T20:51:10.673485+01:00 SEEK2_AFTER pos=12.211
2026-08-17T20:51:14.474158+01:00 SEEK3_BEGIN from=15.999 to=90
2026-08-17T20:51:14.677355+01:00 SEEK3_AFTER pos=90.158
2026-08-17T20:51:19.677967+01:00 END pos=95.148
```

## Physical capture provenance

All acoustic evidence here uses the **SP7 microphone externally recording the SP11 speakers**. The SP11 microphone/capture path is not used.

SP7 WAV:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-seek-v28-seven-nation-25pct-20260817\external-mic-20260817-205041.wav`

SHA-256:

`0EBFA4062D2450CAB840EB6C094570FE2CBAF90C9D3798A28E0A97D86D8C3088`

Format/duration:

- 48 kHz;
- 2 channels;
- 33.99 s.

Music onset in the SP7 recording was independently located from the 20 ms RMS envelope at about `18.12 s`, allowing the three seek times to be aligned at about `24.921`, `28.924`, and `32.928 s` in the WAV.

## Transient discriminator

For each seek and each SP7 channel, the maximum absolute first-sample difference inside a +/-20 ms seek window was compared against first-difference percentiles from the surrounding +/-0.75 s of music with the central +/-60 ms excluded.

### Seek 1

- channel 0: peak derivative / local p99.9 = `0.667`; / p99.99 = `0.546`;
- channel 1: `0.824`; / p99.99 = `0.639`.

The seek transition is below the surrounding music's own high-percentile transients.

### Seek 2

- channel 0: `0.0845`; / p99.99 = `0.0637`;
- channel 1: `0.0852`; / p99.99 = `0.0621`.

No abnormal needle transient is present.

### Seek 3

- channel 0: `1.888`; / local p99.99 = `1.003`;
- channel 1: `2.053`; / local p99.99 = `0.991`.

Seek 3 exceeds the local p99.9 derivative by about 2x, but is essentially identical to the local p99.99 music-transient scale. It is therefore not a unique extreme click/pop outlier in the external acoustic waveform.

## Kernel / teardown gate

Across the timed test interval there were zero matching runtime hits for:

- XRUN / underrun / overrun;
- qcom-APM error;
- WSA error/fault;
- SoundWire error/fault;
- PA error / PA_ON_ERR.

After playback:

- physical ALSA PCM returned `closed`;
- endpoint volume was restored to 1%.

## Conclusion

The full v28 stack passes the objective SP7-external acoustic seek gate: none of the three deterministic discontinuities produces a waveform transient distinguishable as an extreme needle/pop beyond surrounding program material, and the lower stack remains fault-free with normal teardown.

This materially strengthens L03 beyond the older structural-only result. The final L03 closure remains the user's direct listening verdict because the historical RED was an audible sharp seek/transition complaint and the ledger explicitly reserves that subjective gate.
