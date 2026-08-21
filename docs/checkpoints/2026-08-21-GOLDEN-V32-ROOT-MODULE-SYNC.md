# 2026-08-21 — GOLDEN v32 root module synchronization

After promotion and v32->v32 reboot validation, the root module tree was synchronized with the promoted modules so a later manual unload/reload cannot silently fall back to older root copies.

Installed root identities:
- `snd-soc-lpass-wsa-macro`: `F32C7A03F713D1B20F0BF78`
- `snd-soc-wsa884x`: `5859E70AFD0A1D420E8ADD4`
- `soundwire-qcom`: `D008A3D6B585C11BE023992`

All are signed with the existing Golden build key and retain kernel vermagic `7.1.5-sp11-render-parity-v4+`.

The displaced root copies were backed up under:
`/usr/local/lib/sp11-audio/v31-root-modules-backup-20260821`

Backup identities:
- WSA macro: `4AF6F542C17BA6DD46586DA`
- root WSA8845 prior copy: `EB74C0F5E4405EEE429136C`
- SoundWire qcom: `406975A3ED60935B31491BF`

Golden v31's fixed boot initrd was not modified; its boot-time fallback module set remains intact.

`depmod 7.1.5-sp11-render-parity-v4+` completed and `sp11-audio-v32-verify` passed afterward.
