# qcaucd private SoundWire slave-register boundary

Date: 2026-08-10 (Europe/London)

## Follow-up closure — 2026-08-11

The runtime recommendation in this checkpoint has now been superseded by
`docs/findings/2026-08-11-qcaucd-dp6-private-boundary-runtime.md`.

A validated read-only playback trace produced zero DP6-range hits at both
`FUN_140031188` (RVA `0x31188`) and the generic lower wrapper
`FUN_14003e850` (RVA `0x3e850`). Fresh static analysis then identified
`FUN_14003bf40` (RVA `0x3bf40`) as the actual SoundWire data-port programmer;
it constructs the per-port slave-register addresses and calls
`FUN_14003ac60` (RVA `0x3ac60`) directly. A breakpoint at `+0x3ac60`
captured all 18 expected DP6 setup/teardown writes in one speaker playback
cycle, including both active masks `0x03` and the left/right OffsetCtrl1 values
`0x00` / `0x19`.

Do not repeat the `+0x31188` recommendation below. The private DP6 programming
boundary is closed at `+0x3bf40 -> +0x3ac60`; any further strict-Windows
archaeology should move above `+0x3bf40` into the caller/state-population path.

## Result

A new private boundary below the already-closed qcadcm layers has been recovered
statically from the Surface Pro 11 Windows `qcaucd8380.sys` image.

`qcaucd8380.sys` contains generic SoundWire slave-register read/write
abstractions, not only fixed WSA8845 telemetry or amplifier-initialization
routines. This narrows the next strict-Windows CPS experiment to a read-only
runtime trace of the qcaucd slave-register path for SoundWire DP6 register
addresses (`0x0600..0x063f`).

This does **not** prove that Windows sends the public
`PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) through this path, and it does
not replace the already-captured per-slave DP6 result. It identifies a genuinely
new private boundary where Windows can be observed assigning slave registers
without debugger physical-MMIO reads.

## Binary and analysis provenance

Reviewed driver image:

- `qcaucd8380.sys`
- SHA-256 `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`
- source copy: `C:\Users\SurfacePro7\Documents\blobs\sp11-driverdump\qcaucd8380.inf_arm64_53bcc309a68aba55\qcaucd8380.sys`

The existing analyzed Ghidra project was reused rather than re-importing or
repeating broad analysis:

- `C:\Users\SurfacePro7\AppData\Local\Temp\GhidraQCAUCD\qcaucd.gpr`
- program `/qcaucd8380.sys`

A fresh caller/decompile pass was saved outside Git at:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-slave-helper-all-callers.txt`
- size 51,424 bytes
- SHA-256 `518E27C2BEB08AB583303B67412E300BCDAC209545970479A1D9C73B68258489`

## Recovered private boundary

### Direct slave-register write helper

`FUN_140031188`, RVA `0x31188`:

- accepts the slave/device context in argument 0;
- accepts the slave register address directly in argument 1;
- accepts the value in argument 2;
- routes the request through qcaucd's SoundWire command machinery.

Static callers include WSA8845 amplifier setup and status paths, but the helper
is not restricted to a fixed telemetry register set.

### Direct slave-register read helper

`FUN_140031298`, RVA `0x31298`:

- accepts the slave/device context in argument 0;
- accepts the slave register address directly in argument 1;
- accepts a destination pointer in argument 2;
- routes the request through qcaucd's SoundWire read-command machinery.

The destination pointer must not be blindly dereferenced in KD. If this helper
is traced later, pointer class and bounded size must be validated first.

### Generic register-write abstraction

`FUN_140025810`, RVA `0x25810`, is broader than the fixed WSA setup paths.
For one device class (`context[2] == 0x10000`) it maps the supplied register to
`register | 0x3000` and calls `FUN_140031188`. For the other path it passes the
raw register request into lower SoundWire command function `FUN_14003e850`.

This demonstrates that qcaucd has a generic register-write abstraction capable
of reaching the slave transport rather than a collection of telemetry-only
special cases.

### Generic register-read abstraction

`FUN_140020bc0`, RVA `0x20bc0`, is the matching generic read abstraction.
For device class `0x10000` it maps `register | 0x3000` and calls
`FUN_140031298`; otherwise it passes the raw register to lower SoundWire read
function `FUN_14003ea00`.

### Per-slave setup context

`FUN_140031430`, RVA `0x31430`, allocates/initializes per-slave state and reads
slave registers `0x3401..0x3404` while selecting controller/device context from
the discovered slave attributes. This is further evidence that the recovered
helpers are attached to real per-slave state rather than an unrelated local
register block.

## Relationship to the earlier qcaucd conclusion

`docs/findings/2026-08-10-qcslimbus-max34417-cps-closure.md` correctly ruled
out `qcslimbus8380.sys` as the WSA speaker bus and recorded that the qcaucd
routines reviewed at that time did not expose WSA slave DP6 registers.

The new static pass supersedes only that narrow qcaucd statement: qcaucd **does**
expose a generic SoundWire slave-register boundary. Static analysis has not yet
shown a literal fixed caller using DP6 (`0x0600..0x063f`), so it would still be
incorrect to claim that CPS programming has been statically identified.

The already-reviewed Windows runtime DP6 capture remains the source of truth for
the actual speaker layout:

- left WSA8845 logical device 2, identity `0x0000000402170220`;
- right WSA8845 logical device 1, identity `0x0000000402170221`;
- both DP6 channel enable `0x03`;
- left offset 1 = `0x00`;
- right offset 1 = `0x19`.

## First runtime attempt after the discovery

A single `kd.exe` owner was established on SP7 after verifying no existing
KD/WinDbg owner. A persistent log was opened at:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\QCAUCD_SLAVE_HELPER_20260810_2353BST.log`
- size 4,997 bytes
- SHA-256 `A29C75CA651A37AF4ACA2F9C83DC19B268010C02FD9FE9C43AC5FDA2A9A3D0F7`

The attach was deliberately rejected as a basis for RVA breakpoints because the
KD kernel view was internally inconsistent:

- `lm` exposed only `nt` as loaded while listing numerous ordinary drivers as
  unloaded;
- `lm m qcaucd` returned no module;
- `!drvobj qcaucd 2` could not read normal object-manager state and reported
  `ObpRootDirectoryObject` unavailable;
- simultaneously, the independent SP11 PiSlave path reported Windows healthy,
  `qcaucd` service state `Running`, and the Qualcomm Aqstic audio devices
  present and `OK`.

No qcaucd breakpoint was armed in this attach. No physical MMIO read was
performed. No MMIO, DSP, slave-register, or driver-state write was performed.
The session was detached using the required sequence `bc *`, `.logclose`,
`qd`; the `kd.exe` process was then verified absent.

## Next strict-Windows decision

Do **not** return to the closed qcadcm experiments.

On the next trustworthy KD attach, first require a coherent module list and a
resolved qcaucd image base. Then arm only a read-only logging breakpoint on the
qcaucd slave-write helper (RVA `0x31188`) and filter its register argument to
`0x0600..0x063f`, with immediate `gc`. Preserve the slave context pointer,
register, value and timestamp. A matching read-helper trace can be added only
if needed and without blindly dereferencing its destination pointer.

The purpose is to answer one precise question: does normal protected speaker
playback send the already-known DP6 configuration through this qcaucd private
slave-register boundary, and if so, which per-slave context produces the left
and right assignments?

This experiment uses the driver's normal kernel execution path. It must not be
replaced by direct debugger physical-MMIO reads.
