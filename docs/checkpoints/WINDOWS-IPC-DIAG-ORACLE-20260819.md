# Windows IPC/DIAG oracle checkpoint — 2026-08-19

## Hard node mapping
A full retail-Windows Qualcomm IPC Router namespace scan was correlated against Golden v31 Linux `qrtr-lookup`.

- Windows/local APPS node: `1`
- **LPASS / ADSP: node `5`**
- CDSP: node `10`
- WLAN: Linux node `7`

The node-5 fingerprint is decisive: Golden Linux exposes service `0x301` / 769 as **SLIMbus control service** on node 5, plus Snapdragon Sensor Core (`0x190`), matching the Windows node-5 endpoints.

Golden Linux also exposes the transport topology explicitly:
- `6800000.remoteproc:glink-edge.IPCRTR` = ADSP/LPASS side
- `32300000.remoteproc:glink-edge.IPCRTR` = CDSP side
- `6800000...LOOPBACK_CTL_LPASS`
- `32300000...LOOPBACK_CTL_CDSP`

## Critical DIAG model correction
The working Linux userspace bridge is upstream `andersson/diag`, commit `23c12c1`, at `/home/geoca/Tools/diag-router-sp11.tmp`.

It uses `DIAG_SERVICE_ID = 4097 (0x1001)`, but **the host publishes most DIAG endpoints; it does not expect LPASS to advertise them before the host router exists**.

For LPASS (`instance_base = 64`):
- host publishes control `0x1001:64`
- host looks up remote command `0x1001:65`
- host publishes data `0x1001:66`
- host publishes DCI data `0x1001:68`

This explains why retail Windows showed no `0x1001` service in the baseline full namespace scan: Microsoft ships the normal Qualcomm IPC Router but not the host DiagRouter provider, so the host-side DIAG publications never occur and LPASS has no reason to expose the command endpoint.

## Windows ABI already reversed
Retail Windows has `qsocketipcrum.dll` over `\\.\IPC` / `qcipcrouter8380.sys`:
- private address family `0x1b`
- sockaddr size `0x2c` / 44 bytes
- `qbind()` requires address type `1` and registers `{service, instance}` through IOCTL `0x12200c`
- `qconnect()` uses address type `0` with `{node, port}`
- `ipcr_find_name()` uses IOCTL `0x122008`
- lookup no-match = `STATUS_NOT_FOUND (0xC0000225)` -> qsocket `-4`
- qcipcrouter lookup compares exact service and low 8 bits of instance (`0xff` mask internally)

Therefore the next Windows experiment can reproduce the Linux DIAG bootstrap entirely in userspace, without installing `qcdiagrouter8380.sys`:
1. `qsocket(AF=0x1b, type=1)` x sockets.
2. `qbind` host LPASS services `0x1001:64`, `:66`, `:68` using address type 1.
3. Poll `ipcr_find_name(0x1001, 65)`.
4. Require the new endpoint to be on **node 5**.
5. If it appears, connect to it and implement the same non-HDLC/control protocol as `andersson/diag`.
6. Ultimately enable/collect ADSP log code `0x1586` and compare Windows tap 1 / VI tap 2 / CPS tap 3 against Linux.

## Safety
Golden v31 remains the saved Linux default. The Windows CDSP live-disable/restart discriminator was rejected because PnP reported reboot-required. A temporary `ConfigFlags=1` from the disable attempt was explicitly restored to `0`; CDSP remained `Status=OK`.

## Evidence still on Windows NTFS
`C:\Users\geoca\Documents\SP11-Audio-Audit-20260812\windows-protection-deconstruct-20260819\`
contains the full IPC scan, helper scripts and checkpoint hashes. Do not overwrite the original scan.

## Windows bootstrap experiment — hard positive
On the next native Windows boot, a pure userspace qsocket test reproduced the missing host DIAG bootstrap:

- baseline `ipcr_find_name(0x1001,65)` = no service (`-4`)
- `qbind()` host LPASS control `0x1001:64` = success (`0`)
- within ~160 ms, LPASS **node 5** advertised remote CMD `0x1001:65` at port **25**
- host data `0x1001:66` and DCI `0x1001:68` also bound successfully
- after an orderly close in the first bounded test, the remote CMD service disappeared again

This is a causal proof that retail Windows already has all required IPC-router/LPASS plumbing; what is absent is the host DIAG publication/orchestration normally supplied by a Qualcomm DiagRouter component.

A second experiment again produced the LPASS CMD endpoint (this time dynamic port 32) and `qconnect()` returned success. Its first passive receive helper was killed by the tool timeout while blocked in `qrecvfrom()`, leaving orphan qsocket registrations until reboot. The session was therefore discarded/clean-rebooted. Next attempt must use `qpoll()` before each receive and run as a controllable persistent job so `qclose()` always executes.

Windows raw first-bootstrap evidence: `windows-lpass-diag-bootstrap-20260819.csv`, SHA-256 `ABCC3F88B70F86E9C2A2A4E8940EB4DA93A2985E708A20C6B7EDE5EC89017BC7`.
