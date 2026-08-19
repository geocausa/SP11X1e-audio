# SP11 protected-path checkpoint — 2026-08-19 23:36 BST

## Status / provenance

This checkpoint preserves findings made after Linux repo checkpoint commit `28334d682fa86dc5e0696dd2ecac64a81509edd9` (`audio: checkpoint WSA controller and FIFO boundary`). SP11 Linux was offline when this file was written, so these newest findings are preserved on SP7 but are NOT YET committed/pushed into `/home/geoca/Documents/SP11-PROJECT/01-audio-cps-review`.

Authoritative prior repo checkpoint:
- `docs/checkpoints/2026-08-19-WSA-CONTROLLER-105C-AND-FIFO-BOUNDARY.md`
- commit `28334d682fa86dc5e0696dd2ecac64a81509edd9`

Preserve Golden v31. Do not mutate Golden. Disposable one-shot candidates only.

## Decisive data-plane boundary remains

Windows native DATA_LOGGING:
- tap2 = real VI PCM, 8 kHz
- tap3 = real CPS PCM, 24 kHz

Corrected Golden Linux with forced logger:
- tap2/tap3 packetize but payload is all zero during acoustically proven speaker render.

Observed Windows startup timing from the latest timing work:
- CPS becomes live about 236 ms after stream start.
- VI becomes live about 741 ms after stream start.
- Both transitions precede the later 997 Hz tone interval, so the activation is stream-start/lifecycle driven, not tone/content driven.

## Newly closed lower-layer hypotheses

### qcaucd `+0x36510` WSA START owner is not a hidden AFE client

Exact decompilation shows `qcaucd+0x36510` acquires up to four resource objects (types 7/8/9/10), programs WSA controller fields, uses shared controller activation, and calls a final mode-1 callback.

Resolved helpers:
- `FUN_140031b48` = shared-user/reference counter plus controller activation via `FUN_14003f540`; not an AFE/DMA client creator.
- `FUN_140031bd0` continues into already-known WSA/SoundWire programming.
- final indirect callback vtable slot 5 resolves to `qcaucd+0x32e10` or `qcaucd+0x32860`.
- those functions are WSA8845 register/lifecycle handlers. `+0x32e10` is the function already captured as `AUCD_WSA_LIFECYCLE`.

Conclusion: the entire `qcaucd+0x36510` START-owner path collapses into already-accounted WSA/SoundWire/controller lifecycle. It does not contain a hidden AFE hardware-client/sample-attach operation.

Evidence:
- `qcaucd-start-resource-exact-20260819.txt`
- `qcaucd-lifecycle-32860-32e10-20260819.txt`
- prior `graph-birth-v3-structured.json`

### qcaudminiport first-start optional `+0xA8` method is not sample attachment

`qcaudminiport+0x75cd4` is a reference-counted stream lifecycle dispatcher. First START can perform an optional vtable `+0xA8` call before the known `+0x80` op4/qcaucd WSA create.

Vtable/decompile resolution shows `+0xA8` implementations are either:
- a no-op/trace stub, or
- timer arm/cancel logic.

For the relevant first-start path this is cleanup/timer behavior, not AFE/LPASS sample attachment.

Conclusion: do not build a candidate around qcaudminiport `+0xA8`.

Evidence:
- `qcaudminiport-upper-start-chain-20260819.txt`
- `qcaudminiport-preop4-a8-methods-20260819.txt`
- `qcaudminiport-7a4c0-xrefs-20260819.txt`

### qcadcm endpoint-resource activation has no SP11 endpoint components to enable

qcadcm contains:
- `AdcmEnableEndpointResources`
- `AUDIO_DSP_IOCTL_ACTIVATE_EP_RESOURCES`
- LPASS HW-core resource vote support
- endpoint component list handling via `MODULE_ID_ENDPOINT_COMPONENTS_CONFIG`

Earlier exact SP11 ACDB decode shows the endpoint-components payload is eight zero bytes:
- `IsLpi = 0`
- component count = 0

Therefore the endpoint-component portion has nothing to enable on SP11. The generic LPASS HW-core vote overlaps previously closed generic resource hypotheses.

Conclusion: endpoint-resource activation is not currently a justified Linux candidate.

Evidence:
- `qcadcm-endpoint-resource-xrefs-20260819.txt`
- `qcadcm-endpoint-resource-core-20260819.txt`
- `qcadcm-endpoint-components-handler-20260819.txt`
- earlier repo finding `docs/findings/2026-07-29-windows-audio-resource-exact-decode.md`

## New strongest lead: qcadcm post-GSL_CMD_START stream-active callback

qcadcm `SetStreamState` does more than issue graph START.

After `GSL_CMD_START`, qcadcm checks a registered client for the exact endpoint/graph/streaming-mode tuple. On the first matching active stream it sets `IsStreamActive = 1` and invokes a registered callback with a structure containing:
- `EpType`
- `GraphType`
- `StreamingModeType`
- `IsStreamActive`

This occurs after graph START and matches the sub-second window in which Windows tap3/tap2 become live.

This is distinct from generic SPF graph START and from the already-closed SoundWire/controller lifecycle.

Evidence:
- `qcadcm-setstreamstate-xrefs-20260819.txt`
- `qcadcm-streamactive-registration-xrefs-20260819.txt`

## Registrar attribution: qcasd8380.sys, not ordinary qcaudminiport path

Actual qcadcm AudioDsp interface GUID:
- `{A3F19141-A09D-40E5-860F-D0B2B27005FE}`

A full installed-driver fingerprint scan found this GUID only in:
1. qcadcm8380.sys itself
2. qcaudminiport8380.sys
3. qcasd8380.sys

The ordinary qcaudminiport hardware-interface path initially examined used a different GUID:
- `{EC873017-41C8-40C3-AA26-C44A5B008F20}`
That was the qcaucd/hardware side and explained its opcodes 0–12; do not confuse it with qcadcm AudioDsp.

qcaudminiport also has a real qcadcm AudioDsp manager, but its directly resolved first-level AudioDsp calls were opcodes such as `0x20–0x24`, `0x28–0x29`, etc. No concrete qcaud registration of `0x25` was found in the examined paths.

qcasd8380.sys DOES issue qcadcm AudioDsp opcode `0x25` through its wrapper:
- qcasd wrapper `FUN_1400306c0`
- thin wrapper `FUN_140025db0`
- concrete parent `FUN_140025c48` calls `FUN_140025db0(param_1, 0x25)`
- cleanup/teardown `FUN_14005e2e0` also issues opcode `0x25`

Therefore qcasd (Qualcomm ACX/static-endpoint driver) is the confirmed owner/registrar path for qcadcm AudioDsp event callback opcode `0x25`.

qcasd live binary copied from SP11 Windows:
- source: `C:\Windows\System32\DriverStore\FileRepository\qcasd8380.inf_arm64_0c64a81ab0252f11\qcasd8380.sys`
- SP7 evidence copy: `C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11-Feedback-Boundary-20260819\qcasd8380.sys`
- size: 435952 bytes
- SHA256: `10D6F600EF0D9E75B37C7B850EB5D3CE0EE622B31B15E303C859FDF0587262FA`

Evidence:
- `qcasd-audiodsp-guid-xrefs-20260819.txt`
- `qcasd-audiodsp-wrapper-20260819.txt`
- `qcasd-audiodsp-wrapper-callers-20260819.txt`
- `qcasd-direct-opcode-parent-decomp-20260819.txt`
- `qcasd-eventcb-register-functions-20260819.txt`
- Ghidra project: `SP11-Feedback-Boundary-20260819\ghidra-qcasd`

## Current unresolved seam — continue here

Do NOT build a Linux candidate yet.

The immediate task is to decode how qcasd stages qcadcm event-callback parameters around opcode `0x25`, including:
- exact callback function pointer
- callback context pointer
- event type
- watched `EpType`
- watched `GraphType`
- watched `StreamingModeType`
- registration vs deregistration semantics
- callback body when `IsStreamActive` changes to 1

qcadcm exposes multiple methods on the AudioDsp query interface, not only `AudioDspIoctl`; event parameters may be staged through a sibling method before opcode `0x25` is issued. The latest qcadcm interface decompile was produced in:
- `qcadcm-audiodsp-interface-methods-20260819.txt`

Do not misread qcasd's thin `0x25` wrapper as containing the full event payload; follow the interface method/data staging and the qcasd object fields that feed it.

If the resolved qcasd callback performs a private post-RUN AFE/feedback/source activation, port ONLY that missing operation into a disposable v31-derived Linux candidate.

Promotion gate remains:
- real nonzero tap2 at 8 kHz
- real nonzero tap3 at 24 kHz
- acoustically proven speaker render
- no DSP/kernel faults
- Golden v31 unchanged

## Safety / state

- Latest known persistent Linux GRUB saved entry remains `sp11-audio-golden-v31` from the previous verified checkpoint.
- Latest known canonical topology SHA256 remains `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d` from the previous verified checkpoint.
- SP11 Linux was offline at creation of this file, so those two items could not be re-verified in this exact moment.
- No new Linux candidate was built during this qcadcm/qcasd investigation.
- KD live session was not established; one attempt was rejected by the remote safety layer and was not bypassed. Static Ghidra analysis continued instead.

## Next persistence action when SP11 Linux returns

1. Copy this checkpoint into `docs/checkpoints/` in `/home/geoca/Documents/SP11-PROJECT/01-audio-cps-review`.
2. Add a concise finding doc if desired for the qcadcm/qcasd post-RUN callback boundary.
3. Commit on branch `agent/psycho-bass-20260818`.
4. Push to origin.
5. Verify local HEAD == remote branch hash and working tree clean.

## 2026-08-20 continuation: qcasd/qcadcm stream-active callback closure

### Exact qcadcm event registration contract
qcadcm AudioDsp opcode 0x25 accepts a 0x20-byte registration packet. For event type 1 it stores:
- EvtType
- Register flag
- callback pointer
- watched tuple {EpType, GraphType, StreamingModeType}
When the first matching stream transitions to RUN after GSL_CMD_START, SetStreamState increments a reference count and invokes the callback with a 0x10-byte event payload containing {EpType, GraphType, StreamingModeType, IsStreamActive}.

### Exact qcasd registrar
ARM64 assembly at qcasd8380.sys +0x25c48 proves qcasd registers callback qcasd+0x25a80 twice:
1. EvtType=1, Register=1, callback=qcasd+0x25a80, tuple {EpType=1, GraphType=1, StreamingModeType=1}
2. same except StreamingModeType=2
qcasd8380.sys SHA256: 10d6f600ef0d9e75b37c7b850eb5d3ce0ee622b31b15e303c859fdf0587262fa

### Callback body
qcasd+0x25a80 reads IsStreamActive from the fourth field of qcadcm's 0x10-byte event payload and calls qcasd+0x5f108(active).
qcasd+0x5f108 maps active -> worker command 10, inactive -> worker command 11 and queues it via qcasd+0x5f180.
qcasd worker qcasd+0x5f750 handles command 10 by setting internal stream-active flag +0x58=1, desired state +0x4c=2, then qcasd+0x5f4a8. Command 11 clears +0x58, evaluates qcasd+0x5f3f0 and may also call +0x5f4a8.

### Closure: not VI/CPS feed owner
The owning qcasd manager qcasd+0x5e640 explicitly loads SVA/listen-power configuration:
- DefaultSvaPowerType
- NumListenPowerTypes
- qcasd\\...\\Listen\\PowerType0/1
- SampleRate / SampleBitLength / NumChannels / EpLocnMask
The stream-active notification is therefore used to move an always-listening/SVA endpoint between power states when normal rendering becomes active. It does not expose a speaker-protection, WSA feedback, VI, or CPS sample-attachment action.

Conclusion: qcadcm post-GSL_CMD_START opcode-0x25 callback is a real Windows stream-RUN side effect and its timing overlaps the VI/CPS activation window, but its qcasd consumer is SVA/listen power management. CLOSE as root cause for missing Linux VI/CPS sample feed.

### New strongest boundary
HLOS-side paths now closed/accounted include:
- qcaucd op4 physical writes and WSA lifecycle
- qcaucd+0x36510 resource owner / WSA callbacks
- qcaudminiport first-start optional +0xA8 path
- qcadcm endpoint components activation (SP11 component count zero)
- qcadcm post-RUN qcasd opcode-0x25 callback (SVA/listen power only)

Next target: exact Windows DSP/GSL/GPR transactions immediately surrounding speaker graph RUN and the first nonzero CPS/VI frames. Compare against Golden Linux AudioReach graph START at message/parameter level, looking for a Windows-only DSP/AFE endpoint start, module property, or source attachment rather than further HLOS register guesses.
