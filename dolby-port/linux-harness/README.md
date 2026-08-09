# SP11 original-Windows-Dolby Linux harnesses

These are research/reproduction harnesses from the 2026-08-09 ASAR breakthrough.
They host the original ARM64 Windows Dolby HRTF/DAP binaries on Linux with a deliberately small
Windows/COM facade.

Start with the durable finding:

`docs/findings/2026-08-09-ASAR-WINDOWS-DOLBY-LINUX-HOST-BREAKTHROUGH.md`

Important probes:

- `hrtf_activate_smoke.c` — original HRTF factory/activation proof
- `sp11_dap_setapo_smoke.c` — DAP APO-context readiness proof
- `sp11_dap_configure_replay_fmtmap.c` — legitimate DAP ConfigureEncoder closure using captured tuning
- `sp11_hrtf_full_init_smoke.c` — original HRTF orchestrates original DAP and initializes successfully
- `sp11_hrtf_bed_process_smoke.c` — bit-exact stereo-bypass processing proof
- `sp11_hrtf_19object_curve.c` — 19-static-object VirtualSurround/ASAR replay
- `sp11_hrtf_19object_level5_curve.c` — same replay after real vendor leveler amount setter = 5
- `sp11_hrtf_linux_core_state.c` — inspect the underlying Linux-hosted DAP-VR core state

## Private inputs intentionally excluded

Do not commit Microsoft/Dolby DLLs, full-memory dumps, or captured tuning-property blobs here.
Their hashes and local provenance are recorded in the finding document. The SP11 RE archive
contains the matching DLLs, and the Windows oracle workspace contains the matching dumps.

Several exploratory probes are intentionally retained. They document failed/partial hypotheses
and are useful when checking whether a later change regresses a previously reached boundary.
