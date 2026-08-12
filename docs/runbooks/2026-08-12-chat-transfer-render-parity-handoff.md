# 2026-08-12 Chat Transfer — SP11 Render-Parity Closure Handoff

## Objective

Finish the Surface Pro 11 Windows -> Linux **speaker/render** audio port before spending time on microphone/capture. The required end state is not merely "sound comes out": Linux should reproduce the proven Windows render graph families, processing-mode policy, native Dolby path/state, speaker-protection sidechains, and hardware transport semantics as closely as the available evidence permits.

Microphone/capture is explicitly deferred until speaker/render parity is complete.

## Operating methodology / machine roles

- **SP7** is the canonical engineering authority:
  - repo: `C:\Users\SurfacePro7\Documents\SP11X1e-audio-engineering`
  - KDNET host
  - evidence review
  - canonical commits and pushes
- **SP11 Linux** is the default GRUB target and Linux deployment/runtime target.
- **SP11 Windows** is reached only through the one-shot Windows GRUB entry when a specific Windows/KD fact must be closed. Do not change the persistent default away from Linux.
- **Fedora MacBook** is a build/analysis helper, not Git authority.
- Current audible-test ceiling requested by user:
  - Windows: **<= 12%**
  - Linux: **<= 25%** (chosen inside the user's 20-30% range)
  - Always explicitly clamp/check before playback rather than assuming the previous value survived a reboot/session.

## Debugger safety — preserve exactly

- `kd.exe` / kd-mcp is the sole KDNET owner; never run classic WinDbg simultaneously.
- Never debugger-read physical MMIO (`!dd`/direct physical WSA/SoundWire access previously caused fatal 0x124/reboots).
- No debugger MMIO writes, DSP writes, arbitrary driver-state writes.
- Read-only logging/execution breakpoints and immediate `gc` only.
- One risky debugger command at a time.
- Validate pointers and lengths before dereference.
- Use normal kernel virtual addresses only.
- Persistent `.logopen` for evidence captures.
- Avoid `$><` / `$$><` batching for live fragile work.
- ARM hardware execution-breakpoint limit is tight: three concurrent `ba e` probes previously failed with `Too many data breakpoints`; use one or at most the proven small set of two.
- Detach sequence: break, `bc *`, `.logclose`, `qd`; target must not remain globally stopped.
- Reboot invalidates ASLR runtime addresses.
- PiSlave connectivity can flap while the target is globally stopped; this is not itself evidence of OS failure.

## Canonical Git state at handoff creation

Branch:

`agent/cps-dp6-runtime-closure-20260810`

Pre-handoff HEAD/origin:

`1f57f2d  Map remaining Windows speaker render families`

Recent render-relevant commits:

- `1f57f2d` Map remaining Windows speaker render families
- `77fe558` Build exact DEFAULT and NOTIFICATION render families
- `2b61df7` Close NOTIFICATION ACDB calibration
- `d9c2f4e` Localize Windows pre-VLLDP drive to Dynamic VR
- `30acb2c` Preserve CPS V3 runtime closure and recovered source lineage
- `fae2c7c` Finalize SP11 CPS parity patch series
- `1df9feb` Add offline-validated CPS parity candidate
- `dd8b1c7` Correct historical Dolby callback-order claims
- `501ef18` Compare live YouTube and Alerts Dolby paths
- `8eb9045` Add evidence-bound Windows speaker architecture
- `a3a8a42` Document live Windows render-mode split

## Linux runtime state already achieved

The last checked SP11 Linux runtime was `7.1.5-sp11-cps-v3+`, saved GRUB entry `sp11-audio-cps-v3`.

A fresh bounded Linux playback proved the current protected speaker path comes up with:

- normal render at 48 kHz;
- WSA playback ports 1/2/3;
- VI at 8 kHz using both WSA8845 DP5 paths;
- CPS at 24 kHz using both WSA8845 DP6 paths with mask `0x03`/`0x03`;
- `WSA_CODEC_DMA_TX_0` -> VI ready;
- `WSA_CODEC_DMA_TX_1` -> CPS ready;
- SP + SP_VI configuration accepted;
- VI endpoint calibration accepted;
- render endpoint calibration accepted;
- VI+CPS protection enabled;
- MSIIR calibration accepted;
- channel mixer calibration accepted;
- `GRAPH_START` accepted;
- no observed XRUN, SoundWire collision, PA fault, or protection-bypass failure in that bounded check.

The known broad graph-calibration `AR_EUNSUPPORTED` warning remains compatible with the Windows/GSL-style warning policy because graph setup continues and succeeds; do not treat it as a fatal failure by itself.

The deployed Linux runtime is still effectively centered on the proven DEFAULT render path. Offline work has now recovered/built the exact DEFAULT and NOTIFICATION families, but full runtime policy/deployment across Windows processing modes remains unfinished.

## Windows render-mode ground truth already closed

Static mapping, hash-bound to the reviewed drivers:

| Windows mode | miniport flag | qcadcm enum | GKV |
|---|---:|---:|---:|
| DEFAULT | `0x01` | 2 | 2 |
| RAW | `0x02` | 1 | 1 |
| COMMUNICATIONS | `0x04` | 4 | 6 |
| SPEECH | `0x08` | 3 | 5 |
| NOTIFICATION | `0x0A` | 7 | 7 |
| MEDIA | `0x14` | 6 | 4 |
| MOVIE | `0x28` | 5 | 3 |

Live evidence already proves:

- Win32 `PlaySound(..., SND_SYSTEM)` -> flag `0x01` -> DEFAULT.
- WinRT `AudioCategory=Alerts` -> flag `0x0A` -> NOTIFICATION.
- WinRT `AudioCategory=Media` -> flag `0x01` -> DEFAULT in the controlled test.
- WinRT `AudioCategory=Movie` -> flag `0x01` -> DEFAULT in the controlled test.
- Real Edge/YouTube from an idle-to-fresh-client transition -> flag `0x01` -> DEFAULT.
- A single observed `audiodg.exe` host serviced controlled DEFAULT and NOTIFICATION cases; do not invent separate Media/System `audiodg` engines/processes.

## New 2026-08-12 live overlap evidence from the current Windows one-shot boot

This boot was entered specifically for render-policy closure. The SP11 was Windows while SP11 Linux was consequently offline.

ASLR addresses on this boot (DO NOT reuse after reboot):

- qcaudminiport processing-mode translator: `fffff8004c764080` (RVA `+0x94080`)
- qcadcm ACDB selector boundary: `fffff800505e07a8` (RVA `+0x307a8`)
- qcadcm mapped GRAPH_OPEN request boundary: `fffff8005060aa34` (RVA `+0x5aa34`)
- qcadcm outbound GPR boundary `__gpr_cmd_async_send`: `fffff80050601e20` (RVA `+0x51e20`)

The controlled overlap stimulus keeps a Media stream alive, waits 4 seconds, starts an Alerts stream while Media is still active, waits, then stops Alerts and finally stops Media.

Direct live mode hits:

- Media start: `[MODE_LIVE 1] flag=1` -> DEFAULT.
- Alerts start while Media remained active: `[MODE_LIVE 2] flag=a` -> NOTIFICATION.

Therefore Windows definitely requests NOTIFICATION while a DEFAULT-class render stream is already alive. This closes the client/mode-request half of the concurrency question.

A valid DEFAULT selector vector was recovered live:

```
01000001 = 2
01000002 = 2
01000003 = 1
```

A full valid NOTIFICATION six-key vector was also recovered manually from a live kernel pointer:

```
01000001 = 2
01000002 = 7
01000003 = 1
01000004 = 2
01000005 = 7
01000006 = 1
```

This independently confirms the NOTIFICATION processing value (`7`) on both stream and mix processing keys with render endpoint value `1`.

### Important pointer caveat

At `qcadcm+0x307a8`, `x8` is **not universally a valid selector-vector pointer on every invocation**. The live log contains low scalar-looking values (`0`, `1`, `0x1f1`) on many calls. Only dereference `x8` when it is first validated as a canonical kernel VA. Earlier displays of `????????` from low addresses are not evidence and must not be interpreted.

## Incomplete lifecycle experiment — DO NOT overclaim

The second log was intended to capture actual APM lifecycle traffic during DEFAULT+NOTIFICATION overlap using:

- `qcadcm+0x5aa34` for large mapped GRAPH_OPEN OOB bodies;
- `qcadcm+0x51e20` for outbound APM opcodes `0x01001000..0x01001004`.

The breakpoints were armed safely, but **no actual `[OPEN_OOB ...]` or `[APM_LIFE ...]` runtime hit occurred before the handoff was requested and the session was stopped**. The strings appear in the log only because the breakpoint command text itself was logged.

Therefore the following question is STILL OPEN:

> During simultaneous DEFAULT + NOTIFICATION rendering, does Windows keep two render-family graphs alive concurrently, dynamically replace/change one graph, reuse/refcount a shared graph, or perform another lifecycle sequence?

Do not infer the answer from the mode hits alone.

## Raw KD logs preserved

Original SP7 locations:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_RENDER_POLICY_20260812_0740BST.log`
  - size: 76,766 bytes
  - SHA-256: `750CA2445001B15BF34DAA955F93F1149509D7814CEBEEE4B7116CB281932BBD`
- `C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_RENDER_OVERLAP_LIFECYCLE_20260812_0823BST.log`
  - size: 8,642 bytes
  - SHA-256: `2C6849B964D9D51BE457B00D72B325442985712D096ECA95624CD37532444A8B`

The first contains the useful mode/GKV overlap observations. The second is a setup/incomplete lifecycle witness and must remain labeled as such.

## KD state at handoff

The live KD session was explicitly closed before writing this handoff:

1. target interrupted;
2. `bc *`;
3. `.logclose`;
4. `qd`;
5. verified no `kd.exe` process and no WinDbg process on SP7.

No debugger MMIO reads/writes occurred.

## Dolby render pipeline — current canonical model

Current strong evidence supports:

```
source PCM
  -> DolbyApoVr
  -> DolbyAPOvlldp150
  -> exact AudioEng final limiter
```

Also established:

- `DolbyDax3Apo` wrapper is directly on the live speaker PCM path.
- The wrapper uses its equal-rate direct branch; prior work rules out an invented wrapper SRC/gain/mix stage for the controlled path.
- Current real YouTube DEFAULT and isolated Alerts/NOTIFICATION both execute strict `VR -> VLLDP` outer-callback ordering on the tested boot(s).
- The older `VLLDP -> VR` callback-order observation is historical/session-specific and not a universal architecture rule.
- For the controlled Windows Dynamic profile, the unexplained pre-VLLDP drive is localized inside **Dolby VR**, not a guessed +3 dB stage, channel sum, VLLDP gain, or Linux EQ.
- `DAX3API.exe` is control/policy, not inline PCM.
- SurfaceAPO / modern ASAR / HRTF modules have observed presence/lifetime evidence; do not place every loaded DLL serially in the PCM path without hot-path evidence.

## Exact AudioReach structural state

DEFAULT and NOTIFICATION are processing-mode alternatives, not physical left/right chains.

DEFAULT and NOTIFICATION exact structural models and ACDB work are already in the canonical repo via `77fe558` and `2b61df7`.

Both converge on the shared root:

```
SAL 4001
 -> CHMIXER 402c
 -> SPEAKER_PROTECTION 4027
 -> SPLITTER 4002
 -> LOGGER 4003
 -> CODEC_DMA_SINK 4157
```

Ordinary playback endpoint is proven as:

- LPAIF WSA interface index 1;
- 48 kHz;
- fixed-point PCM;
- 16-bit;
- two channels;
- active mask `0x3`;
- one enabled WSA/SoundWire macro instance;
- exactly two WSA8845 amplifiers.

VI and CPS are separate feedback/control branches into speaker protection, not serial PCM stages.

CPS transport is already closed and Linux V3 is live:

- WSA interface 3;
- 24 kHz S32;
- two channels mask `0x3`;
- one shared physical SoundWire master port 13;
- both slaves DP6 mask `0x03`;
- left WSA8845 identity `0x0000000402170220`, Offset1 `0`;
- right WSA8845 identity `0x0000000402170221`, Offset1 `25`.

Do not copy CPS DP6 geometry onto ordinary render.

## Highest-priority next experiment

Repeat the DEFAULT+NOTIFICATION overlap lifecycle capture correctly from a known phase boundary.

Recommended sequence:

1. Confirm SP11 is in Windows through the one-shot entry; persistent default remains Linux.
2. Clamp Windows endpoint to <=12% before playback (using the existing Core Audio helper; 6% is also fine).
3. On SP7 verify **zero existing kd/WinDbg owner**.
4. Start one KD owner and recover current ASLR bases. Do not reuse the addresses in this file after reboot.
5. Use at most **two** execution breakpoints at once.
6. Phase A should favor:
   - qcaudminiport mode translator `+0x94080`, and
   - qcadcm outbound GPR `+0x51e20`, filtered to lifecycle opcodes.
   This directly correlates mode request and lifecycle without exceeding the hardware-breakpoint budget.
7. Arm breakpoints **before** starting the overlap stimulus. The incomplete 08:23 pass armed lifecycle probes after the useful overlap activity had already happened.
8. Run the existing overlap stimulus:
   `C:\Users\Geoca\Documents\SP11-Audio-RE\night-matrix\overlap-modes.ps1`
   It was originally clamped at 6%, safely below the current 12% ceiling.
9. Capture the sequence across:
   - DEFAULT Media start,
   - NOTIFICATION Alerts start while DEFAULT is alive,
   - Alerts stop,
   - Media stop.
10. If a GRAPH_OPEN body itself is still needed, do a separate Phase B with `qcadcm+0x5aa34` rather than adding a third simultaneous `ba e` breakpoint.
11. Break, `bc *`, `.logclose`, `qd`.
12. Hash raw log, decode lifecycle, create reviewed JSON/finding, commit/push from SP7.

The decision needed for Linux is specifically whether `7e/7f` and `82/83` coexist, switch, or are refcounted/changed under overlap.

## Render-first remaining work, in order

1. **Close DEFAULT + NOTIFICATION lifecycle/concurrency** with the bounded Windows KD experiment above.
2. **Implement/deploy Linux multi-family render policy** using the exact DEFAULT and NOTIFICATION bodies/calibration already recovered. Do not invent Windows policy; implement what the lifecycle trace proves.
3. **Close live behavior for remaining processing modes only where it affects Linux policy.** Static mapping/family ACDB work exists for SPEECH, COMMUNICATIONS, MEDIA, etc.; ordinary WinRT Media/Movie tests previously selected DEFAULT, so distinguish API category behavior from the existence of static GKV families.
4. **Complete Dolby parity**:
   - exact VR state/profile/history behavior;
   - VR -> VLLDP state matching;
   - final limiter/loudness oracle comparison Windows vs Linux;
   - avoid guessed gain compensation.
5. **Finish speaker render/protection parity** where still relevant to output quality/safety:
   - any remaining amp reset/lifecycle semantics;
   - protection telemetry/limiter intervention validation;
   - PBR/VI runtime details only when they materially affect speaker protection or output quality.
6. **Ordinary render SoundWire last-mile binding** remains less closed than CPS (slave dataport/offset/slot details and protected-speaker index -> physical L/R). Do not fabricate it; close it if needed for parity or diagnostics.
7. **Microphone/capture stays last** by explicit user priority.

## Project discipline

- When a wall is hit, record it precisely, mark what is proven vs unproven, then move to the next independent closure item. Return later with a different evidence route rather than looping the same failed experiment.
- Prefer exact Windows binaries/ACDB/live KD evidence over prose or old assumptions.
- Prefer existing upstream Linux semantics/framework mechanisms where they match Windows evidence; keep SP11-specific physical values in board data where appropriate.
- Every worthwhile discovery/correction/build/deployment result goes into the SP7 repo and is committed/pushed.
- Never claim "complete Windows parity" merely because sound is audible. Completion means the render topology, processing-mode policy, Dolby path/state, protection sidechains, and hardware boundary are evidence-backed and Linux runtime validation agrees.
