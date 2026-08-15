# WSA pre-sync observation candidate

This directory records the isolated trace-only CPS-v3 experiment used on 2026-08-15 to compare selected LPASS WSA-macro hardware registers immediately before and after the driver's existing first-MCLK `regcache_sync()`.

The candidate is diagnostic only. It must not be confused with a production audio candidate and it makes no new hardware writes.

Signed candidate module staged on the SP11 during the experiment:

- srcversion: `6CAE2EF0203C16B82CF0892`
- vermagic: `7.1.5-sp11-cps-v3+ SMP preempt mod_unload modversions aarch64`
- signature key: `5A:58:B6:3A:CA:17:D4:5F:02:D0:58:3E:39:7E:39:87:55:B5:62:47`
- uncompressed SHA-256: `d125f1c3d5b00d0b1848518e67c2e0df5fdba43a92cf529aa501a1553c413d49`
- compressed SHA-256: `0ad27b6298d31a3cd56bdff1db961b85ab5041fb19a04b1caaf3365b2b8cf0b7`

The temporary initramfs hook was removed immediately after the custom image was built. The custom `/etc/grub.d` entry was removed after the observation. Persistent fallback stayed `sp11-audio-cps-v3` throughout.

Result: all 37 selected WSA registers were identical before and after sync. See `docs/findings/2026-08-15-WSA-PRESYNC-STATE-CLOSEOUT.md` and the reviewed JSON/log artifacts.
