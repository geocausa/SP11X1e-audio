# WSA8845 current-limit init v12/v12R provenance and rejection — 2026-08-17

## Provenance correction: original v12 was not an experiment

The first `ilim44-init-v12` boot was intended to add `CURRENT_LIMIT=0x44` to SP11 codec initialization. A post-run binary/source audit proved the candidate source file was byte-identical to v5 and the packaged module was byte-for-byte the signed v5 module (`feea9384...` compressed). Its acoustic capture is therefore only another v5 control and **must not be cited as a current-limit result**.

## v12R: provenance-clean isolation

The corrected v12R candidate starts from exact v5 source SHA-256 `f5555cfde5f8c72001a779ac9d0dc0aac527284e88c6333a450027af4f340f97` and adds one SP11 2S init write immediately after `PWRSTAGE_CTRL2=0xf1`:

`CURRENT_LIMIT = 0x44`

The after-source SHA-256 is `edfbfd7d37f274723bea449387fdae19e30368531146d17a284750705533c8d9`; patch SHA-256 `bee1c856866ccdfdae6638f0559cb3c649c842387bd99d63e6377b6aec2da5db`. Fresh source-tree output was required. Executable `.text` differs from v5 and disassembly proves `regmap_write(0x3091,0x44)` directly after the `0x30a5=0xf1` write.

Signed module SHA-256 `e388b627a80b277055199ff6d9389b401b1379a30701fdf983b383e35e2ab708`; compressed SHA-256 `ecb7f3814b70ab0d66660b0841a2cbf8bc6197d4bd786473647285079b76a423`; loaded srcversion `67590C1AADAD9EC262762E8`. The initramfs used the exact proven v5 WSA-macro and x1e modules and was byte-audited before boot.

## Digital-zero result

Endpoint stayed at 1% and muted. The source was 48 kHz stereo S16_LE all-zero PCM, SHA-256 `87d8420ddaf7d56d3f5068c6a74362451fc2859197445490d15e7b3d456fa22e`. Physical ALSA PCM was independently `RUNNING` during each test and returned `closed`.

Two PA cycles on the same v12R boot were radically different:

- first capture SHA-256 `FB4A2C22ED3EB54AA3714AFF1973D032601E827157B4492800759494558532CF`: active-window diff-RMS median approximately `1.98e-4` (limited/short plateau, about 0.29x v5 and 10.8x Windows);
- second capture SHA-256 `6074A20E989F3381759D65F4C24863470A75A434923989BF3DAE230C5572FD40`: sustained diff-RMS median `3.8369e-3`, about **5.67x v5 and 210x Windows**.

The second capture is a sustained two-channel broadband plateau, not a room impulse. No music/chirp escalation was performed.

## Interpretation and decision

**Reject v12R.** Establishing final `CURRENT_LIMIT=0x44` early is not sufficient and makes the CSR-off state strongly cycle/history dependent. This is direct evidence that the manufacturer's current-limit/analog initialization must be treated as an ordered state transition, not a final-value assignment.

Linux already reaches `0x44` at PBR-enabled speaker POST_PMU by clearing override and programming current code `0x11`; v12R changed only the history before that lifecycle. Windows instead executes a coherent analog tail containing `CURRENT_LIMIT 0xd4 -> 0x44` with reference/boost/OCP/VCM/CKWD/power-stage programming between the two writes. The next candidate must reproduce a coherent, address-audited Windows block rather than another isolated final-value write.
