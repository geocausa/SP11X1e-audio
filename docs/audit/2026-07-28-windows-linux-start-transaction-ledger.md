# SP11 default-speaker start transaction ledger

Date: 2026-07-28

## Scope and evidence rule

This ledger covers the Windows DEFAULT speaker path through graph start. Dolby
dynamic processing remains a separate project; its module position may remain
in the graph, but no Dolby user-mode tuning is synthesized here.

No single evidence source is sufficient:

- the recovered Qualcomm AudioReach headers and GSL source define packet
  layouts and the intended host/DSP protocol;
- the full Windows QGPR capture proves the selected runtime branch, instance
  IDs, command order, sizes, inline payloads, and OOB boundaries;
- the reviewed REV_0D ACDB resolves the OOB parameter bodies;
- the KD graph capture proves the instantiated graph structure;
- Linux runtime logs prove the first rejected transaction on this firmware.

The canonical dynamic source is
`qgpr_records_domain2_boot_init_after_music_toggle_volume_FULL_20260612_015043.decoded.csv`.
The command pattern below repeats at sequences 3-31, 5717-5769, and
10365-10399.

## Root cause of the repeated Linux failure

Windows instantiates IID `0x4660` as module `0x07001006`,
`SH_MEM_PULL_MODE`. The old Linux topology generator deliberately rewrote it
to `0x07001000`, `WR_SHARED_MEM_EP`, and the DAI then used legacy
`DATA_CMD_WR_SH_MEM_EP_DATA_BUFFER_V2` period submissions.

Those are different endpoint contracts:

- `SH_MEM_PULL_MODE` reads continuously from a mapped circular buffer,
  publishes a byte index through a second mapped position page, and raises
  registered watermark events;
- `WR_SHARED_MEM_EP` consumes individually submitted data-buffer commands and
  returns one completion per submitted buffer.

The DSP therefore opened the graph, accepted the supported calibration
records, and then returned `AR_ENOTEXIST` when Linux configured the substituted
endpoint instance. This was not evidence that Windows used a different DSP
firmware or that the atomic calibration blob was fundamentally invalid.

## Proven pre-start command sequence

| Step | Windows transaction | Exact target/data | Linux implementation status |
|---:|---|---|---|
| 1 | Map circular data page | 4 KiB map; captured DSP address `0x1_00008000` | Data mapping exists before graph open; pull config uses the mapped PCM ring handle |
| 2 | Map position page | 4 KiB dedicated map; captured DSP address `0x1_00009000` | Dedicated coherent position page, SID-encoded DSP address, distinct map token |
| 3 | `GRAPH_OPEN` | Canonical 29-module, 26-edge integrated graph | Exact IID `0x4660`/MID `0x07001006`; no endpoint translation |
| 4-6 | ACDB deregistration housekeeping | ACDB service traffic, not graph content | Not reproduced; no persistent registration is created by Linux |
| 7 | Graph calibration, OOB | 10,464 bytes | Rebuilt from reviewed REV_0D ACDB; `AR_EUNSUPPORTED` policy matches GSL |
| 8 | Pull-ring config | IID `0x4660`, PID `0x0800100a`, 28-byte payload; ring 3,840 bytes | Exact parameter structure and mapped data/position handles |
| 9 | Pull watermark registration | IID `0x4660`, event `0x0800101c`; levels 1,920 and 3,840 | Exact `APM_CMD_REGISTER_MODULE_EVENTS` payload |
| 10 | Soft-pause event registration | IID `0x466b`, event `0x0800103f` | Implemented without config payload |
| 11 | Soft-resume event registration | IID `0x466b`, event `0x08001043` | Implemented without config payload |
| 12 | Pull media format | IID `0x4660`, PID `0x0800100c`; 48 kHz/S16/stereo | Exact PCM media-format record |
| 13 | PCM converter format | IID `0x465f`, PID `0x08001008`; 48 kHz/S16/stereo | Explicit exact-IID configuration |
| 14 | MFC output format | IID `0x466a`, PID `0x08001024`; 48 kHz/S16/stereo | Explicit exact-IID configuration |
| 15 | SP operating mode | IID `0x4027`, PID `0x080011e9` | Captured inline frame |
| 16 | SP tag calibration, OOB | 1,888 bytes | Reviewed module-tag row |
| 17 | SP configuration query | IID `0x4027`, PID `0x080011e8`, 68-byte query | Exact request size and position |
| 18 | SPVI configuration query | IID `0x4024`, PID `0x080011f6`, 44-byte query | Exact request size and position |
| 19-21 | Dynamic SPVI parameters | PIDs `0x080011f5`, `0x080011f4`, `0x080011ff` | Seven Windows cycles are byte-identical |
| 22 | SPVI tag calibration, OOB | 1,328 bytes | Reviewed module-tag row |
| 23 | ACDB deregistration housekeeping | ACDB service traffic | Not reproduced |
| 24 | Render endpoint calibration, OOB | 64 bytes | Reviewed endpoint-tag row |
| 25 | VI endpoint calibration, OOB | 64 bytes | Reviewed endpoint-tag row |
| 26 | VOL_CTRL gain | IID `0x4a63`, PID `0x08001038`, 104-byte payload | Full captured parameter frame is byte-identical |
| 27 | Volume-step MSIIR calibration, OOB | 216 bytes | REV_0D subgraph `0x7f`, 48 kHz/stereo/step 30/device 1/2-channel CKV |
| 28 | VOL_CTRL mute | IID `0x4a63`, PID `0x08001039`, 104-byte payload | Full captured parameter frame is byte-identical |
| 29 | Root channel-mixer calibration, OOB | 40 bytes | Tag `0x04010009`; default-equivalent ACDB payload |
| 30 | ACDB deregistration housekeeping | ACDB service traffic | Not reproduced |
| 31 | `GRAPH_START` | Root, speaker `0x7f`, render `0x7e` | Graph start after every required setup stage succeeds |

## Runtime transport contract

The protected PCM interface is intentionally fail-closed and exposes only:

- 48,000 Hz;
- signed 16-bit little-endian PCM;
- two interleaved channels;
- two periods;
- 480 frames / 1,920 bytes per period;
- 960 frames / 3,840 bytes per circular buffer.

The DSP reads directly from the ALSA mmap ring. Linux does not send
`WR_SHARED_MEM_EP_DATA_BUFFER_V2` for this graph. Watermark module events call
`snd_pcm_period_elapsed()`, and the ALSA hardware pointer is taken from the
DSP-updated position page using the documented double-read of
`frame_counter` around the byte index.

## Why static assembly did not eliminate all runtime validation

Disassembly exposes every possible branch but cannot prove which graph key
vector, calibration row, endpoint mode, or user-mode policy Windows selected
on this machine. A trace alone proves order and IDs but, for OOB commands,
often records only an address and byte count. The reliable reconstruction is
therefore the intersection:

1. dynamic trace for selected order and boundaries;
2. graph/KD capture for instantiated identities and connections;
3. Qualcomm source for ABI semantics;
4. ACDB resolution for OOB bytes;
5. one Linux execution for firmware acceptance and physical behavior.

The next boot is not intended to discover another anonymous command. Every
pre-start stage now has a stable name in the kernel log, so any remaining
firmware divergence will identify the exact Windows transaction that differs.
