# QCADCM boot-zero trace excludes hidden WSA codec-register resource

Date: 2026-08-15
Status: GREEN negative closure for the normal qcadcm boot path

## Question

Could Windows be programming the WSA macro through Qualcomm's private ADSP codec-register hardware-resource service (`0x0800131b`) during early boot, before the earlier runtime trace was armed?

That remained a real gap because the previous qcadcm trace began after Windows had already reached roughly 10.8 seconds of uptime.

## Exact binary identity

The target SP11 Windows `qcadcm8380.sys` and the SP7 reverse-engineering copy were re-hashed before the trace and match exactly:

`37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429`

The recovered RVAs therefore apply to the live target binary:

- `AudioHwRscIoctl`: RVA `0x89380`
- `gsl_command_hw_rsc_custom_config`: RVA `0x5c5d8`

## Strict loader-trapped method

Classic `kd.exe` ran on SP7 through ConPTY/KDNET. No WSA MMIO reads were used.

A first early-boot pass confirmed that no qcadcm hardware-resource traffic occurred between approximately 10.9 seconds and 2:11 uptime, suggesting the important traffic was earlier.

For the strict pass, KD remained attached across a reboot and was configured with:

`sxe ld:qcadcm8380.sys`

The second Windows boot stopped on the qcadcm module-load event at 10.941 seconds uptime. The target remained paused while breakpoints were installed on both recovered qcadcm resource boundaries. Execution then resumed and was observed through 1:48.901 uptime, by which time normal Windows userspace/PiSlave was online.

The breakpoints only read qcadcm arguments and bounded host-memory request buffers. They performed:

- no direct MMIO access;
- no target register writes;
- no synthetic hardware-resource request;
- no audio-policy change.

## Result

The strict loader-trapped segment contains **26 hardware-resource transactions** total.

Only three resource families occur:

| Resource | Meaning | Requests | Releases | Total |
|---|---|---:|---:|---:|
| `0x08001032` | hardware core | 5 | 5 | 10 |
| `0x0800102c` | clocks | 6 | 6 | 12 |
| `0x080014f3` | endpoint DSP GPIO | 4 | 0 | 4 |

The clock requests cover IDs `0x30c`, `0x314`, and `0x315`, each requested twice at 19.2 MHz and released twice.

The recovered private codec-register resource:

`0x0800131b`

appears **zero times**.

Only the expected hardware-resource commands occur:

- `0x0100100f` — request
- `0x01001010` — release

## Combined closure

This result closes the remaining early-boot loophole in the qcadcm theory.

We now have three independent negative results around the same hypothesis:

1. The complete Microsoft Surface REV_0D ACDB/private-driver-data inventory contains no private codec-resource IDs and no WSA-macro physical addresses.
2. A marked native-speaker playback trace contains no codec-register hardware-resource request.
3. A strict qcadcm module-load-trapped boot trace contains no `0x0800131b` request from module load through normal boot completion.

Therefore the normal SP11 internal-speaker host path does **not** source a hidden WSA-macro register program through qcadcm's hardware-resource service.

## Remaining boundary

The private ADSP codec-register service is real and may be used by other modes or clients, so its existence is not being denied. It is simply not part of the observed normal internal-speaker boot/playback path.

The remaining Windows-vs-Linux producer gap should now be pursued inside:

- ADSP/HAL-internal WSA macro runtime state;
- platform/silicon reset state and hardware defaults;
- another non-qcadcm mechanism.

Direct APPS-side reads of the `0x06b00000` WSA macro window remain rejected because the preserved KD experiment caused a fatal WHEA `0x124`.

## Evidence

- `artifacts/reviewed/2026-08-15-qcadcm-hw-rsc-bootzero.log`
- `artifacts/reviewed/2026-08-15-qcadcm-hw-rsc-bootzero.json`
- `artifacts/reviewed/2026-08-15-qcadsp-codec-resource-vs-acdb.json`
- `artifacts/reviewed/2026-08-15-windows-wsa-macro-mmio-kd-whea124.json`
