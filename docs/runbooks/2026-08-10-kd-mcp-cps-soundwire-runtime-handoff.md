# KD-MCP handoff: capture the Windows CPS/WSA runtime boundary

Date: 2026-08-10 (Europe/London)

## Copy/paste instruction for the next agent

> Work in the SP11 audio repository and read
> `docs/runbooks/2026-08-10-kd-mcp-cps-soundwire-runtime-handoff.md` completely.
> Execute that KD-MCP capture against the Windows SP11 using `kd-mcp` as the
> only debugger owner. Hash-gate qcadcm/qcaucd, persist the raw log, capture
> idle/active/post-stop WSA master port 13 in both banks, and resolve both
> WSA8845 slave DP6 configurations or the complete runtime `0x08001259`
> payload. A master-only dump is partial and must not lead to another Linux
> transport guess. Clear all breakpoints and detach with `qd`, then return the
> deliverables and an evidence/inference-separated finding.

## Task for the next agent

Use one controlled Windows `kd-mcp` session to determine the exact per-speaker
CPS SoundWire transport used by Surface Pro 11. Do not change Linux, build a
new GRUB candidate, or guess another channel mask during this task.

The capture is complete only if it binds the WSA master port-13 configuration
to the two individual WSA8845 slave identities and preserves either the
runtime `PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) payload or an equivalent
complete per-slave DP6 configuration.

A master-port dump by itself is not success.

## Read first

- `docs/findings/2026-08-10-qcslimbus-max34417-cps-closure.md`
- `docs/deployment/2026-08-10-audio-cps-lab-candidate.md`
- `docs/findings/2026-08-10-cps-transport-reconstruction.md`
- `docs/runbooks/windows-kdnet-structural-gap-capture.md`
- `tools/kdnet/capture-structural-gaps.kd` for command style only; do not run
  its unrelated old breakpoint set for this capture.

Relevant recovered header:

`00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/Research_Hub_Audio/SOURCE/audioreach_src/audioreach-graphservices/spf/api/modules/sp_rx.h`

## Facts already established

- `qcslimbus8380.sys` is SLM1/Bluetooth and is not the speaker bus driver.
- `qcaucd8380.sys` owns the WSA SoundWire side; WSA master MMIO begins at
  physical `0x06b10000`.
- CPS CODEC_DMA_SOURCE instance `0x402b` is 24 kHz, S32 fixed-point, two
  channels/mask `0x3`, LPAIF WSA interface index 3. Linux maps that endpoint to
  `WSA_CODEC_DMA_TX_1` (`0xb003`).
- WSA master port 13 independently has a 24 kHz interval.
- Linux split masks `0x1`/`0x2` clashed. Left-only mask `0x1` also clashed.
  Left-only native mask `0x3` was clean in one bounded test. This does not
  establish the Windows two-speaker layout.
- ACDB contains neither `0x08001259` nor threshold parameter `0x08001254`.
  The recovered header says HLOS supplies `0x08001259` at runtime.
- MAX34417 is unrelated platform-rail monitoring and is out of scope.

Hash-lock the Windows binaries before using any RVA:

| Driver | Required SHA-256 |
|---|---|
| `qcadcm8380.sys` | `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429` |
| `qcaucd8380.sys` | `bd0c8276c51fc7a020c616e904dd613b6ccf187ec3e1fe6f94c2c811c8adc8bf` |

If either hash differs, stop. Rebase the static analysis before setting an
RVA breakpoint.

## Debugger safety rules

1. `kd-mcp`/`kd.exe` must be the only debugger owner. Never launch classic
   WinDbg concurrently; competing KDNET owners can crash or freeze the SP11.
2. Obtain the current connection string from the operator or secure lab
   configuration. Do not place its key in Git or the handoff result.
3. Open a persistent debugger log before arming breakpoints. Raw output is the
   primary evidence; copied excerpts are not a substitute.
4. Issue breakpoint commands individually through `kd-mcp`. In this lab,
   passing a `.kd` file through `$$><` can collapse lines and corrupt commands.
5. Prefer logging breakpoints that immediately `gc`. Avoid repeatedly breaking
   the whole machine while audio is starting.
6. Before detach: break in once, run `bc *`, then `qd`. Do not use plain `q` in
   kernel mode, because it can leave the target frozen.
7. Do not write MMIO, WSA slave registers, driver state or DSP payloads.

## Capture phases

Use one scenario ID in every marker and filename, for example
`CPS_SWR_YYYYMMDD_HHMMSSZ`.

### 1. Establish identity and logging

Record, before playback:

- Windows build and boot time;
- `lmvm qcadcm8380` and `lmvm qcaucd8380`;
- independently verified on-disk hashes;
- debugger connection time and scenario ID;
- the exact commands/breakpoints armed.

Open the debugger log from inside the `kd-mcp` workflow. Do not depend on a
second WinDbg instance or an operator copying the console later.

### 2. Capture an idle WSA baseline

Use read-only physical dword dumps. The controller base is `0x06b10000`.

Record:

- master current-bank status: `0x06b1104c` (bit 0 is the active bank);
- WSA enumerator/device-ID window beginning at `0x06b10530`;
- port 13 bank 0:
  `0x06b11d24`, `0x06b11d28`, `0x06b11d2c`, `0x06b11d30`,
  `0x06b11d34`, `0x06b11d38`, `0x06b11d3c`;
- port 13 bank 1:
  `0x06b11d64`, `0x06b11d68`, `0x06b11d6c`, `0x06b11d70`,
  `0x06b11d74`, `0x06b11d78`, `0x06b11d7c`.

Label the dump `CODEX_CPS_SWR_IDLE`. Reading both banks is required; do not
assume the current bank from a prior session.

### 3. Arm generic runtime boundaries

The hash-locked `qcadcm8380.sys` function at RVA `0x60b78` is the recovered
`gsl_set_custom_config` entry. ARM64 `x1` is the original payload pointer and
`w2` is its size before the in-band/OOB split. Use a bounded logging breakpoint
to preserve payloads through `0x4000` bytes during the scenario. The raw log
must be searchable for little-endian parameter IDs:

- `59 12 00 08` (`0x08001259`);
- `54 12 00 08` (`0x08001254`);
- `37 15 00 08` (`INTENT_ID_CPS`, `0x08001537`).

Do not conclude that HLOS omitted `0x08001259` merely because this SET_CFG
entry does not see it. The parameter can cross a query/event or response path.
If absent, continue into the qcaucd path and identify the actual boundary.

The hash-locked `qcaucd8380.sys` routine at RVA `0x1bf80` is a generic
32-bit physical-MMIO helper with the physical address in `w0`/`x0`, a data
pointer in `x1`, and read/write selector in `w2`. A bounded logging breakpoint
may record requests in the `0x06b10000..0x06b15000` WSA range. It is supporting
evidence only: no hit does not prove that qcaucd avoids the controller, and an
entry hit records the request before a read result is stored.

Do not arm `qcslimbus8380.sys`; it is the Bluetooth path.

### 4. Run exactly one protected speaker scenario

With breakpoints armed and logging active:

1. mark `CODEX_CPS_SWR_PLAYBACK_BEGIN` with UTC time;
2. start ordinary Windows speaker playback through the normal effects path;
3. allow the protected graph and both amplifiers to reach steady state;
4. break in once while audio is active;
5. repeat the bank-status, enumerator and both port-13 bank dumps;
6. mark `CODEX_CPS_SWR_PLAYBACK_ACTIVE` with UTC time;
7. continue, stop playback normally, then capture one post-stop snapshot;
8. mark `CODEX_CPS_SWR_PLAYBACK_END`.

If target-side playback cannot be automated before debugger control begins,
ask the operator for only the start/stop action at explicit markers. Do not ask
them to transcribe debugger output.

### 5. Resolve the slave side

This is the decisive phase. Use the runtime payload, qcaucd state or traced
SoundWire commands to bind **each** enumerated WSA8845 identity to slave DP6.
For left and right separately preserve:

- SoundWire device identity/logical device number;
- DP6 direction and enabled channel mask;
- sample interval/rate;
- offset 1 and offset 2;
- word length and block packing;
- active bank;
- corresponding master port/data-port assignment;
- packed VBAT and temperature register addresses supplied to CPS.

Also preserve the LPASS SoundWire write-command, read-command and read-FIFO
physical addresses from `0x08001259` when present.

If the public parameter never appears, the equivalent complete qcaucd runtime
structure is acceptable only when its field interpretation is backed by raw
bytes, call context and both slave identities. An unlabeled memory dump is not
enough.

## Acceptance gate

Success requires all of the following:

- raw persistent debugger log with identity, commands and timestamps;
- matching driver hashes;
- idle, active and post-stop master bank status;
- both bank snapshots for master port 13;
- both WSA slave identities;
- per-slave DP6 configuration or a captured/decoded `0x08001259` payload;
- an evidence-backed explanation of how two speakers avoid driving the same
  SoundWire slots;
- breakpoint cleanup and clean `qd` detach.

If any of these is missing, report a partial capture. Do not recommend a new
Linux mask, allocator patch or reboot candidate.

## Deliverables

Return:

1. the untouched raw debugger log;
2. a command transcript and session metadata file;
3. a machine-readable decoded record for master port 13 and both slave DP6
   configurations;
4. a concise reviewed finding that distinguishes observed bytes from inferred
   field meanings;
5. SHA-256 hashes for every deliverable.

Keep raw logs in the repository's ignored local `artifacts/raw/` area until
they have been checked for transport credentials or unrelated private data.
Commit only the reviewed extraction, hashes and redacted command transcript.

The next Linux implementation decision is out of scope for the KD session.
It begins only after this acceptance gate passes.
