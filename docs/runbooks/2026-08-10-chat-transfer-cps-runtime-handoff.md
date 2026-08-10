# SP11 CPS / SoundWire chat-transfer handoff

Date: 2026-08-10 23:40 Europe/London
Purpose: restart this RE session in a fresh ChatGPT conversation without depending on conversational memory.

## New-chat startup contract

Do not reconstruct this project from memory or guesses. Treat this file, the Git branch, and the raw KD logs on SP7 as the source of truth.

On a fresh chat, use PiMaster to reach SP7 and SP11. Before starting any debugger:

1. Read this file in full.
2. Check Git status/log in `C:\Users\SurfacePro7\Documents\SP11X1e-audio-engineering`.
3. Check SP7 process list for `kd.exe`, `windbg.exe`, and debugger PTY jobs.
4. If a debugger is already active, attach to/control that existing job. Never start a second KD owner.
5. Check PiMaster client status. SP11 Windows may flap offline during debugging; KD connectivity is independent and remains the primary evidence path.
6. Preserve all useful discoveries in `docs/findings` + `artifacts/reviewed`, commit them, and push the branch without waiting for the user to ask.

No KDNET key, credential, token, or other secret belongs in Git, findings, debugger logs, or chat output.

## Machines and roles

- SP7 Windows: debugger host and primary repo checkout.
- SP11 Windows: KDNET target under test.
- KD transport: USB EEM/KDNET between SP7 and SP11.
- SP11 Windows PiSlave: useful for short audio/device actions when reachable, but it can drop during kernel debugging/network power transitions.
- macbook Fedora: optional auxiliary host.
- SP11 Linux: often offline; Linux-side work is not required for the Windows capture unless moving to implementation/testing.

## Absolute debugger safety rules

- `kd.exe` / kd-mcp is the sole KDNET owner. Never run classic WinDbg at the same time.
- Never perform direct debugger physical MMIO reads such as plain `!dd` against WSA/SoundWire registers on this target. Prior physical reads caused fatal 0x124 failures/reboots.
- No debugger MMIO writes, DSP writes, or arbitrary driver-state writes.
- Use read-only logging breakpoints and immediate `gc` where possible.
- Send one potentially risky debugger command at a time; read and verify its response before the next.
- Do not blindly dereference payload pointers. Validate pointer class and bounded size first.
- Prefer normal kernel virtual-memory payloads already owned by the driver.
- Persistent `.logopen` before interesting breakpoints.
- Avoid debugger command-script batching (`$><`, `$$><`) because line collapsing previously corrupted commands.
- Healthy detach sequence: break in, `bc *`, `.logclose` if logging, then `qd`. Do not leave target globally broken.
- A reboot invalidates ASLR runtime addresses. Resolve module base again before setting RVA-derived breakpoints.

## Git state at transfer preparation

Repository:
`C:\Users\SurfacePro7\Documents\SP11X1e-audio-engineering`

Branch:
`agent/cps-dp6-runtime-closure-20260810`

Remote:
`origin` -> GitHub repository `geocausa/SP11X1e-audio`

State immediately before adding this handoff:
- clean working tree;
- branch synchronized with origin;
- HEAD `dc70dff` — `Document Windows CPS ID driverstore sweep`.

GitHub CLI authentication on SP7 was refreshed successfully in this session. Normal HTTPS pushes are working again.

Recent commits before this handoff:
- `dc70dff` Document Windows CPS ID driverstore sweep
- `dbe173d` Close qcadcm query response follow-up
- `9f2e3bf` Document qcadcm query event runtime negative
- `66f1774` Document common GPR CPS runtime negative
- `d0c240c` Document qcadcm tagged config runtime capture
- `8e93aab` Prepare Windows-parity CPS replacement plan
- `4d8673f` Document Windows CPS DP6 runtime transport
- `d4e2e42` Document CPS transport closure and KD capture handoff

## Original task / success criterion

Determine the exact Windows per-speaker CPS SoundWire transport used by Surface Pro 11, without changing Linux channel masks during capture.

The strict handoff considered capture complete when WSA master port 13 was bound to the two individual WSA8845 identities and either:
- literal runtime `PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) was captured, or
- equivalent complete per-slave DP6 configuration was preserved.

The second condition has been achieved. Therefore the core Windows transport question is functionally closed. Literal `0x08001259` is now a strict/bonus gap, not a blocker to the per-speaker transport result.

## Hash gate / exact Windows drivers

`qcadcm8380.sys`
SHA-256: `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`
Loaded package path:
`C:\WINDOWS\system32\DriverStore\FileRepository\qcadcm8380.inf_arm64_f5fba49e0720d715\qcadcm8380.sys`

`qcaucd8380.sys`
SHA-256: `bd0c8276c51fc7a020c616e904dd613b6ccf187ec3e1fe6f94c2c811c8adc8bf`
Loaded package path:
`C:\WINDOWS\system32\DriverStore\FileRepository\qcaucd8380.inf_arm64_53bcc309a68aba55\qcaucd8380.sys`

Do not mix evidence from another driver build without explicitly re-hashing/re-basing static RVAs.

## Transport result — established Windows runtime truth

CPS graph endpoint:
- CODEC_DMA_SOURCE instance `0x402b`
- 24,000 Hz
- S32 / 32-bit fixed-point
- 2 channels
- channel mask `0x3`
- LPAIF WSA interface index `3`
- Linux endpoint correspondence: `WSA_CODEC_DMA_TX_1` (`0xb003`)

WSA master:
- qcaucd owns the speaker SoundWire bus.
- WSA SoundWire master MMIO physical base: `0x06b10000`.
- Master port 13 has the 24 kHz CPS interval.

Speaker identities:
- left WSA8845: `0x0000000402170220` (logical dev 2 in captured FIFO traffic)
- right WSA8845: `0x0000000402170221` (logical dev 1)

Both speakers use SoundWire DP6 ChannelEnable `0x03`.
They are NOT split `0x1` / `0x2`.

Per-speaker DP6 active programming observed from the Windows driver's own SoundWire command FIFO:

Common to both:
- `0x0630` ChannelEnable = `0x03`
- `0x0632` SampleCtrl1 = `0x1f`
- `0x0633` SampleCtrl2 = `0x03`
- `0x0636` HCtrl = `0xff`
- `0x0603` BlockCtrl1 = `0x18`
- `0x0637` BlockCtrl3 = `0x00`

Left `0x0000000402170220`:
- `0x0634` OffsetCtrl1 = `0x00`

Right `0x0000000402170221`:
- `0x0634` OffsetCtrl1 = `0x19` (25)

No observed writes to:
- `0x0631` BlockCtrl2
- `0x0635` OffsetCtrl2

Teardown:
- ChannelEnable `0x00` written to both `0x0620` and `0x0630` on both devices.

Sample interval:
- `SampleCtrl2:SampleCtrl1 = 0x03:0x1f` => encoded value `0x031f` => interval 800 bus clocks.
- At 19.2 MHz this matches 24 kHz.

Master port-13 writes captured through qcaucd's own helper:
- `0x06b11d24 = 0x0000001f`
- `0x06b11d2c = 0x00000018`
- `0x06b11d34 = 0x000000ff`
- `0x06b11d38 = 0x00000000`
- `0x06b11d3c = 0x00000003`
- `0x06b11d54 = 0x00000003`
- `0x06b11d64 = 0x00ff001f / 0x0300001f` during transitions
- `0x06b11d74 = 0x000000ff`
- `0x06b11d78 = 0x00000000`
- `0x06b11d7c = 0x00000003`

Packed SoundWire FIFO command decode:
`packed = reg | (cmd_id << 16) | (logical_dev << 20) | (data << 24)`

The successful qcaucd capture decoded 328 FIFO writes and repeated the same CPS programming across three playback initializations.

## Linux implication already established

Rejected Linux reconstruction:
- split CPS masks `0x1` / `0x2` conflicted;
- left-only `0x1` conflicted.

Windows parity says a future Linux candidate must keep native CPS mask `0x3` for BOTH WSA8845 DP6 ports and carry the speaker distinction through the per-device timing offset (`0` left / `25` right), using normal SoundWire/WSA port-parameter machinery rather than direct MMIO.

## Literal `0x08001259` search — current evidence

`PARAM_ID_CPS_LPASS_HW_INTF_CFG = 0x08001259` has NOT been observed literally.
This is now backed by several independent negatives rather than one missed hook:

1. qcadcm static binary: no aligned/static `0x08001259`.
2. qcaucd static binary: no aligned/static `0x08001259`.
3. qcadcm custom/tagged-config wrapper captures: no literal `0x08001259`.
4. common qcadcm GPR `APM_CMD_SET_CFG` boundary:
   - 74 recognized SET_CFG submissions;
   - 135 complete bounded payload searches;
   - sizes through `0x28e0` bytes;
   - no `0x08001259`.
5. CPS graph-open OOB payload:
   - repeated `0xb18` payloads;
   - contains aligned `INTENT_ID_CPS = 0x08001537`;
   - does not contain `0x08001259` or `0x08001254`.
6. qcadcm GPR receive/query path:
   - full GET_CFG response bodies captured and searched;
   - no `0x08001259`.
7. graph-event path:
   - thousands of graph events observed;
   - dominant event `IID 0x4660 / event 0x0800101c / size 4`;
   - one observed `IID 0x466b / event 0x08001043 / size 0`;
   - no `0x08001259`.
8. qcadcm HW-resource custom command path was traced and statically identified as generic AudioHwRscIoctl core/clock/GPIO management, not CPS LPASS transport.
9. whole captured Windows audio DriverStore aligned-dword sweep:
   - aligned `0x08001259`: zero files / zero hits;
   - aligned `0x08001254`: zero files / zero hits;
   - positive controls worked: aligned `0x08001537` in `qcadsp8380.mbn`; aligned `0x08001063` in `qcadcm8380.sys`.
   - apparent unaligned Dolby matches were false positives inside PE `.pdata` unwind metadata.

Interpretation: this exact Windows build does not appear to materialize the public `0x08001259` ID through the ordinary qcadcm graph configuration/query/event paths, and may implement the semantic contract privately or below qcadcm. Do not generalize this absence to all Qualcomm platforms or versions.

## Important static RE corrections / dead ends

- `qcslimbus8380.sys` is SLM1/Bluetooth, not the speaker bus.
- qcadcm GetDmaCfgInfo selector at static RVA around `0x674c4` is `0x08001063` (`PARAM_ID_CODEC_DMA_INTF_CFG`), not `0x08001259`.
- Live GetDmaCfgInfo hits were ordinary 48 kHz endpoint-HW lookups, not the internal CPS graph module.
- qcadcm `gsl_command_hw_rsc_custom_config` / `AudioHwRscIoctl` path handles hardware core/clock/GPIO enable/disable and speaker-protection event callback registration; it is not the missing CPS LPASS transport path.
- qcadcm `SetDpHwIfCfg` is DisplayPort audio (`mst_idx`, `dptx_idx`, `channel_allocation`), not SoundWire data-port 6. Its compared PID is `0x08001154`. Do not confuse "DP" there with SoundWire DP6.
- ACDB lacks `0x08001259` / `0x08001254`; `AcdbGetWsaCfg` is a driver-data retrieval path and should not be relabeled as the HLOS CPS parameter without runtime evidence.

## Reconstructed CPS topology evidence

The repo topology contains the exact `0x402b` records:

- `0x402b / 0x08001017`: 24000 Hz and `0x00020020` (2 channels / 32-bit formatting context)
- `0x402b / 0x08001063`: payload dwords `0x00000002, 0x00000003, 0x00000003`
- adjacent `0x402b` records include `0x08001018`, `0x08001176`, `0x080013d5`

This independently agrees with the 24 kHz / 2-channel / WSA interface reconstruction.

## Primary reviewed findings in Git

Read these before repeating any experiment:

- `docs/findings/2026-08-10-windows-cps-dp6-runtime-capture.md`
- `artifacts/reviewed/2026-08-10-windows-cps-dp6-runtime.json`
- `docs/findings/2026-08-10-qcadcm-tagged-config-runtime.md`
- `artifacts/reviewed/2026-08-10-qcadcm-tagged-config-runtime.json`
- `docs/findings/2026-08-10-qcadcm-common-gpr-cps-runtime.md`
- `artifacts/reviewed/2026-08-10-qcadcm-common-gpr-cps-runtime.json`
- `docs/findings/2026-08-10-qcadcm-query-event-hwresource-runtime.md`
- `artifacts/reviewed/2026-08-10-qcadcm-query-event-hwresource-runtime.json`
- `docs/findings/2026-08-10-windows-audio-driverstore-cps-id-sweep.md`
- `artifacts/reviewed/2026-08-10-windows-audio-driverstore-cps-id-sweep.json`
- `docs/runbooks/2026-08-10-kd-mcp-cps-soundwire-runtime-handoff.md`

## Raw KD evidence on SP7 (outside Git)

Per-slave DP6 capture:
`C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_DP6_SLAVES_20260810_2007BST_25f4_2026-08-10_20-07-15-660.log`
- size 48,229
- SHA-256 `A6BBF3574E6CAAF5FDB0FC46EC4BAD0106321AF90CCE91FBEB4F2015B60B66EB`

Reviewed extract:
`C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_DP6_SLAVES_20260810_2007BST-extract.txt`
- size 4,182
- SHA-256 `D262B3842F9D4620E26DF495EA016412C419010FC24321825978F126ED0662EF`

Master/enumerator runtime:
`C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_SWR_RUNTIME_20260810_1909Z_2d7c_2026-08-10_19-09-12-880.log`
- SHA-256 `EE8CB66EB3D7A44BF7FE4AADD61F04BB29BA85520DF5E15E5DED95D2C1B3DC36`

Common qcadcm GPR capture:
`C:\Users\SurfacePro7\Documents\KDNET\Codex\QCADCM_DMA_CFG_20260810_2119BST_2448_2026-08-10_21-19-15-709.log`
- size 151,696
- SHA-256 `43722CAEE559A04F6D3729D1D2A6A8AE7C18966A6A60FCD474289F383D9D7900`

Query/event/early-boot follow-up:
`C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_EVT_20260810_2249BST_167c_2026-08-10_22-49-44-527.log`
- size 385,126
- SHA-256 `1CB1E8C1D4CA1BFAFA26C759897FF958EC48CB5FB86AA3B51B261F42B17EB337`
- ended with `bc *`, `.logclose`, `qd`.

Older external checkpoint:
`C:\Users\SurfacePro7\Documents\KDNET\Codex\2026-08-10-cps-runtime-re-checkpoint-2136.md`
This transfer handoff supersedes it when the two disagree.

## Current debugger state at transfer preparation

SP7 process-list check at handoff creation found zero processes matching `kd`.
The latest KD session detached cleanly. Assume NO breakpoint is active and NO runtime qcadcm address remains valid for a future session.

Before any new KD session, still re-check process/job state rather than trusting this snapshot.

## SP11 connectivity behavior

SP11 Windows PiSlave may alternate online/offline while the target is under kernel debugging. This appears tied to target-side networking/power state and should not be mistaken for loss of KD evidence.

Use PiSlave opportunistically for short actions (e.g. audio playback). Do not repeatedly restart devices merely to recover PiSlave. If PiSlave is down, continue static/KD work and retry later.

## Recommended next decision

The Windows reverse-engineering objective is already satisfied at the transport-equivalence level. Do NOT endlessly repeat qcadcm searches for `0x08001259` unless a genuinely new boundary is identified.

Best next move depends on the user's goal in the new chat:

A. If continuing strict Windows archaeology:
- identify a truly new private boundary below/around qcaucd or DSP image handling, not another wrapper around qcadcm SET_CFG;
- remain read-only;
- do not use physical debugger MMIO reads.

B. If moving the project forward toward Linux audio:
- use the established Windows parity as the implementation target;
- preserve both WSA8845 CPS DP6 masks at `0x3`;
- left Offset1 `0`, right Offset1 `25`;
- 24 kHz / 800-clock interval;
- use normal SoundWire/WSA port parameter paths;
- keep the literal `0x08001259` absence documented as a Windows-version implementation detail, not a prerequisite for Linux transport parity.

Before changing Linux/build/GRUB configuration, read the current repo plan and rejected-candidate findings so old mask experiments are not accidentally reintroduced.

## Methodology to preserve

This session's useful pattern was:

- hash-lock proprietary binaries first;
- use static Ghidra work to identify narrow, semantically meaningful runtime boundaries;
- validate static RVA against live instructions after every reboot/base change;
- use bounded read-only logging breakpoints instead of broad breaks;
- capture the driver's own writes rather than directly reading dangerous hardware registers;
- distinguish direct observation from source-backed interpretation from inference in every finding;
- retain raw evidence outside Git, put reviewed/machine-readable summaries inside Git;
- hash raw logs in the finding;
- commit/push discoveries promptly;
- explicitly document false leads so a new agent does not repeat them.

## Minimal bootstrap prompt for the new chat

Use this exact idea (paths are sufficient; no need to paste this whole file into chat):

> Continue my Surface Pro 11 CPS/SoundWire reverse-engineering session. SP7 is the KDNET host, SP11 Windows is the target, and PiMaster can access both. First, on SP7 read `C:\Users\SurfacePro7\Documents\SP11X1e-audio-engineering\docs\runbooks\2026-08-10-chat-transfer-cps-runtime-handoff.md` in full. Treat it and the Git branch as the source of truth. Verify Git status/HEAD and verify no existing kd/WinDbg owner before doing anything. Preserve the debugger safety rules exactly, do not use direct physical MMIO reads, and commit/push every worthwhile discovery. Then continue from the recommended next decision rather than repeating closed experiments.

## Post-transfer continuation checkpoint (2026-08-10 23:58 BST)

The first strict-Windows continuation found the genuinely new private boundary requested above:

- `qcaucd8380.sys` direct slave write helper: RVA `0x31188` (`FUN_140031188`), register in argument 1, value in argument 2;
- direct slave read helper: RVA `0x31298` (`FUN_140031298`);
- generic write abstraction: RVA `0x25810` (`FUN_140025810`), which can route a raw register into the lower SoundWire command path rather than only fixed WSA telemetry;
- generic read abstraction: RVA `0x20bc0` (`FUN_140020bc0`), with the matching lower SoundWire read path;
- per-slave setup function RVA `0x31430` reads slave identity/state registers `0x3401..0x3404`.

Reviewed result:

- `docs/findings/2026-08-10-qcaucd-slave-register-private-boundary.md`
- `artifacts/reviewed/2026-08-10-qcaucd-slave-register-private-boundary.json`

This supersedes only the earlier narrow statement that reviewed qcaucd routines did not expose a slave-register boundary. It does **not** prove a static DP6 caller or a literal `0x08001259` path.

A first runtime attempt was intentionally abandoned before arming any breakpoint because KD produced an internally inconsistent kernel view (`lm` showed only `nt`, while SP11 PiSlave independently reported `qcaucd` Running and the Qualcomm audio stack healthy). `!drvobj qcaucd 2` also failed to read normal object-manager state. No physical MMIO read, MMIO write, DSP write, slave-register write, or driver-state write was performed. The session ended with `bc *`, `.logclose`, `qd`, and `kd.exe` was verified absent.

**Updated next strict-Windows decision:** on a future coherent KD attach, resolve the fresh qcaucd image base and use a bounded read-only logging breakpoint on RVA `0x31188`, filtered to slave registers `0x0600..0x063f`, with immediate `gc`. The exact question is whether normal protected speaker playback sends the already-known left/right DP6 assignments through this private qcaucd boundary. Do not return to the closed qcadcm searches first, and do not replace this with debugger physical-MMIO reads.

End of transfer handoff.
