# Historical Custom1 GEQ forensic recovery — 2026-08-05

## Purpose

Recover the exact twenty user GEQ values that were visible in the 2026-06-12
Dolby Access `Custom1` capture. This is historical-data recovery only: the
original Dolby GEQ implementation itself is already recovered and runs on
Linux with arbitrary 20-band values.

## What is proven about the old Custom1 state

The archived capture
`20260612_084403_dolby_access_custom1_surround_on_leveler_on_dde_spatial_atmos`
records:

- Custom1 selected;
- a visibly shaped high-bass/high-treble custom EQ curve;
- Surround On;
- Volume Leveler On;
- Device Default Effects;
- Dolby Atmos for Speakers;
- Windows volume 14.

The old DAX RPC probe did not preserve the numeric curve. Its `GetGEQLevels`
implementation incorrectly treated the return as a scalar/short array. Fresh
server-side RE of `DAX3API.exe` proves the actual contract is exactly twenty
signed `int32` values (80 bytes), each in public range `-192..+192`.

Do not derive the curve from the VLLDP `child1+0xC0C` vector. GEQ is applied in
the later VR stage.

## DAX3API server-side confirmation

Verified DAX3API binary SHA-256:

```text
e77f87dd29275a6f814352494fe019c7a742a1a4ab0fa7911550d15586dda19c
```

Fresh throwaway Ghidra analysis confirms:

- opnum 17 / `GetGEQLevels` returns a 20-element SAFEARRAY;
- opnum 18 / `SetGEQLevels` accepts exactly twenty int32 values and validates
  every element in `-192..+192`;
- the configuration property is named `graphic-equalizer-bands`;
- Reset GEQ restores the corresponding profile property rather than using a
  separate DSP algorithm.

The DAX service path updates the current endpoint/profile configuration. It did
not expose a separate durable file/registry write that could recover the old
values after the app profile state was replaced.

## Direct read-only Windows-partition recovery

The original internal Windows partition is still present as:

```text
/dev/nvme0n1p3
NTFS, about 202.4 GiB
```

It was mounted with `udisksctl ... --options ro` and independently verified by
`findmnt` as `ro`. No read-write Windows mount was used.

The original user package directories still exist:

```text
Users/Geoca/AppData/Local/Packages/
  DolbyLaboratories.DolbyAccessOEM_rz1tebttyb220/
  DolbyLaboratories.DolbyAccess_rz1tebttyb220/
```

Both contain real UWP `Settings/settings.dat` registry hives plus transaction
logs and app-service logs.

### OEM Dolby Access hive

Current SHA-256:

```text
42b524e7c98ad96d31a5acbd8d26bbeed9d74efd0f18ce35cc2415abd291c4e0
```

`hivexml` shows the current `DapProfile` is:

```json
{"IntelligentEqualizerType":"Off","Type":"Movie"}
```

The retained OEM logs begin 2026-07-05. Its July transaction logs contain later
Movie state only, not the June Custom1 profile.

### Standard Dolby Access hive

Current SHA-256:

```text
49a6f4885d74863772047150ec451870042ebd4d05e45dfa71fd3d974ca8d3eb
```

Current visible values include:

```json
{"Type":"Dynamic"}
```

and profile history for later Movie/Music state. The August transaction logs
also contain later Movie/Music/Game/Dynamic records, but no Custom1/Personalize
GEQ array.

Dolby's retained app-service log format is capable of serializing
`CustomEqualizerSettings`; an August Movie response explicitly records it as
`null`. Searching all retained package logs found no historical non-null
CustomEqualizerSettings record.

## Lower-level forensic checks

The Windows volume was unmounted before the NTFS deleted-file scan and remounted
read-only afterwards. `ntfsundelete` found no potentially recoverable deleted
`settings.dat*` MFT record. This indicates the package update/reset reused or
overwrote the relevant hive metadata rather than leaving a separately
recoverable old hive.

`System Volume Information` contains shadow-storage-looking files dated only
2026-08-03/04. They postdate the June Custom1 state and cannot be a June source.

A direct search of the live Windows user tree found no image/screenshot saved
around the 2026-06-12 Custom1 session. Archive/Codex/Claude/Gemini/session-log
searches likewise preserve only the description `high bass / high treble`, not
twenty numeric slider values.

A ChatGPT persistent Library search was also attempted for a potentially
uploaded Custom1 screenshot, but the Library retrieval service returned HTTP
401 Unauthorized in this session. Therefore that source is currently
inaccessible rather than proven empty.

## Conclusion

The exact historical Custom1 twenty-value array is not recoverable from the
currently accessible on-machine/archive evidence. This is an evidence-recovery
hard wall, not a processing implementation gap.

The native Linux host already implements the original Dolby GEQ path and can
apply any valid twenty-value DAX Custom curve. The missing artifact is only the
specific old user-edited curve from 2026-06-12. If the original uploaded
screenshot or an older Dolby Access package hive becomes accessible later, the
values can be entered without further DSP reverse engineering.
