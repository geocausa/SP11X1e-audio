# WSA8845 PDM-watchdog-latched v14 — first-cycle rejection

## Why v14 was tested

Static qcaucd writer inventory establishes that Windows writes `PDM_WD_CTL=1` during codec initialization and does not touch `0x348b` in its ordinary PA start/stop functions. Linux, in contrast, writes `PDM_WD_EN=1` at speaker POST_PMU and clears it to zero on every PRE_PMD. v13 had materially lower noise on cycle 1 and catastrophic noise on cycle 2, so the Linux-only watchdog clear was a plausible state-loss boundary.

## Candidate

v14 is exact v13 plus one lifecycle change: SP11 2S PRE_PMD no longer clears `PDM_WD_EN`. Non-2S behavior remains upstream. Machine-code audit proves the 2S PRE_PMD branch bypasses the `0x348b=0` write while POST_PMU still enables it.

Source before SHA-256 `5b747b52...91f1e`; source after `ee284b4e...2ec55`; patch `2d03b1cf...5b49`; signed module `bb5304e5...c1bf`; compressed module `4b5a5a4d...2cb6`; srcversion `C639E72843D5AF566FC255B`; initramfs `b2683217...bbfc`. Exact v5 WSA-macro/x1e modules were force-loaded.

## Muted digital-zero gate

Exact 10-second all-zero source SHA-256 `87d8420d...fa22e`, endpoint 1% and muted. PCM was `RUNNING` during the stream and returned `closed`.

Cycle 1 capture SHA-256 `4A59D8447B4FF9563FF0C1AD6161D54ACB2219767DABCC517F8C95D1B8B3B7F7` produced median steady diff-RMS `4.36489e-3`: **6.45x v5 and 239x Windows**. The broadband plateau was sustained on both channels.

## Decision

**Reject v14 immediately.** No second cycle or program audio was run.

This negative result is structurally important. Windows can leave the PDM watchdog enabled while its own idle/PA-off lower stack remains coherent; Linux cannot simply retain that amp state while its passive host path fully stops/deprepares the producer and SoundWire stream. H03 therefore moves below the isolated WSA8845 register layer into **transport/clock-stop/datapath state preservation across PA-off idle**. The next work must compare Windows and Linux SoundWire/producer lifecycle before another amp-register candidate.
