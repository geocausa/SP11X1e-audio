# Linux speaker-protection parked mode — 2026-07-26

## Finding

The stock Linux AudioReach driver cannot place the Windows speaker-protection
modules in a graph without also configuring and enabling them.

For every `MODULE_ID_SPEAKER_PROTECTION` widget,
`audioreach_set_media_format()` sends:

1. `PARAM_ID_SP_OP_MODE` in normal mode; then
2. `PARAM_ID_MODULE_ENABLE = 1`.

For every `MODULE_ID_SPEAKER_PROTECTION_VI` widget, it sends:

1. SP_VI operating-mode configuration;
2. `PARAM_ID_SP_VI_CHANNEL_MAP_CFG`;
3. SP_VI excursion-mode configuration; then
4. `PARAM_ID_MODULE_ENABLE = 1`.

That sequence is not the recovered Windows startup sequence. Windows first
loads graph and tagged calibration, reads the live SP/SP_VI channel
structures, constructs the two-channel R0/T0 body, loads SP_VI-tag
calibration, configures the VI endpoint, and only then starts the graph. The
live Windows commands recovered so far do not contain Linux's automatic
SP_VI channel-map command at this point.

Adding SP and SP_VI widgets to a Linux topology without changing this behavior
would therefore turn an incomplete reconstruction into an active,
non-Windows protection path.

## Default-disabled evidence

The recovered Qualcomm AudioReach API defines
`PARAM_ID_MODULE_ENABLE` (`0x08001026`) as the common processing-module
enable parameter. Its documented values are:

- `0`: disable, the default;
- `1`: enable.

The recovered `sp_rx.h` and `sp_vi.h` module declarations both list this
common enable parameter as supported. This supports a conservative parked
state: instantiate the modules and their exact control link, but send no
Linux-generated protection setup or enable command.

This does **not** prove calibrated speaker protection, and it does not claim
that every internal algorithm state is equivalent to Windows. It establishes
only the safe structural baseline required before calibration parity work.

## Offline candidate

`patches/0004-audioreach-add-speaker-protection-bypass.patch` adds the opt-in
topology token:

```text
AR_TKN_U32_MODULE_SPEAKER_PROTECTION_BYPASS = 262
```

When the token is `1` on an SP or SP_VI module, the media-format path sends no
protection parameters and no enable command. The module remains in the
AudioReach API's default-disabled state.

The behavior is deliberately opt-in:

- topologies without the token keep the existing Linux automatic setup;
- the token changes only SP and SP_VI modules;
- no mixer, amplifier, VI feedback path, boot file, or live kernel is changed.

The token should be set on both Windows-derived SP11 widgets until the exact
calibration and activation sequence is implemented.

## Validation

The patch was checked in both supported construction orders:

1. directly against the preserved pristine Linux 7.1.5 source;
2. stacked after
   `0003-audioreach-add-topology-control-links.patch`.

Results:

- patch dry-run in both orders: pass;
- strict kernel style check: zero findings;
- ARM64 `audioreach.o` build with `W=1`, stacked after `0003`: pass;
- ARM64 `topology.o` build with `W=1`, stacked after `0003`: pass.

The candidate is not installed and has not touched the running audio stack.

## Consequence

The earlier driver-side blocker to representing the Windows root is now
closed as an offline candidate. A clean DEFAULT-mode topology model can
include SP, SP_VI, their exact `INTENT_ID_SP` control link, and the remaining
root modules while keeping protection parked.

Protection activation remains separately gated by exact calibration payload
selection, ordered runtime commands, muted hardware validation, and the
single-WSA VI transport observation.
