# 2026-08-24 — Golden v33 root-module synchronization

Golden v33 initially booted the corrected WSA macro from its fixed initrd while
the ordinary root module tree still held the v32 copy. That creates a
maintenance trap: a later initramfs regeneration could silently restore v32.

The root module was therefore synchronized to SHA-256
`39674078b0781323464b3de647caf9db0b25cde51d447e8e0253630de91d3f2d`
after backing up the displaced v32 root copy under
`/usr/local/lib/sp11-audio/v32-root-modules-backup-20260824`, followed by
`depmod 7.1.5-sp11-render-parity-v4+`.

The fixed v33 initrd remained byte-identical at
`19db416046a363821f1d0887a43562d69c3593f6df85b7b16017adcc6bc59a44`.
Golden v32 fixed boot assets were not modified.
