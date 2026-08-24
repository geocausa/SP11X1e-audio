# CURRENT SP11 AUDIO HANDOFF

**Read this first when resuming the project.**

Date: 2026-08-24
Current promoted kernel Golden: **v33**
Current production userspace engine: **UbiG**
Repository: `geocausa/SP11X1e-audio`
Canonical branch: `main`

## Machine / boot identity

- kernel `7.1.5-sp11-render-parity-v4+`
- marker `sp11_entry=7.1.5-sp11-golden-v33-topcfg1-physical-vi`
- saved GRUB entry `sp11-audio-golden-v33-topcfg1-physical-vi`
- immediate rollback `sp11-audio-v32-feedback-exact-golden`
- v33 WSA macro srcversion `3FAA616CDE10DDBF9D90D6F`
- q6apm `687B16CF9C43B43E90C0746` (unchanged Golden mapping)
- v33 WSA `.ko.zst` SHA-256
  `39674078b0781323464b3de647caf9db0b25cde51d447e8e0253630de91d3f2d`
- fixed v33 initrd SHA-256
  `19db416046a363821f1d0887a43562d69c3593f6df85b7b16017adcc6bc59a44`

The root module tree has been synchronized to v33 after backing up its displaced
v32 copy, so a future initramfs regeneration cannot silently restore the old
WSA macro. Golden v32's fixed initrd was not modified.

## Closed speaker root cause

Golden v32 produced valid VI immediately, but TAP2 was physically ordered
`I,V,I,V` before SP_VI. Native Windows was `V,I,V,I` from the first valid packet.
Windows `qcaucd` physically writes WSA macro `TOP_CFG1=0x03` after each enabled
VI pair. Linux had `0x03` only as a regmap default, which did not guarantee the
physical write on the SP11 cache policy.

Golden v33 materializes the Windows write on `microsoft,denali`. TAP2 therefore
becomes `V,I,V,I` **before SP_VI**, with q6apm untouched. The earlier downstream
SP_VI `[2,1,4,3]` reorder is rejected; combined with TOP_CFG1 it double-corrected
the contract and reproduced right-amp `err0=0x20`/static failures.

## Final acceptance

- first-valid v33 TAP2 is sane `V,I,V,I`; no sentinel/garbage block;
- dedicated literal-zero starts remain near ~-97 dBFS in the SP7 RAW 3–20 kHz
  band;
- clean 160 Hz and 997 Hz 50% stress;
- source-identical quiet-room Windows/Linux program A/B at 10% within about
  0.1 dB across useful bands;
- 50% 10-second interior: broadband +0.027 dB, 80 Hz–10 kHz +0.002 dB,
  200 Hz–5 kHz -0.011 dB, 315 Hz–8 kHz -0.022 dB, 3–10 kHz -0.016 dB,
  40–120 Hz +0.089 dB; envelope correlation 0.956;
- 20/20 true-cold 50% protection cycles alternating 160/997 Hz at 0.20 source
  peak: 20 enables, 20 disables, zero PA faults, zero `err0=0x20`, zero XRUNs.

A single ~50 ms broadband burst was found in stricter post-analysis of soak
cycle 1. It had no kernel fault and did not recur in cycles 2–20 or a dedicated
long-idle two-birth literal-zero test. Preserve the evidence, but do not reopen
the producer-order fix unless a reproducible failure returns.

## UbiG production identity

Active Linux product/UI naming is UbiG. The promoted runtime plugin is
`~/.local/lib/ubig/ubig-sp11.so`; candidate staging paths are retired.


- `effect_input.sp11_ubig` — visible/default sink
- `effect_input.sp11_ubig_engine` — hidden engine input
- `effect_output.sp11_ubig` — speaker output
- `effect_input.sp11_ubig_bypass` — diagnostic bypass
- `sp11-ubig-volume-sync.service`
- `sp11-ubig-monitor-link.service`

Historical Windows-oracle evidence may retain vendor names where they identify
the proprietary source being studied. Those names must not be reused as active
Linux sink/service branding.

## Maintenance rules

- Keep Golden v32 fixed-initrd rollback intact.
- Do not reintroduce the SP_VI reorder.
- Keep root WSA macro and fixed v33 initrd identities hash-pinned.
- Do not use direct debugger physical WSA MMIO reads.
- Use SP7 WASAPI RAW at 0 dB capture gain for future physical A/B work.

## Canonical pointers

- `README.md`
- `deploy/golden-v33/`
- `repro/golden-v33/`
- `deploy/ubig/`
- `ubig/docs/STATUS.md`
- `docs/checkpoints/2026-08-24-GOLDEN-V33-PROMOTED.md`
- `docs/findings/2026-08-24-WINDOWS-TOP-CFG1-PHYSICAL-VI-ORDER.md`
- `deploy/golden-v32/` — rollback

## Next target

Speaker output/protection is closed. Start the next subsystem from this baseline
instead of continuing speculative WSA/SPVI tuning.
