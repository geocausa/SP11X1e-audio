# Windows WSA macro direct MMIO is unsafe

Date: 2026-08-15  
Status: RED / DO NOT REPEAT for APPS-side direct WSA-macro physical reads

## Question

Can classic KD on SP7 be used as a read-only Windows oracle for the LPASS WSA macro at physical `0x06b00000`, specifically the compander windows at `0x06b00580` / `0x06b005e0`?

## Debugger-control result

Yes for debugger control itself. PiMaster's SP7 ConPTY backend can launch classic `kd.exe`, connect over the existing KDNET port/key, issue the console break, reach `0: kd>`, send commands, resume, and detach. A user-side WinDbg click is not required.

This closes the tooling-access question and means future safe KD work can remain autonomous.

## Hardware-access result

No for direct WSA-macro MMIO.

The exact command tested was:

```
!dd [uc] 06b00580 L1
```

Two sessions produced different outcomes:

1. Earlier logged session `SP11_WSA_MACRO_UC_20260815.log`:
   - connected to Windows 26100 ARM64;
   - the read returned `Physical memory read at 6b00580 failed`;
   - target remained alive;
   - SHA-256 `f59cdf9dda55872ecb854e0068125fad5bface9ccc50064def22aae42a9553b2`.

2. Later direct ConPTY KD session (`job_59bEJS6nUiw5Bms9wFYmKHgT`):
   - connected cleanly at system uptime `0 days 0:37:08.317`;
   - reached `0: kd>` through a normal console break;
   - issuing the same uncached one-word read caused:

```
*** Fatal System Error: 0x00000124
(0x0000000000000011, 0xFFFFAE87C4F03028, 0, 0)
```

   - KD transport was then lost;
   - SP11 later returned on the persistent `sp11-audio-cps-v3` fallback while SP7 remained online.

## Interpretation

The important result is not that one read failed or that one read crashed. It is that **the same nominally read-only APPS-side WSA-macro access is not safe or deterministic** under Windows.

The WSA macro is owned/powered below the normal Windows APPS audio-driver layer. Access state depends on secure/resource ownership and power-domain state. A debugger physical read can therefore provoke a platform hardware error even when marked uncached.

This means direct KD MMIO cannot be used as the Windows WSA producer oracle.

## Permanent safety rule

Do not issue direct debugger physical reads (`!dd`, `!db`, `!dq`, cached or uncached) into the WSA macro physical aperture `0x06b00000...` for parity work.

Safe Windows evidence paths remain:

- bounded host-memory inspection at qcadcm/qcaucd breakpoints;
- qcaucd command-FIFO observation without reading protected MMIO;
- qcadsp/DevCfg/HAL static reverse engineering;
- ACDB graph/private-driver-data reconstruction;
- existing driver-owned runtime telemetry and event traces.

## Recovery / handback

The crash did not alter the saved boot policy. SP11 returned to `7.1.5-sp11-cps-v3+`; `saved_entry=sp11-audio-cps-v3` and `next_entry` is empty. No experimental DRE/CSR state was armed.
