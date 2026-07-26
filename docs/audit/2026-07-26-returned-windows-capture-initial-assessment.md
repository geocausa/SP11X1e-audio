# Initial Returned Windows Capture Assessment — 2026-07-26

> Historical checkpoint. A later USB return on the same date supplied the
> missing ETLs, WASAPI probes, and eight additional state snapshots. See the
> current second-return assessment. The packet-label corrections and warnings
> about generated KD narrative below remain valid.

## Decision

The returned material contains useful Windows baseline evidence and three real
kernel packet samples, but it is **not a complete topology capture**. It must
not be promoted to the canonical Windows/Linux graph ledger as proof of graph
structure, module order, or scenario-dependent behavior.

The most valuable immediate discovery was not in Gemini's three reports. A
genuine target-side cold-boot snapshot remained on the Surface Pro 11 Windows
partition under `C:\SP11-PARITY-OUTPUT`. It has been copied byte-for-byte into
`artifacts/raw/windows-target-20260726/` and protected with a SHA-256 manifest.

Gemini's narrative reports are preserved unchanged under
`artifacts/raw/windows-kdnet-20260726-gemini/`. Their claims were reviewed
against their included bytes, recovered Qualcomm/AudioReach definitions, the
target snapshot, and recovered ACPI.

## What was actually returned

### Target-side raw evidence

Only one collection stage completed:

- `cold_boot_idle`, collected at `2026-07-26 00:01:15`
- 13 files containing session metadata, qcadcm version lock, services and
  processes, `audiodg.exe` modules, module hashes, devices, signed drivers,
  Dolby packages, event logs, endpoint registry, Dolby registry, Windows audio
  registry, and a CLSID-resolution dump

No later scenario snapshots were present. There was no final snapshot, WPR
ETL, scenario-notes file, screenshot collection, dry-run probe output, or live
probe output in either `C:\SP11-PARITY` or `C:\SP11-PARITY-OUTPUT`.

### Returned KD narrative

`sp11_audio_parity_captured_telemetry.md` says all six boundaries and all
scenario data were captured. The document itself contains only:

- one completed GET_CFG buffer;
- three outbound packet blocks;
- no ACDB selector vector;
- no SET_CFG buffer;
- no OOB GRAPH_OPEN body;
- no inbound response block;
- no scenario marker, timestamp correlation, or full debugger transcript.

The internal counts cannot be audited from the report. For example, the table
claims three completed GET_CFG buffers but only one sample is included, with
reported index 7. The selector, SET_CFG, and OOB graph bodies are described as
"verified" without any underlying bytes.

The package's own structural checker therefore correctly evaluates the
returned report as `INCOMPLETE_OR_REVIEW_REQUIRED`.

## Verified facts worth retaining

### Windows version and driver lock

- Windows 11 Home `10.0.26200`, ARM64.
- `qcadcm8380.sys` version `1.0.0.7966`.
- File length `768224` bytes.
- SHA-256
  `37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429`.
- The target collection's expected-hash comparison returned `True`.

This is an important lock: the packet RVAs and the static qcadcm analysis refer
to the intended binary.

### Active Windows speaker endpoint at cold idle

The active render endpoint was
`{5bb689e6-2c6b-4357-b4c1-beb815638f88}`, with `DeviceState = 1`.
Its registry identifies:

- `Qualcomm(R) Aqstic(TM) Audio Adapter Device`;
- `AUCD\VEN_QCOM&DEV_0C29&SUBSYS_MSHW0486&REV_0D`;
- the speaker pin `wavespeaker`;
- the Surface profile `SurfaceAPO_0D.json`;
- Dolby software-device registrations for GUIDs `0EBD8605`, `0EBD8606`,
  `0EBD8611`, and `0EBD8612`;
- Surface APO GUID `34D30CD8-370E-4229-85BE-3346C594C805`.

The composite effect lists recorded by Windows are:

| Endpoint property | Registered effect GUIDs |
| --- | --- |
| `d04e...,13` | `0EBD8605...` |
| `d04e...,19` | `0EBD8611...` |
| `d04e...,14` | `0EBD8606...`, `34D30CD8...` |
| `d04e...,20` | `0EBD8612...`, `34D30CD8...` |

These lists establish registration/association. They do not by themselves
establish runtime instantiation, bypass state, or processing order.

### Cold-idle user-mode module baseline

At the genuine cold-idle snapshot, `audiodg.exe` PID 11388 had these relevant
modules loaded:

| Module | Version | SHA-256 |
| --- | --- | --- |
| `DolbyAPOvlldp150.dll` | `3.30704.742.0` | `A2553FF7B013B5A248E50BDCAE46D08405E393C0085073975214D035CEDF02C1` |
| `DolbyApoVr.dll` | `3.30704.742.0` | `1D74477EA0DAE66961A21BF6BC3CE0D8062836FC4DD96B59C14DE11257F5EECC` |
| `DolbyDax3Apo.dll` | `3.30704.742.0` | `6EA1702C0F86766E45C2E248E169022E3D71EAA3C655B3FCA159B4DD59F18D87` |
| `SurfaceAPO.dll` | `1.216.42.0` | `AA3A97E2CC7740CE3BD6B80B154354A023170D3EF29992978E36C179550A5206` |
| `VirtualSurroundApo.dll` | `10.0.26100.8115` | `8AE87E55F69EAE4C1EDDA186C3F91AD55B4F4177BA3A3752B1ACEA91E255B739` |

The `DolbyDAXAPI` service was running, as were `audiodg.exe` and two
`DAX3API.exe` processes.

This genuine snapshot does **not** show `DolbyAudioProcessing.dll`,
`DolbyHrtfEnc.dll`, `voiceclarityapo.dll`, `SNPE.dll`, `libcdsprpc.dll`, or
`SnpeHtpV73Stub.dll` in that `audiodg.exe` instance. Their appearance in
`extra-capture.md` may have come from a later process/scenario, but the raw
module output needed to verify that claim was not returned.

Loaded DLLs are presence evidence only. They do not prove a DLL's APO was
instantiated or that its processing path was active.

## Corrected packet interpretation

The three packet blocks contain real and useful bytes. Two of their labels are
wrong.

### Packet 7 — endpoint GET_CFG

- Opcode `0x01001007`: `APM_CMD_GET_CFG`.
- Source `0x2010`, destination `0x4675`.
- Module instance ID `0x4675`.
- Parameter `0x080011B4`: `PARAM_ID_HW_EP_GET_CFG`.
- Parameter size 20 bytes.
- Completed caller buffer reports status success and begins with value 2,
  followed by zeros in the shown sample.

This is valid evidence, but not a new topology fact. The exact packet form
already exists in recovered June gate traces.

### Packet 10 — endpoint-resource enable, not graph close

- Opcode `0x0100100F`: `PRM_CMD_REQUEST_HW_RSC`.
- Parameter `0x080014F3`: the qcadcm custom DSP-GPIO resource packet.
- GPIO count 2.
- Records:
  `[10,1,1,0,1,0]` and `[11,1,1,0,1,0]`.

The opcode name is confirmed by Linux AudioReach PRM definitions. The custom
parameter and record layout independently match the recovered
`AdcmAudioHwRscIoctl` static analysis. This is useful live confirmation of a
specific qcadcm endpoint-resource enable payload. It is **not**
GRAPH_CLOSE or GRAPH_STOP.

### Packet 11 — hardware-resource release, not memory unmap

- Opcode `0x01001010`: `PRM_CMD_RELEASE_HW_RSC`.
- Parameter `0x08001032`: `PARAM_ID_RSC_HW_CORE`.
- Parameter size 4.
- Hardware block ID 1.

This is a PRM hardware-core resource release. It is **not** a shared-memory
unmap packet.

These corrections mean the returned KD samples confirm part of endpoint
resource bring-up/tear-down, but reveal no graph module list, subgraph list,
module connection list, graph management lifecycle, or scenario-dependent
topology.

## Review of `extra-capture.md`

Treat this file as a lead index, not as a canonical architecture description.

Potentially useful leads:

- the qcadcm base address and WDF context references may help relocate a future
  live session;
- the claimed `audiodg.exe` module names tell us what to look for in a
  scenario-bound module capture;
- the ADSP PnP identity is broadly consistent with the recovered driver
  package.

Unsupported conclusions:

- a loaded DLL does not prove APO instantiation or execution order;
- `SNPE.dll`/FastRPC presence would not alone prove active NPU audio offload;
- the drawn `audiodg -> WUDFRd -> WDF -> qcadcm` path is not established by
  the supplied output;
- the `qcadcm` device stack is not the complete Windows PCM processing graph;
- the narrative assigns semantics to multiple modules without call, COM/APO,
  ETW, or configuration evidence.

The claimed image sizes for several modules also do not agree with the exact
recovered binaries, so the missing raw module command output is required
before those addresses or process associations can be relied on.

## Review of `power_rails.md`

This document is not topology evidence and its audio conclusions must be
quarantined.

One decisive error is the claim that `PA05` is at I2C address `0x14` and
monitors the smart audio amplifiers. Recovered SP11 ACPI says:

- device `PA05` has HID/CID `MAX34417`;
- its `_CRS` resource encodes I2C address `0x12`;
- its named measurements include `MAINMEM_VDD2H`, `MAINMEM_VDD2L`, `WIFI`,
  `PMI_CPU_CX`, and `PMI_CPU_APC0`;
- it contains no smart-amplifier measurement name.

The address table in `power_rails.md` is also shifted for PA03 through PA05.
No audio-driver decision should be based on its proposed PA05 mapping or its
PMIC register narratives without the underlying raw trace and an independent
ACPI/driver confirmation.

## Capture-workflow correction

The initial assessment incorrectly suggested searching the SP7 for a classic
WinDbg log. Operator clarification established that:

- the SP7 session used `kd-mcp`, not the supplied classic-WinDbg launcher;
- classic WinDbg and `kd-mcp` cannot own this target simultaneously and doing
  so crashes or destabilizes the target;
- the handoff required target-side PowerShell, WPR, UI, probe, and note-taking
  actions on the SP11 while the operator was physically conducting the
  debugger session from the SP7;
- therefore `start-host-kdnet.ps1` never created its planned log, and only the
  first target-side baseline stage completed.

There is no reason to keep searching for
`C:\SP11-PARITY\output\sp11_windows_audio_parity_*.log`, ETL, scenario notes,
or probe logs from this run. Those files were not produced. Their absence is a
capture-design failure, not an operator omission.

The original handoff was not executable by one operator in this laboratory
configuration. It combined a WinDbg-only host launcher, manual target actions,
and two-machine scenario coordination despite the known exclusive `kd-mcp`
debugger ownership. The handoff should not be reused.

Any future live capture must:

1. use `kd-mcp` as the only debugger owner;
2. persist raw debugger output from inside the `kd-mcp` workflow, rather than
   depend on a second WinDbg instance;
3. install an autonomous, pre-staged target scenario runner before reboot, so
   the SP11 does not require an operator at its keyboard during host control;
4. use deterministic target-side timestamps and scenario IDs that can be
   matched to host-side markers;
5. collect and package its outputs automatically; and
6. fail closed if the raw bytes for a required boundary were not saved.

No repeat capture is being requested now. The recovered QGPR, KD, ETW, ACDB,
process-dump, and static-analysis corpus must be exhausted first.

## Repository disposition

- Raw target snapshot: preserved unchanged with manifest.
- Gemini reports: preserved unchanged with manifest.
- Machine-readable reviewed extraction:
  `artifacts/reviewed/windows-kdnet-20260726-gemini/reviewed-evidence.json`.
- Canonical graph ledger: unchanged.
- Linux topology/driver: unchanged.
- Git commit/push: deliberately deferred; the capture does not yet justify a
  topology implementation change.
