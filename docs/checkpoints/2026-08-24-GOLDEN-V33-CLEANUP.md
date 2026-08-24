# 2026-08-24 — Golden v33 post-promotion cleanup

After Golden v33 promotion and root-module hardening, the superseded diagnostic
boot entries were archived by hash and removed from `/boot`/GRUB:

- v32 SP_VI I/V reorder;
- rejected v32 SP_VI reorder + TOP_CFG1 postpair;
- v32 TOP_CFG1-only candidate, now represented by Golden v33.

The archive is retained locally under
`00-RE-archive/boot-candidate-cleanup-20260824/` and includes the removed GRUB
scripts plus SHA-256/file manifests for each boot directory. Candidate source
and evidence trees outside `/boot` were not deleted.

Kept boot paths:

- `sp11-audio-golden-v33-topcfg1-physical-vi` — saved default;
- `sp11-audio-v32-feedback-exact-golden` — immediate rollback;
- Golden v31/v28 historical rollback/comparison entries;
- CPS-v3 conservative rescue.

`update-grub` completed successfully and the saved GRUB default remained Golden
v33 throughout. No reboot was performed for this cleanup.

The UbiG userspace engine was also moved from the lab `ubig-candidate` paths to
the stable production runtime `~/.local/lib/ubig/ubig-sp11.so`. The ALSA TLV
writer is rebuilt from tracked `tools/tlv_write.c` into
`~/.local/lib/ubig/tlv_write`; candidate-only systemd drop-ins/marker/staging
directories were retired to `00-RE-archive/ubig-candidate-retired-20260824/`.
A literal-zero production smoke completed one clean protection enable/disable
lifecycle with UbiG request/ack synchronized and no PA/XRUN fault markers.
