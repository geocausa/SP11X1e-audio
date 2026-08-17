# WSA8845 ordinary clock-stop retention v27 and static-source boundary — 2026-08-17

## Verdict

**GREEN ordinary lifecycle retention; physical zero-noise parity remains RED/AMBER-open.** Candidate v27 fixes a concrete Linux lifecycle error discovered after the exact Windows 63/10/6 WSA8845 state machine was implemented in v26. The remaining muted-zero static is now repeatable rather than cycle-dependent, and a synchronized mid-stream mute A/B places it downstream of (or independent from) the WSA-macro RX digital sample path.

This finding does not claim system suspend/resume parity. That remains explicitly deferred.

## v26 root cause: ordinary clock-stop was treated as cold context loss

The v26 register-level START/STOP implementation itself was correct: three natural cycles produced exactly 96 WSA writes, i.e. `3 × 2 amps × (10 START + 6 STOP)`, with every write matching the shipping Windows oracle.

The failure was one layer lower in SoundWire status handling. A kprobe on `wsa884x_update_status()` directly captured `SDW_SLAVE_UNATTACHED` (`status=0`) followed by `ATTACHED` (`status=1`) during ordinary speaker activity/clock-stop behavior. The generic callback interpreted every UNATTACHED as hardware context loss: it cleared `hw_init`, enabled cache-only mode, marked the complete regcache dirty, and on ATTACHED performed a full sync plus `wsa884x_init()`.

That was observably wrong for this resident SP11 clock-stop path. A normal wake repeated the `detected VPHX supply configuration: 2S` initialization log and changed both amplifiers from the UCM/runtime value `DRE_CTL_1=0x0e` to the 63-write cold-oracle value `0x00`. The visible ALSA PA control simultaneously read back as 31 instead of the intended 24. The same event also resurrected the multi-second WSA startup path.

This explains how v26 could show perfect 10/6 visible runtime writes yet alternate between materially different acoustic states.

## v27 change

Patch `0064` adds a Denali-only resident-clock-stop branch when the SP11 Windows profile is active and `hw_init` is already true:

- ordinary `UNATTACHED`: enter regcache cache-only mode, but retain `hw_init` and do **not** mark the entire cache dirty;
- following `ATTACHED`: leave cache-only mode and sync only genuine writes accumulated while suspended; do **not** rerun the 63-write cold initialization;
- first attach remains unchanged because `hw_init` is initially false;
- generic non-SP11 behavior is unchanged.

The v27 module has srcversion `74B81D565B3BFEAB48F9452` and exact running-release vermagic `7.1.5-sp11-render-parity-v4+`. It is signed with the existing render-parity-v4 kernel key. The isolated v27 initrd contains 4,394 entries and differs from v26 in exactly one regular file: `snd-soc-wsa884x.ko.zst`. CPS-v3 remains the persistent GRUB fallback.

## Live retention gate

After a 20-second idle, a muted zero stream reached ALSA RUNNING in **150 ms**. The boot-time WSA initialization count stayed at exactly two (one per physical amplifier) before and after playback. Both raw codecs remained `DRE_CTL_1=0x0e` after teardown.

The dedicated WSA register trace contains exactly **32 ordinary writes**: two amplifiers times the exact Windows 10-write START plus 6-write STOP. No 63-write cold replay, DSM rewrite, DRE rewrite, current-limit rewrite or watchdog rewrite occurred. This restores the ordinary cold-start latency result and fixes the state-clobber mechanism.

## The physical static remains, now stationary

The standard external-microphone digital-zero gate was repeated at RX81, PA Volume 24 / raw `DRE_CTL_1=0x0e`, visible endpoint 1% and muted. v27 stayed initialized (`init_count=2`), retained raw `0x0e`, and reached RUNNING in 100–150 ms on repeated cycles.

Yet the active physical noise remained large and reproducible:

- clean A median steady diff-RMS: `0.0021256792` (~116.5× the Windows reference);
- clean B: `0.0023499975` (~128.7× Windows);
- Windows reference: `0.0000182532`.

Thus the v27 lifecycle fix is real but does not itself solve W03. It converts a non-stationary state problem into a stationary hardware-side failure that can be localized cleanly.

## DRE_CTL_1=0 remains rejected

A one-variable v26 test explicitly set both `Spkr* PA Volume` controls to 31, producing raw `DRE_CTL_1=0x00` as in the Windows cold transaction. That did **not** make Linux quiet: median steady diff-RMS rose to about `0.00211847` (~116× Windows). The safe PA24/raw-`0x0e` state was restored immediately. Do not promote a blind `DRE_CTL_1=0` policy.

## Decisive mid-stream RX digital-mute boundary

To determine whether the static was carried by ordinary digital audio, a 16-second zero stream was started on v27 and allowed to reach RUNNING. With the PA remaining active, both `WSA WSA_RX0/RX1 Digital Mute` controls were then switched ON, verified ON, held for about five seconds, and switched OFF again while SP7 continuously recorded the external microphone.

Median microphone diff-RMS was:

- room before PA wake: `0.0000177298`;
- active before RX mute: `0.002325299`;
- RX digital mute ON: `0.002289567`;
- active after RX mute: `0.002303676`.

The mute interval is 98.46% of the pre-mute noise and 99.39% of the post-mute noise. In other words, muting the normal WSA-macro RX sample path changes the static by only about one percent while the noise remains roughly 129× the room floor.

**Therefore this static is downstream of, or independent from, the WSA-macro RX digital sample path.** Do not reopen Dolby, q6apm, endpoint-volume processing, or ordinary PCM sample data as the cause of this specific noise. The next investigation belongs at the WSA8845 analog/PA state, SoundWire transport/clock state, or another non-sample-data coupling.

Machine-readable evidence is in `artifacts/reviewed/2026-08-17-linux-wsa8845-clockstop-retention-v27.json`.
