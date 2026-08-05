# Dolby integration map — binaries, imports, entry points (2026-08-01)

> **HISTORICAL / PARTIALLY SUPERSEDED (2026-08-05):** retain this file for binary/import provenance. Its architecture predates the Aug-4 live DAX/VLLDP/VR callback recovery and later runtime-map work. Use `docs/audit/2026-08-05-CANONICAL-DOLBY-PIPELINE.md` for the current model.

Built by direct inspection of the shipped ARM64 PE binaries with `objdump`,
not from prior notes. Every claim here is reproducible with the commands shown.

---

## 1. The binaries

All present at
`00-RE-archive/.../OPENCODE/source_copies/SOURCE/Dolby/SpeakerDLLs/`
(a second copy of most lives under `SOURCE/Dolby/RPC_Reversal/source/`, which
also holds `DAX3API.exe`, `CaptureStreamMonitor.dll`, and older APO versions
`DolbyAPOvlldp`, `...120`, `...130`, `DolbyAPOv251`).

| Binary | Size | Role |
|---|---:|---|
| `DolbyAPOvlldp150.dll` | 1,895,656 | **the live dynamic processor (VLLDP)** |
| `DolbyDax3Apo.dll` | 1,323,016 | DAX3 APO wrapper / MFX entry |
| `DolbyApoVr.dll` | 3,262,800 | virtualizer / VR path |
| `DolbyAPOv2100.dll` | 2,325,936 | older APO variant |
| `Dax3Ref.dll` | 833,320 | DAX3 reference implementation |
| `Dax3DapControl.dll` | 539,112 | DAP control / parameter setters |

All are `pei-aarch64-little`, i.e. **native ARM64 code that executes directly on
this hardware**. No emulation is required to run the instructions.

`DolbyAPOvlldp150.dll` entry point: `0x18004a940`.

---

## 2. Imports — the key finding

**Correction to an earlier assessment in this project.** It was previously
stated that porting would require reimplementing `audioeng.dll`, the Windows
audio engine. That is wrong.

`DolbyAPOvlldp150.dll` imports **one** function from `audioeng.dll`:

```text
audioeng.dll     AERT_Free                          (1 function)
```

`AERT_Free` is an allocator free helper, not the APO framework. The same is
true of `DolbyAPOv2100`, `DolbyApoVr` and `DolbyDax3Apo`: `AERT_Free` is their
only `audioeng` import.

Full import surface of `DolbyAPOvlldp150.dll`:

```text
KERNEL32.dll     MultiByteToWideChar
USER32.dll       CharNextW
ADVAPI32.dll     EventWriteString                   (ETW logging)
ole32.dll        StringFromGUID2
audioeng.dll     AERT_Free
AVRT.dll         AvRevertMmThreadCharacteristics    (MMCSS)
RTWorkQ.DLL      RtwqPutWorkItem                    (real-time work queue)
OLEAUT32.dll     13 ordinal-only imports (BSTR/VARIANT family)
```

Reproduce with:

```sh
objdump -p DolbyAPOvlldp150.dll | sed -n '/The Import Tables/,/The Export Tables/p'
```

Total named imports per binary (excluding OLEAUT32 ordinals): 4–7. These are
**small, stubbable surfaces**, not subsystem dependencies.

`Dax3DapControl.dll` and `Dax3Ref.dll` do not import `audioeng.dll` at all.

---

## 3. Exports — all four are COM

Every processing DLL exports only the COM server quartet:

```text
DllCanUnloadNow
DllGetClassObject
DllRegisterServer
DllUnregisterServer
```

There is **no exported processing entry point**. Anything that wants the
algorithm must either go through COM class activation, or call an internal
address directly.

This is why the existing prototype uses a PE loader
(`sp11_vlldp_pe_loader.h`) and calls `FUN_18001f7a8` — the main processor,
identified by prior RE with all 17 callees documented — rather than trying to
instantiate the APO.

---

## 4. Windows integration chain (from the KDNET workbook)

Proven call path, `00_WORKBOOK/INDEX.md`:

```text
AUDIODG!CAudioPump::OutputPumpWorkRoutine
  -> audiokse
  -> NtDeviceIoControlFile, IOCTL 0x2f0003
  -> ksthunk
  -> ks!KspPropertyHandler
  -> portcls!CPortPinWaveRT
  -> qcaudminiport8380
  -> qcadcm8380
```

Dolby activation path:

```text
AUDIODG graph/APO activation
  -> audioeng ASAR / CAdaptiveSpatialAudioRenderer
  -> DolbyAudioProcessing!DllGetActivationFactory

AUDIODG CAPOWrapperSrv::CreateSystemEffect
  -> CoCreateInstance
  -> DolbyDax3Apo!DllGetClassObject
  -> DolbyAPOvlldp150 / DolbyApoVr
```

Supporting user-mode components:

```text
DAX3API.exe                 Dolby control service and RPC layer
CaptureStreamMonitor.dll    watches playback sessions, selects profile
                            (browser playback maps to the "dynamic" profile)
```

Core identifiers:

```text
IOCTL          0x002f0003
KS GUID        a855a48c-2f78-4729-9051-1968746b9eef
audiodg PID    3570 (in capture)
```

---

## 5. What a port actually needs

Revised assessment given the import data:

| Requirement | Difficulty |
|---|---|
| Run ARM64 code natively | none — same architecture |
| Map PE image, apply relocations | done — `sp11_vlldp_pe_loader.h` exists |
| Stub `AERT_Free` | trivial — free() |
| Stub `MultiByteToWideChar`, `CharNextW`, `StringFromGUID2` | easy |
| Stub `EventWriteString` (ETW) | trivial — no-op |
| Stub `AvRevertMmThreadCharacteristics` | trivial — no-op |
| Stub `RtwqPutWorkItem` | needs care if the processor defers work |
| OLEAUT32 ordinals (13) | need identifying; BSTR/VARIANT helpers |
| Reach the processing function | done — `FUN_18001f7a8` located |
| **Seed internal state correctly** | **the real remaining problem** |

The last row is where the prior effort stopped. Outputs 275–286 are all
warmup / cold-init oracle / B-array gain-source work, and
`286_vlldp_exact_barray_gain_source_linux_validation_20260617.md` was still
mid-validation when the work ended.

Processing gate: `child1+0xc6c` — `0` = bypass, `1` = live. Correlation with
Windows' `DisableSysFxPkey` is proven (output R6 in `00_STATUS.md`).

---

## 6. Status of the existing prototype

`SOURCE/Dolby/Prototype/sp11-processor/`

```text
sp11_vlldp_pe_loader.h              C PE loader for the native ARM64 bridge
sp11_vlldp_exact.c                  exact native-bridge processor
sp11_vlldp_exact_ladspa_smoke.c     LADSPA smoke harness
sp11_vlldp_v19.c                    APPROXIMATE FFT/bin prototype - not exact
sp11_vlldp_fun18001de90_cdb_vectors.h   1.5 MB of test vectors
sp11_full_chain_core_test.c         whole-chain test against native output
sp11_vlldp_v8_runtime_contract.h    generated Windows boundary contract
```

Verified deployment (output 284, 2026-06-16): the exact plugin was built,
installed to `~/.local/lib/ladspa/sp11_vlldp_exact.so`
(sha256 `8ddc6cec...`), loaded as a **separate** PipeWire/Pulse LADSPA sink,
and played a 997 Hz tone without crashing. It was never made the default sink.

Its own conclusion:

> "This test proves real Linux graph loadability and basic playback survival.
> It does not prove Windows-exact live orchestration/gain-source equivalence."

---

## 7. The final project verdict (output 287, 2026-06-17)

> "Enough evidence exists to continue the Linux VLLDP live-processor path now.
> Not enough evidence exists to claim a complete, byte-exact Windows speaker
> chain."

Named missing evidence:

1. qcadcm/GSL/APM runtime OOB body bytes, handles, GKV mapping, and final order
2. VLLDP Phase-6 B-array / `FUN_180023d20` input coverage across cold/warm/
   profile transitions

---

## 8. The unresolved architectural question

Even a perfectly running VLLDP produces coefficients that must reach the DSP's
MSIIR modules. The KDNET workbook found that `0x08001022` (MSIIR coefficients)
**never appears in any Windows capture**. So how Windows delivers them is not
established.

Two possibilities, not distinguished by current evidence:

* Dolby processes samples in `audiodg` and the DSP MSIIR stays flat, or
* Dolby injects coefficients through a path not covered by the four
  instrumented breakpoints.

This matters: a correct port could still produce nothing audible if the output
has nowhere to go. The runtime injection control added 2026-08-01
(`SP11 MSIIR Inject`, verified `rc=0` on both MSIIR instances) provides a
delivery mechanism if the first possibility is wrong.

---

## 9. Measured Windows behaviour (the acceptance target)

From `SP11_DOLBY_LINUX_RECONSTRUCTION_PLAN_20260518.md`, loopback measurements:

```text
1 kHz reference        +8.01 dB
55 Hz bursts           +6.01 dB, peak-limited
90 Hz bursts           +6.91 dB, peak-limited
140 Hz bursts          +6.08 dB, peak-limited
75 Hz at -30 dBFS     +16.82 dB
75 Hz at -12 dBFS     +10.25 dB
loud bass              hard-limited near -0.13 dBFS
```

Level-dependent bass recovery of roughly 6.5 dB between -30 and -12 dBFS, with
a limiter at -0.13 dBFS. Any port can be validated against these numbers
without needing byte-exactness.

Note also: Windows adds about **+8 dB at 1 kHz** through dynamics alone. That
is separate from, and additional to, the amplifier gain recovered on
2026-08-01.
