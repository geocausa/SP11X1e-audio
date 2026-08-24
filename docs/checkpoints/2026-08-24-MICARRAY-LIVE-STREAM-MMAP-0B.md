# SP11 Windows MicArray ordinary stream start reaches GSL MmapDSP op 0xB, not codec op4

Date: 2026-08-24
Status: live KD observation / read-only
Branch: agent/microphone-re-20260824

## Scope

Follow-up to the static recovery of the MicArray1 `UseInternalCodec=1` qcasd subclass and its qcaucd private op4/op5 start/stop path.

The target was left in normal Windows runtime with two auto-continue probes armed:

- qcasd `+0x2df14`, immediately at the internal-codec qcaucd-op4 dispatch boundary;
- qcaucd `+0x4cba0`, private op4 body.

No PnP restart, qcasd/qcaucd restart, ACDB mutation, graph-selector mutation, or Linux mutation was performed.

An ordinary short WASAPI capture was requested via the already-existing `Record-WindowsMicDefault.ps1` script.

## Live result

The user-mode connector dropped during the capture attempt, concurrent with the already-observed Qualcomm WLAN unload sequence in KD:

- `netadaptercx.sys`
- `WifiCx.sys`
- `qcwlanhmt8380.sys`

Neither of the two armed internal-codec probes emitted a runtime hit.

Instead KD stopped at:

`qcadcm8380+0x61578` = recovered `FUN_140061578` / `gsl_ioctl` entry.

Live registers:

```
x0 = 0x000000000047534c
x1 = 0x000000000000000b
x2 = ffffed8e848fa5d0
x3 = 0x20
x4 = 0x264
x5 = 0x11710
lr = qcadcm8380+0x8c910
```

So the live GSL operation is **0xB**, not case 5 (`GSL_CMD_ADD_GRAPH`).

Input at `x2`, 0x20 bytes:

```
00000f00 00000001 00000000 00000000
0000000b 04000002 00000000 00000000
```

Representative stack:

```
qcadcm8380+0x61578
qcadcm8380+0x8c910
qcasd8380+0x307b4
qcasd8380+0x27828
qcasd8380+0x26ee8
qcasd8380+0x2434c
qcasd8380+0x416b4
Acx01000!Acx::AfxRtStream::EvtRtAudioBufferCallback+0x3f4
Acx01000!Acx::AfxHelper::DispatchProperty+0x138
Acx01000!Acx::AfxStream::DispatchRequest+0x148
...
```

## Static correlation

The already-recovered qcadcm `AudioDspGraphOpen` decompile shows the relevant call site under the explicit log/function label `MmapDSP`:

```
uVar31 = 10;
if (*piVar34 != 1) {
    uVar31 = 0xb;
}
FUN_140061578(..., uVar31, local_190, 0x20, ...);
```

The surrounding log strings are:

- `MmapDSP`
- `gsl_ioctl:%d failed:%s`
- `failed MmapDSP:0x%x`

Thus qcadcm GSL operations `0xA/0xB` here are the DSP buffer mapping path, with 0xB selected for this live stream family.

## Consequence

An ordinary default-WASAPI MicArray open is **not** a reliable trigger for:

- qcasd internal-codec op4;
- qcaucd physical hardware-resource start;
- qcadcm `GSL_CMD_ADD_GRAPH` case 5;
- ACDB `AcdbCmdGetGraph` for a fresh graph.

Instead the ordinary recorder path reaches the ACX runtime stream callback and maps/attaches buffers to an already-resident graph using GSL operation 0xB.

This strengthens the cached/resident-graph model and explains why repeated ordinary recordings do not reproduce graph-birth or physical-codec initialization hooks.

It also means the qcasd `UseInternalCodec=1` op4 path recovered statically must occur at an earlier endpoint/device/graph lifecycle transition than ordinary WASAPI stream attachment.

## Constraints preserved

- Windows observation only; no selector or ACDB runtime patch.
- No reboot.
- No PnP/device restart during this observation.
- Linux golden v33 untouched.
- Windows bootdebug remains enabled for the eventual graph-birth capture phase.
