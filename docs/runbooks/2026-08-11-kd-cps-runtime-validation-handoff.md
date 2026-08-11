# Handoff: Windows KD after Linux CPS transport closure

Date: 2026-08-11 (Europe/London)

## Copy/paste instruction for the next agent

Continue the SP11 lower-level speaker-protection investigation from the
evidence in `geocausa/SP11X1e-audio`, branch
`agent/cps-parity-review-20260811`. Read this handoff completely, then read:

- `deploy/audio-cps-v3/DEPLOYMENT-PROVENANCE.md`;
- `docs/findings/2026-08-10-windows-cps-dp6-runtime-capture.md`;
- `docs/findings/2026-08-11-qcaucd-slot14-shared-master13-correction.md`;
- `docs/findings/2026-08-10-qcslimbus-max34417-cps-closure.md`;
- `docs/architecture/2026-08-11-windows-speaker-audio-path-evidence-bound.md`.

Do not rebuild Linux, change GRUB, restore the rejected split CPS masks, or
invent physical master port 14. Linux CPS transport is now runtime-closed.
Use `kd-mcp` as the only debugger owner and perform a read-only Windows capture
only for the remaining HLOS CPS payload/telemetry, PBR DP4, and physical
per-speaker questions described below. Preserve raw timestamped output before
interpretation, clear every breakpoint, and detach with `qd`.

## Current Linux result: accepted

The entry `sp11-audio-cps-v3` booted successfully on
`7.1.5-sp11-cps-v3+`. It is now the persistent GRUB default at the operator's
explicit request; Windows remains available in the menu.

The exact codec follow-up is patch
`patches/0041-ASoC-wsa884x-make-DT-CPS-boot-proof.patch`, produced from local kernel
commit `4c5c85f`. It fixes two runtime issues:

1. the SoundWire codec reads `qcom,enable-cps` and `qcom,cps-offset1` from its
   codec OF node, not the SoundWire-created primary firmware node;
2. DT-enabled CPS is fixed board wiring and cannot be disabled by a stale ALSA
   restore file.

The final boot deliberately retained CPS=`false` in
`/var/lib/alsa/asound.state`. Both live CPS controls nevertheless remained on.
Runtime evidence then proved:

- left/right WSA8845 CPS offsets `0` and `25`;
- playback DAI at 48 kHz using slave ports 1/2/3;
- VI feedback at 8 kHz using slave port 5;
- CPS feedback at 24 kHz using slave DP6, native mask `0x03`, on both codecs;
- successful SP and SPVI configuration/query, R0/T0 setup, endpoint
  calibration, VI+CPS enable, volume, MSIIR, channel mixer and `GRAPH_START`;
- real playback through the configured PipeWire Windows-Dolby default returned
  success, with zero DAI2-selection failures.

The signed codec hash is
`ccc9a4d1a3e0cc34e4761a0b4ddaebbbc152b822bb4f579dd512374e9fe4251e`.
The deployed initramfs hash is
`8504076a0f40926eee09233451b078d32da1fbb72bb52d4556bec528bd6e6153`.
The full installed tree passed 7,886-module/327,477-import CRC closure; the
extracted initramfs passed 2,910-module/128,311-import closure.

The earlier `sp11-audio-cps-parity-v2` mixed-ABI candidate remains rejected and
must not be booted. Its history is not evidence against the final V3 result.

Patch `0042-ASoC-wsa884x-add-bounded-live-status-observer.patch` adds a
default-off, read-only, 40-sample maximum observer to the existing 100 ms PA
health worker. Linux captured 12 samples from each amplifier during playback:
24/24 reads succeeded, both PAs stayed active, all fault/interrupt bytes were
zero, and the two amplifiers returned distinct changing raw ADC, temperature
and VBAT words. Both live `CURRENT_LIMIT` registers were `0x44`, proving the
PBR-enabled 2-cell current policy. Both local `CPS_CTL` registers remained
zero even though CPS DP6 and the DSP VI+CPS graph were active. Subsequent
static review found no `0x3468` scalar in the shipped `qcaucd8380.sys` and no
Linux WSA884x write to that register, so zero is not a missing-Linux-write
signal or a deployment blocker. Read
`docs/findings/2026-08-11-linux-cps-v3-live-wsa-observation.md` and its reviewed
JSON artifact before the KD session.

## What is still unknown

These are now evidence-quality questions, not Linux audio blockers:

1. **CPS HLOS hardware-interface payload.** Preserve the live
   `PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) bytes, or an equivalent
   qcaucd runtime object. Resolve the packed VBAT/temperature register
   addresses for both amplifiers plus the LPASS SoundWire write-command,
   read-command and read-FIFO physical addresses.
2. **CPS thresholds and calibrated/action telemetry.** Linux now captures
   distinct live per-amplifier raw ADC, temperature and VBAT words, but not
   their calibrated physical units or a nonzero limiter action. Determine
   whether Windows sends
   `PARAM_ID_CPS_LPASS_SWR_THRESHOLDS_CFG` (`0x08001254`) at runtime and find a
   passive response/event or object which exposes calibrated CPS measurements
   or protection state. Treat `CPS_CTL=0x00` only as an undocumented register
   observation unless new Windows evidence proves otherwise. Do not provoke
   thermal, over-current or speaker faults.
3. **PBR DP4 transport.** Linux has the recovered 2S PBR policy, 0x11 current
   limit and 15-step thresholds, and PBR affects amplifier register policy.
   It has not proved whether Windows also schedules WSA8845 sink DP4 as a
   SoundWire sideband during playback. Capture the relevant Windows master and
   slave DP4 state; do not infer transport merely from the PBR enable bit.
4. **Physical speaker binding.** Bind the two WSA8845 identities to physical
   left/right and normal render channel assignment. CPS offsets identify the
   two slave schedules but do not, by themselves, prove the audible channel
   location.
5. **Non-fatal DSP status.** Linux sometimes logs response status 3 for command
   `0x01001006` immediately before all required stages are accepted. Identify
   the Windows semantic equivalent if it naturally appears, but do not treat
   this as a failure or make it the primary KD objective.

MAX34417 is out of scope. Recovered ACPI labels its channels as platform
memory/Wi-Fi/CPU rails, live Linux probes NACKed the optional devices, and it
cannot identify speaker current or WSA protection engagement.

## Fixed Windows contract

- WSA SoundWire controller base: `0x06b10000`.
- CPS endpoint IID: `0x402b`.
- Format: 24 kHz, S32, two-channel mask `0x03`.
- WSA interface 3 / Linux `WSA_CODEC_DMA_TX_1`.
- One shared physical master port 13.
- Left/right slave DP6 OffsetCtrl1 values: `0` and `25`.
- Software slot 14 is the right-slave companion, not physical master port 14.
- `qcaucd8380.sys` owns the WSA speaker bus; `qcslimbus8380.sys` is the
  SLM1/Bluetooth path.

Hash-lock Windows binaries before using any RVA:

| Driver | Required SHA-256 |
|---|---|
| `qcadcm8380.sys` | `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429` |
| `qcaucd8380.sys` | `bd0c8276c51fc7a020c616e904dd613b6ccf187ec3e1fe6f94c2c811c8adc8bf` |

If either hash differs, stop and rebase the static analysis before using an
RVA breakpoint.

## Debugger rules

1. `kd-mcp` must be the only debugger owner. Never launch classic WinDbg at the
   same time.
2. Do not put the KDNET key or connection string in Git, transcripts or chat.
3. Open a persistent raw log before setting breakpoints.
4. Issue breakpoint commands individually. Do not pass a multi-line `.kd`
   script through `$$><` in this lab.
5. Use bounded logging breakpoints which immediately continue where possible.
6. Read only. Do not write MMIO, SoundWire registers, DSP payloads or driver
   state.
7. Before detach, break in once, run `bc *`, and detach with `qd`.

## Minimum capture

Use one scenario ID and record idle, active playback and post-stop timestamps.
At minimum preserve:

- `lmvm` identity and independent hashes for qcadcm/qcaucd;
- WSA active bank and master port 13 state in both banks;
- both WSA8845 identities and their DP6 state as a consistency check;
- both slaves' DP4 state and its matching master scheduling, if active;
- the full `0x08001259` payload/equivalent object;
- `0x08001254` or passive CPS measurement/state events if present;
- evidence that binds each amplifier identity to physical left/right.

The known generic capture boundaries remain qcadcm
`gsl_set_custom_config` RVA `0x60b78` and qcaucd physical-MMIO helper RVA
`0x1bf80`, but a miss at either boundary is not proof of absence. Follow the
actual hash-matched call path instead of guessing another Linux layout.

## Deliverables and acceptance

Return:

1. untouched raw debugger log;
2. command transcript and session metadata without transport credentials;
3. machine-readable decoded records for `0x08001259`, any `0x08001254` or CPS
   telemetry, master port 13, both DP6 slaves and any active DP4 path;
4. a concise reviewed finding separating observed bytes from inference;
5. SHA-256 hashes for every deliverable.

Store raw logs only in ignored local `artifacts/raw/` until checked for secrets
and unrelated private data. Commit reviewed/redacted extraction only. A capture
without the HLOS payload/equivalent object is partial, but it does not reopen
the already successful Linux CPS transport.
