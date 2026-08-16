# qcadcm ATS / DiagRouter transport recovery

Date: 2026-08-16
Status: AMBER — command namespace and DiagRouter init/registration IOCTLs recovered; live GET not yet issued

## Scope

This note narrows the safe Windows path for observing LPASS WSA-macro state without direct APPS MMIO. Direct physical reads of the WSA macro aperture remain permanently rejected after the WHEA 0x124 event.

## ADIE ATS command namespace

The exact SP11 qcadcm8380.sys ADIE service dispatch table contains these little-endian constants:

- 0x41520001 — codec information
- 0x41520002 — single-register GET
- 0x41520003 — multi-register GET
- 0x41520004 — single-register SET
- 0x41520005 — multi-register SET

The service category is the high 16-bit value 0x4152 ("AR" in the driver's ATS registry logic). ats_execute_command validates the service category before dispatch.

For single-register GET, the already-recovered payload is exactly 12 bytes:

    { uint32 handle, uint32 register_id, uint32 mask }

On success qcadcm returns one 32-bit register value and reports output size 4.

## ATS framing

ats_execute_command parses the outer buffer as:

- uint32 command_id at offset 0
- uint32 payload_length at offset 4
- payload beginning at offset 8

It requires payload_length == total_input_size - 8 before dispatch.

This means the read-only ADIE request is distinguishable at the outer ATS layer by command_id 0x41520002 before the 12-byte payload is interpreted.

## DiagRouter transport constants

qcadcm's internal ATS thread opens `\\Device\\DiagRouter` and formats two private IOCTLs whose exact constants were recovered from the shipped binary data used by the WDF request path:

- 0x80082400 — DiagRouter ATS INIT path (custom device type 0x8008, function 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
- 0x80082404 — DiagRouter ATS registration path (custom device type 0x8008, function 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)

The constants were recovered directly from qcadcm8380.sys data near 0x14004bd04/0x14004bd08 and are not guessed CTL_CODE values.

## Safety boundary

Do not issue 0x41520004 or 0x41520005. Do not guess a WSA handle. Do not re-use direct WSA MMIO reads.

The next live Windows step is allowed only after the remaining registration/read packet boundary and valid WSA/codec handle semantics are pinned, so that the request can be proven end-to-end to reach 0x41520002 only.

## Relevance to H03

This closes two major unknowns in the safe observation path: the exact read-only ADIE command ID and the exact qcadcm-to-DiagRouter initialization/registration IOCTLs. The unresolved pieces are now limited to the DiagRouter registration/read envelope and discovery of the valid handle for the WSA macro / codec instance.

## Evidence

SP7 retained static analysis:

- C:\\Users\\SurfacePro7\\Documents\\KDNET\\Codex\\qcadcm-ats-dispatcher-20260816.txt
- C:\\Users\\SurfacePro7\\Documents\\KDNET\\Codex\\qcadcm-adie-service-handler-20260816.txt
- C:\\Users\\SurfacePro7\\Documents\\KDNET\\Codex\\qcadcm-adie-command-set-20260816.txt
- C:\\Users\\SurfacePro7\\Documents\\KDNET\\Codex\\qcadcm-public-ioctl-20260816.txt

Fresh 2026-08-16 Ghidra DumpBytesAt confirmation on SP7:

    14004bd04 00 24 08 80 04 24 08 80
    14004bd08 04 24 08 80 00 00 00 00

which decodes to 0x80082400 followed by 0x80082404.

## Exact Diag command and ACTP envelope

Further static recovery on the same binary pins the actual qcadcm ATS Diag command, not merely the registered range:

- qcadcm embeds the four-byte response/request subsystem header `4b 11 03 08` at 0x14004c838.
- This is `DIAG_SUBSYS_CMD_F` (`0x4b`), subsystem `0x11`, command `0x0803` little-endian.
- qcadcm registers subsystem `0x11` for command range `0x0803..0x0834`, but the ATS response builder specifically reuses the fixed `0x0803` header.
- The DiagRouter write IOCTL beside this path is 0x80082410.

The 16-byte ACTP header immediately after the four-byte Diag header is now structurally decoded from `DiagATSProcessCommands` and the response-fragment builder:

- byte 0: fixed protocol/version value `0x01`
- byte 1: fixed header-size/type value `0x10`
- byte 2: fragment sequence index (generated fragments start at 1)
- byte 3: flags; bit 1 marks final fragment, bit 2 marks response, bit 3 marks fragmented transfer; normal request path has bit 0 clear
- uint32 at +4: fragment payload offset
- uint16 at +8: fragment payload length
- uint16 at +10: preserved/reserved by this implementation
- uint32 at +12: total payload length for fragmented transfer

For a non-fragmented inbound packet, qcadcm copies the 16-byte ACTP header, allocates exactly the uint16 payload length at +8, and passes that payload to the ATS callback. The response path copies the same header and sets ACTP response flag bit 2.

This reduces the candidate read-only codec-info request to a fully bounded stack:

1. Diag header `4b 11 03 08`
2. 16-byte ACTP header
3. ATS header `{ command_id=0x41520001, payload_length=0 }`

The next Windows experiment can therefore begin with codec-info enumeration before any register GET. SET commands 0x41520004/05 remain forbidden.

Additional SP7 evidence:

- `qcadcm-ats-callback-xrefs-20260816.txt`
- `qcadcm-diag-header-helpers-20260816.txt`
- Ghidra bytes: `14004c838 4b 11 03 08 10 24 08 80`
