# Session findings 2026-07-31 — pipeline completeness audit

Everything established in this session, with provenance. Written because the
session's most valuable output (the AudioReach module ID map) was sitting in
`/tmp` and would have been lost on reboot.

Provenance tags: `OBSERVED` = read from this live machine today.
`STATIC` = from an on-disk blob. `SOURCE` = read from kernel source.
`PUBLISHED` = from Qualcomm's public repos. `INFERRED` = reasoned; inputs named.

---

## 1. Speaker protection IS working — confirmed live

`OBSERVED`. Kernel `7.1.5-sp11-audio-vi`. Played a 3-second tone at 30% volume;
the full protection chain configured and was accepted by the DSP:

```text
SP operating mode                      accepted
SP tag calibration                     accepted
SP configuration query                 accepted
SPVI configuration query               accepted
SPVI R0/T0                             accepted
SPVI channel mode                      accepted
SPVI processing mode                   accepted
SPVI tag calibration                   accepted
render endpoint calibration            accepted
VI endpoint calibration                accepted
SP/SPVI enabled with VI feedback       accepted
VOL_CTRL gain                          accepted
full-volume MSIIR calibration          accepted
VOL_CTRL mute                          accepted
channel-mixer calibration              accepted
GRAPH_START                            accepted
```

And the VI feedback transport is physically running on both amplifiers:

```text
wsa884x sdw:1:0:0217:0204:00:0  SP11 VI feedback stream: rate=8000 port=5 direction=source
wsa884x sdw:1:0:0217:0204:00:1  SP11 VI feedback stream: rate=8000 port=5 direction=source
WSA VI Protection: SP11 VI feedback ready on WSA_CODEC_DMA_TX_0
```

8 kHz on port 5 matches the format predicted from the Windows analysis.
`VISENSE = on` both amps, `WSA_AIF_VI Mixer VI_1/VI_2 = on` both.

**The speakers are no longer running open-loop.** This supersedes every earlier
project statement that protection was absent or unproven at the transport level.

Not established: whether the V/I data is numerically correct, or whether the
loop meaningfully constrains excursion. Accepted-by-DSP is not the same as
working-correctly.

---

## 2. Graph-calibration warning isolated to one unsupported record — closed

`OBSERVED`, corrected full-config diagnostic boot on 2026-07-31:

```text
sp11 graph-cal frame diagnostic: frames=107 accepted=106 rejected=1 first-rejected=63
SP11 OOB frame 63 offset 8352 iid 0x0000412b param 0x0800113d size 28 rejected: -95
```

`-95` is `-EOPNOTSUPP`. The normal aggregate 10,464-byte `SET_CFG` still returns
`AR_EUNSUPPORTED`, but the one-shot diagnostic re-sent every parsed record
individually and found that 106 of 107 are supported. The repeated Q6APM
`Error (3) Processing 0x01001006` messages in the boot are repeated aggregate
reports as graphs are created; they are not separate calibration defects.

This rules out the two broad failure theories that the manufacturer calibration
is wholly unsupported or that the aggregate consists mostly of malformed
records. It strongly supports continuing after this error as Qualcomm GSL does.
The diagnostic does not formally prove whether the original aggregate request
has partial or atomic application semantics; queryable post-state is the narrow
remaining way to settle that distinction.

The earlier offline walk reported 111 candidate frames and several trailing
marker-like records. For runtime acceptance accounting it is superseded by the
kernel diagnostic's own bounded walk: 107 frames, one rejection. The earlier
analysis remains useful as parser-development history, not as the authoritative
frame count.

## 3. AudioReach is open source — the reference nobody was using

`PUBLISHED`. `github.com/Audioreach/audioreach-engine` publishes every module's
numeric ID beside its name in the build files (`AMDB_MID` / `AMDB_MOD_NAME`),
and `audioreach.github.io/dev/available_modules.html` documents what each
algorithm does and where its source lives.

Extracted and saved permanently:

```text
01-audio/reference/audioreach-amdb-module-ids.json        (35 IDs)
01-audio/reference/audioreach-engine-provenance.txt       (commit + method)
```

Repo commit `1dbe615183e332e7b448a4f4b86672d609e9b1e2` (2026-07-23).

### Corrections to long-standing project assumptions

| Module ID | project believed | actually is |
|---|---|---|
| `0x07001038` | **MBDRC** | **SYNC** |
| `0x07001017` | UNKNOWN | **IIR_MBDRC** (the real MBDRC) |
| `0x07001013` | UNKNOWN | **CHMIXER** |
| `0x07001098` | UNKNOWN | **MUX_DEMUX** |
| `0x07001007` | UNKNOWN | SH_MEM_PUSH_MODE |
| `0x07001028` | UNKNOWN | PRIORITY_SYNC |
| `0x07001041` | UNKNOWN | RATE_ADAPTED_TIMER |

`ar_topology_inventory.py` corrected accordingly (41 verified names, was 20
partly-wrong guesses). Backup: `ar_topology_inventory.py.bak-before-amdb-20260731`.

Still unnamed (not in Qualcomm's open subset): `0x07001068`, `0x07001033`,
`0x0700102D`, `0x07001020`, `0x070010AF`, `0x070010A3`, `0x070010E4`,
`0x07001097`. Note `0x07001031` = `MODULE_ID_SMECNS_V2` from the Linux header.

Caveat: the published engine is Qualcomm's reference build. What runs on this
ADSP is a Microsoft/Qualcomm build that may differ. Treat published names as a
strong hypothesis, not automatic truth about this device.

---

## 4. What is deployed vs what Windows has

`OBSERVED` (deployed) / `STATIC` (Windows, from ACDB bundles, evidence class
`static Windows ACDB; not proof of runtime selection`).

### The protection subgraph is EXACT

Deployed connections match the Windows ACDB byte-for-byte on instance IDs and
ports:

```text
0x4001 SAL      :1 -> 0x402C CHMIXER :2
0x402C CHMIXER  :1 -> 0x4027 SP      :2
0x4027 SP       :1 -> 0x4002 SPLITTER:2
0x4002 SPLITTER :1 -> 0x4003 LOGGING :2
0x4003 LOGGING  :1 -> 0x4157 DMA_SINK:2

VI lane A: 0x4026 -> 0x4025 -> 0x4024 SP_VI
VI lane B: 0x402B -> 0x402A -> 0x4029 -> 0x4028
```

### The tuning half is almost entirely absent

Windows' splitter `0x4002` is a 1->7 with **four** outputs. Deployed has **one**.

```text
0x4002:1  -> 0x4003 logging -> DMA sink      DEPLOYED
0x4002:5  -> 0x4747                          MISSING
0x4002:9  -> 0x47C9                          MISSING
0x4002:11 -> 0x4730                          MISSING
```

Recovered contents of the three missing subgraphs (bundles `0x127c`, `0x1a4cc`,
`0x1b10`):

| | tap 5 | tap 11 | tap 9 |
|---|---|---|---|
| MSIIR | 3 | 3 | - |
| **IIR_MBDRC** | **1** | **1** | - |
| GAIN | 1 | 1 | - |
| CHMIXER | - | 1 | - |
| VOL_CTRL | 1 | 1 | - |
| PRIORITY_SYNC | 1 | 1 | 1 |
| SPLITTER | 1 | 1 | 2 |
| MFC | 2 | 2 | 2 |
| DATA_LOGGING | 7 | 7 | 6 |
| SH_MEM_PUSH_MODE | 1 | 1 | - |
| RD_SHARED_MEM_EP | - | - | 1 |
| SWR_SINK | 1 | 1 | - |
| CODEC_DMA_SOURCE | 1 | 1 | 1 |
| PCM_CNV / SOFT_PAUSE | 1 / 1 | 1 / 1 | 1 / - |
| UNKNOWN | 2 | 2 | 3 |

Taps 5 and 11 are structurally identical with mirrored instance IDs — the two
per-speaker tuning chains. Tap 9 is a distinct readback/monitoring path
(`RD_SHARED_MEM_EP`, two splitters).

Deployed graph has **2 MSIIR total and zero IIR_MBDRC**. Windows has 3 MSIIR
and 1 IIR_MBDRC **per speaker**, plus per-speaker GAIN, CHMIXER and VOL_CTRL.

Full connection topology for all three subgraphs is in the bundle JSONs under
`artifacts/live/windows-bundle-0x{127c,1a4cc,1b10}.json`.

---

## 5. wsa884x driver assessment

`SOURCE`. The amplifier driver is **not** the weak layer:

- full hwmon temperature sensor (`wsa884x_get_temp`, `hwmon_read`, `is_visible`)
- 97 OTP references — reads each chip's factory calibration
- CPS, PBR, VISENSE all present; regmap with proper `reg_defaults`
- stock upstream apart from patch 0023's three log lines

Two real gaps:

1. **Amp die temperature is exposed to Linux via hwmon but is not fed to the
   DSP protection loop.** No `thermal` references in the driver.
2. **R0/T0 sent to the DSP is static topology data, not measured.**
   `SOURCE`: `audioreach_stage_frame(dynamic, i)` reads frame 1 of a pre-baked
   blob. Nothing queries wsa884x for what these specific amplifiers measured.
   R0 is the voice-coil DC resistance the protection algorithm uses as its
   heating reference; its provenance here is unknown.

---

## 6. Correction: a fabricated symptom

The operator has **never described how the audio sounds**. Earlier in this
session I introduced the word "thin", reasoning from the full-volume MSIIR and
absent MBDRC, then repeatedly wrote as though it had been reported — including
into `audio-gap-audit.md` as "the symptom profile matches the reported ones".

**No symptom was reported.** That claim is retracted; see the correction applied
to section 4 of `audio-gap-audit.md`. The technical findings stand on their own
and do not need a symptom to justify them.

This is the same failure mode this project has repeatedly caught elsewhere:
inference presented as observation.

---

## 7. State of the diagnostic kernel — corrected and successful

The earlier 159-module candidate remains unusable and preserved as negative
evidence. A corrected replacement was rebuilt from the same diagnostic source
using the exact `audio-vi` configuration and booted successfully as
`7.1.5-sp11-audio-diag-observe+`.

Observed live:

```text
4,061 built-in options
7,651 module options
7,886 installed module files
ALSA card present
speaker playback working
touchscreen working
```

Patch `0023` captured complete GET_CFG bodies and the actual WSA port masks.
Patch `0024` made the existing frame diagnostic run once and produced the
106/107 result above. Patch `0024` is intentionally non-passive and should not
be carried into the daily `audio-vi` kernel.

See
[`2026-07-31-diagnostic-observation-success.md`](2026-07-31-diagnostic-observation-success.md).

---

## 8. Ordered next steps

1. Preserve the successful diagnostic result and exact patches in the repo.
2. Decode the stable 92-byte SP and 68-byte SPVI GET_CFG payloads and identify
   dynamic V/I, R0, temperature and excursion fields.
3. Build one isolated manual runtime `VOL_CTRL` candidate. Hold PipeWire level
   fixed so DSP gain is tested without double attenuation.
4. After runtime gain is proven, select the recovered MSIIR row for the same
   endpoint volume state.
5. Perform controlled A/B tests before adding topology complexity.
6. Reconstruct the two per-speaker tuning branches and monitoring branch only
   after the bounded volume path.
7. Keep PBR and CPS transport as separate hardware work; do not add their shared
   ports blindly to the playback stream.

Do not rerun the one-shot 107-frame experiment merely to reproduce the same
answer, and do not graft the missing ~50 modules before the smaller volume path
is measurable and rollback-safe.
