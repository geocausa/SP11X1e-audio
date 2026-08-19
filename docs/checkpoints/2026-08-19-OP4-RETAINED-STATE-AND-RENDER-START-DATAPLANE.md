# SP11 Windows op4 retained-state and render-start data-plane checkpoint

Date: 2026-08-19 (Europe/London)
Branch: `agent/psycho-bass-20260818`
Status: new Windows-oracle checkpoint; Golden v31 remains protected

## Scope

This checkpoint records all material findings obtained after commit `28334d682fa86dc5e0696dd2ecac64a81509edd9` (`audio: checkpoint WSA controller and FIFO boundary`). It supersedes speculative follow-up notes around qcaudminiport op8 / WSA ADC-state programming and preserves the narrowed Windows render-start boundary before further KD work.

The decisive unresolved discrepancy remains:

- Native Windows DATA_LOGGING tap2 carries real 8 kHz VI PCM.
- Native Windows DATA_LOGGING tap3 carries real 24 kHz CPS PCM.
- Corrected Golden Linux can packetize the corresponding tap2/tap3 streams but payload remains all-zero during an acoustically proven render.

Do not redo hypotheses already closed below without new evidence.

## Safety / baseline

Golden v31 remains the protected Linux baseline.

- Persistent GRUB id: `sp11-audio-golden-v31`
- Golden topology SHA-256: `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`
- Golden module srcversions:
  - `snd_q6apm`: `687B16CF9C43B43E90C0746`
  - `q6apm_dai`: `870A8676B068C98323A4B10`
  - `snd_soc_x1e80100`: `13326073E27DFA035180C56`

Before the current Windows hop, Golden was verified clean and the canonical topology hash matched. The two newly discovered Windows op4 physical register values described below were also verified to already be present on Golden Linux, so no candidate was built for them.

## 1. qcaudminiport op8 / WSA ADC-state path is not the missing per-render feed activation

Static reconstruction found a previously unmodeled qcaudminiport path after hardware-device creation:

- qcaudminiport creates/configures a 64-byte hardware descriptor through qcaucd indirect operation 4.
- if object state at `+0x108` is dirty/valid, qcaudminiport may call `FUN_14006ceb8`, which translates to qcaucd operation 8.
- qcaucd op8 dispatches internal command `0x202`, event type `0x4a`.
- event `0x4a` programs WSA8845 analog ADC-family registers around `0x300d`-`0x3011`.
- event `0x4b` also touches `0x300f` / `0x3011` and related sensing state.

Linux WSA8845 headers identify this region as the analog BG/TSADC / ADC programming block (`ADC_PROG`, `ADC_IREF_CTL`, `ADC_ISENS_CTL`, `ADC_CLK_CTL`).

A live KD render test armed before normal speaker playback with:

- qcaudminiport op8 breakpoint at `qcaudminiport8380+0x6ceb8`
- qcaucd writes filtered to `0x300e`-`0x3011`

During a controlled ~10 s 997 Hz speaker render, neither op8 nor the ADC-register writes fired.

Later, a trustworthy protected-speaker lifecycle capture showed the real sequence is:

- op4 create/configure
- protected render
- op5 teardown

with **no op8 invocation** in that lifecycle.

Disposition: qcaudminiport op8 / qcaucd `0x202` ADC-state path is a configuration/analog-state mechanism, not the missing VI/CPS per-render sample attachment. Keep closed unless new direct evidence appears.

## 2. Windows really does establish reusable state; ordinary renders do not necessarily reissue setup writes

A controlled PnP-reload experiment was performed with the live qcaucd/qcaudminiport stack. Breakpoint rebasing issues were corrected by catching module load events and planting relocation-correct absolute addresses before initialization continued.

After the reload, a proper shared-mode WASAPI render of a 997 Hz tone was acoustically verified using the external microphone. The acoustic signal was strong (about +62.2 dB / +52.7 dB above pre-tone baseline in the measurement used for this discriminator), while the render itself did **not** reissue:

- qcaudminiport hardware-interface trampoline `+0x6d9b0`
- the previously observed WSA controller writes `0x105c` / `0x1d54`

Therefore Windows can render through already-established hardware/controller state instead of reconstructing all setup on every playback call.

This validated the user's retained-state hypothesis as a real behavior, while later timing work (section 6) shows that the **missing VI/CPS sample-feed activation itself is still render-start coupled**, not simply a boot-only action.

## 3. Trustworthy op4 protected-speaker create capture

A clean restart/load-event capture obtained fresh module bases before initialization and trapped the first protected-speaker op4 create transaction.

The op4 path uses the qcaudminiport 64-byte hardware descriptor and reaches qcaucd internal command `0x200` (device configure/create). The corresponding real Windows lifecycle includes the known WSA controller/dataport programming.

Important static/runtime closure:

- qcaudminiport `+0x6d8d0` is a thin hardware-interface trampoline into qcaucd.
- the previously observed live stack entry at `qcaudminiport+0x6d9b0` is inside that trampoline path; the decompiled SP7 qcaudminiport image is byte-for-byte identical to SP11 Windows.
- qcaudminiport binary SHA-256: `79B26804D05332304C736C4E03E942DB6A07EA886A2B07F3A4FF5947D1D05531`
- qcaudminiport version: `1.0.0.10545`
- qcaucd hash remains `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`

The op4 `0x200` second-stage function (`FUN_14002c158`) contains a callback/object-list walk, but a live probe proved the callback list is **empty for this class-1 speaker-device path**. There is no hidden second callback performing an LPASS/AFE attach operation there.

## 4. Full op4 physical-MMIO expansion found only two non-WSA writes, both already matched by Golden Linux

A broad qcaucd physical-write trace was run for the real op4 create transaction, excluding the already studied WSA `0x06b1xxxx` controller window.

Only two non-WSA physical writes were observed:

1. `0x0725a000 = 0x00050000`
   - live return owner: `qcaucd +0x3ab94`
   - static owner: `FUN_14003aaa0`
   - interpreted as pin/pad slew-type setup, not a hidden sample client

2. `0x06b6c0b0 = 0x00000001`
   - live return owner: `qcaucd +0x3bce4`
   - static owner: `FUN_14003b9b0`
   - part of LPASS Audio / WSA SoundWire controller clock/reset bring-up

Golden Linux was then inspected live and both corresponding values were already present:

- `0x06b6c0b0 = 1`
- `0x0725a000 = 0x50000`

Disposition: these are genuine Windows op4 physical-side actions but **not the missing feed gate**. No disposable Linux candidate should be built merely to reproduce them.

This materially closes the remaining obvious HLOS physical-register gap inside qcaucd op4.

## 5. audiodg / SurfaceAPO / Qualcomm ProxyAPO investigation

The user suggested that `audiodg.exe` might participate in the missing mechanism. This was investigated directly rather than dismissed.

### Existing lifecycle evidence

The protection graph remains resident across ordinary playback and `audiodg` recycle. Restarting `audiodg` does **not** reconstruct the qcadcm protection graph. Restarting the ADCM/qcadcm PnP device does tear down and rebuild the protected graph.

Thus audiodg is not the owner of graph birth.

### SurfaceAPO

Live audiodg hosts include Dolby APOs, `SurfaceAPO.dll`, and Microsoft virtual-surround components.

SurfaceAPO contains generic hardware-proxy-capable code, including strings such as `Query audio module for PP configuration` / `ProxyManagerEFXImpl`. However the live SP11 speaker endpoint configuration does not enable that hardware-proxy path:

- endpoint provides `SurfaceAPO_0D.json`
- no live `PKEY_SurfaceApoEfxProxyNames` value was found
- active JSON contains software MFX EQ / volume-dependent processing
- keyword counts in that JSON were zero for `Proxy`, `DSP`, `Hardware`, `Communicator`, `AudioStream`, `Protection`, `Feedback`

Disposition: SurfaceAPO is not establishing the VI/CPS feedback transport on this SP11 configuration.

### Qualcomm Audio Proxy APO

The speaker endpoint also references Qualcomm's componentized APO:

- device: `Qualcomm Audio Proxy APO`
- INF: `qcaudminiport_apo8380.inf`
- driver version: `1.0.4281.8500`
- binary: `ProxyAPO.dll`
- ProxyAPO SHA-256: `B8B7CB0D3A7D671B7F84722C0EE533F7A4FEE73B9636C06C4EAA2F36334591FC`
- binary file version: `1.0.0.10545`
- endpoint-effect CLSID: `{697DA8EE-64F9-4096-BEBC-7C1C42CC2876}`
- mode-effect CLSID: `{7B51F67F-7354-481D-8D44-DE3E3818839A}`

Static PE import inspection is decisive: `ProxyAPO.dll` imports COM/audioeng, registry and PnP/configuration APIs, but **no driver-I/O path** (`CreateFile`, `DeviceIoControl`, KS I/O, GPR/AFE/SoundWire calls are absent).

Disposition: Qualcomm ProxyAPO is an effects/configuration proxy, not the missing VI/CPS hardware sample-feed owner.

Overall audiodg disposition: audiodg/APOs can be an upstream stream-start/configuration trigger, but neither SurfaceAPO nor Qualcomm ProxyAPO contains the missing low-level feedback attach action.

## 6. Clean synchronized LPASS data-plane timing after qcadcm graph rebuild

Fresh Windows evidence directory:

`C:\Users\Geoca\Documents\SP11-Audio-Audit-20260812\windows-protection-deconstruct-20260819\graph-birth-dataplane-20260819-222134-clean`

Collector: copied from the validated Windows LPASS `0x1586` collector, run in an isolated evidence directory so previous oracle files were not overwritten.

Exact UTC markers:

- qcadcm/ADCM restart begin: `2026-08-19T21:22:15.9809750Z`
- qcadcm/ADCM restart end: `2026-08-19T21:22:16.6362847Z`
- WASAPI invocation begin: `2026-08-19T21:22:19.6403190Z`
- WASAPI engine READY / stream start marker: `2026-08-19T21:22:20.3894037Z`
- WASAPI invocation end: `2026-08-19T21:22:32.5307550Z`

The renderer is the known shared-mode default-multimedia WASAPI oracle, 48 kHz stereo, with the 997 Hz tone deliberately delayed ~3 seconds after stream start.

Parsed DATA_LOGGING timing:

### CPS tap3

- first frame: `2026-08-19T21:22:20.625022Z`
- rate: 24 kHz
- payload: 1920 bytes
- first frame already nonzero: 1440 nonzero bytes
- delta from WASAPI READY: **+235.619 ms**

### VI tap2

- first frame: `2026-08-19T21:22:21.130783Z`
- rate: 8 kHz
- payload: 640 bytes
- first frame already nonzero: 320 nonzero bytes
- delta from WASAPI READY: **+741.380 ms**

### Render tap1

- first packet after READY: `2026-08-19T21:22:20.436997Z`, initially zero (the renderer is still in its deliberate silence pre-roll)
- first nonzero render PCM: `2026-08-19T21:22:24.325292Z`
- delta from WASAPI READY: **+3935.889 ms**

Interpretation:

Windows activates physical CPS/VI feedback transport immediately after the speaker render stream starts, **while the render PCM is still digital silence**. Therefore VI/CPS activation is not waiting for acoustic energy, volume-dependent speaker protection, or the 997 Hz tone itself.

Graph existence alone is insufficient: the qcadcm graph was rebuilt ~3.75 s before WASAPI READY, but observed VI/CPS tap traffic begins only after stream start.

This is the strongest current localization of the missing Linux operation:

`speaker stream START -> sub-second kernel/hardware-client activation -> CPS real PCM (+236 ms) -> VI real PCM (+741 ms)`

## 7. Exact Windows WSA start-owner chain to correlate against tap birth

The preserved Windows graph-birth trace contains exactly one `AUCD_WSA_START_OWNER` event. Its stack is:

- `qcaudminiport8380+0x10f60`
- `qcaudminiport8380+0x5710`
- `qcaudminiport8380+0x75cd4`
- `qcaudminiport8380+0x7a5e4`
- `qcaudminiport8380+0x6d9b0`
- `qcaucd8380+0x4f1ac`
- `qcaucd8380+0x4d51c`
- `qcaucd8380+0x268f8`
- `qcaucd8380+0x255e8`
- `qcaucd8380+0x2c2ec`
- `qcaucd8380+0x286d4`
- `qcaucd8380+0x1e574`
- `qcaucd8380+0x36510` (`AUCD_WSA_START_OWNER`)

The same lifecycle subsequently reaches the known SoundWire dataport programming and WSA lifecycle functions:

- dataport apply around `qcaucd +0x3df18`
- resource/start handling around `+0x31d28` / `+0x31d54`
- WSA lifecycle mode=1 around `+0x32e10`

Do not infer causality from the old trace's debugger wall-clock timing because breakpoints distorted time. The next experiment must timestamp this call chain and DATA_LOGGING packets in the **same fresh render-start run**.

## 8. Current synchronized-capture state at checkpoint time

A fresh KD session was started on SP7 for a synchronized WSA-start/tap capture:

- KD job id: `job_-YmoJUEncC7Sm6YN1gnB1A-p`
- target: SP11 Windows
- connected over KDNET port 50005
- debugger was manually broken at approximately `2026-08-19 23:34:07 +01:00`

At the moment of this checkpoint, `lm m qcaudminiport8380` / `lm m qcaucd8380` returned no currently loaded modules. No new conclusion is drawn from that alone. The intended capture has **not yet produced a result** and must resume from this point or cleanly re-arm on module load.

## 9. Closed hypotheses reinforced by this checkpoint

Keep closed absent new direct evidence:

- qcaudminiport op8 / qcaucd `0x202` / WSA ADC-state path as the per-render sample-feed activation
- hidden class-1 op4 callback-list attachment
- missing `0x0725a000 = 0x50000`
- missing `0x06b6c0b0 = 1`
- generic qcaucd op4 physical MMIO parity as the remaining root cause
- SurfaceAPO hardware proxy on this SP11 configuration
- Qualcomm ProxyAPO as a direct driver-I/O / VI/CPS hardware-feed owner
- audiodg recycle as protection-graph birth trigger

Earlier closed hypotheses from previous checkpoints remain closed as well, including generic SoundWire geometry, generic CODEC_DMA_SOURCE SET_CFG, static SP/SP_VI config, IMCL existence, simple trigger ordering, host clock + `0x105c/0x1d54`, and WSA slave FIFO/DP6 programming.

## 10. Next decisive experiment

Run one synchronized capture with:

1. Windows LPASS `0x1586` collector recording tap1/tap2/tap3 UTC timestamps.
2. KD traps restricted to the render-start chain:
   - qcaudminiport hardware-interface boundary around `+0x6d9b0`
   - qcaucd WSA start-owner `+0x36510`
   - immediate resource/start functions as needed (`+0x31d28/+0x31d54`, `+0x32e10`)
3. Rebuild qcadcm protection graph through the ADCM PnP restart.
4. Start the known shared-mode WASAPI renderer with silent pre-roll and delayed 997 Hz tone.
5. Correlate the first kernel event(s) against:
   - CPS tap3 first nonzero
   - VI tap2 first nonzero

If `qcaucd+0x36510` or one immediate child operation consistently occurs immediately before tap3/tap2 become live, decompile and instrument only that path to identify a non-matched LPASS/AFE/hardware-client side effect. Port only that missing operation into a disposable Golden-v31-derived Linux candidate.

Promotion gate remains unchanged: Linux candidate must produce real nonzero tap2 8 kHz VI PCM and tap3 24 kHz CPS PCM during an acoustically proven speaker render, with no faults. Golden v31 must never be overwritten.
