# SP11 Windows WSA HD2 isolation: structurally correct, not the final acoustic lever

Date: 2026-08-16  
Status: **preserve structural correction; no standalone acoustic win**

## Candidate and hard gate

`sp11-audio-rpv4-macro84-winproducer-nohd2-v3` changed only `wsa_macro_hd2_control()` relative to the already Windows-aligned producer v2: the generic Qualcomm HD2 compensation routine became a no-op. Compander, Surface curve, primary half-dB-off, VBAT/BCL, CB_DECODE, RX84, TOP_CFG1, protection, Dolby and the safe CSR-assisted WSA8845 lifecycle were unchanged.

The signed module loaded as srcversion `4AF6F542C17BA6DD46586DA`; compressed SHA-256 is `05D19A94C21B5A7683922E024F714877D744B588CF59C1152EA694F401E4B530`. The initramfs SHA-256 is `7668ED49F0ADC558A2647A7DE1E8BF11296EDFF874EEEC7650B5AE5AD2FCB935`; it was unpacked and byte-verified with the exact known RX84 X1E module before boot. Persistent fallback remained `sp11-audio-cps-v3` and the one-shot cleared after boot.

Before any acoustic stimulus, the runtime-active Linux regmap had the exact intended Windows render state:

- RX0/RX1 CFG0 = `0x02` (compander enabled, HD2 bit clear);
- RX0/RX1 SEC3 = `0x00` (no HD2 alpha/scale programming);
- RX0/RX1 CFG1 = `0xef` once VBAT was active;
- RX0/RX1 CFG2 = `0x8f`;
- TOP_CFG1 = `0x03`;
- primary half-dB register = `0x08`;
- Surface compander curve, BCL bytes, softclip clocks and CB_DECODE remained at the Windows-proven values.

After a real deterministic playback the same HD2/volume gate still passed and no new WSA/PA/SoundWire/XRUN fault appeared.

## Acoustic result

Five SP7 external-mic captures were taken with the exact fixed chirp, endpoint 12%, and WSA RX84/0 dB. The existing synchronized Windows/RX84 oracle and the same ridge extractor were reused.

Best known RX84 generic producer remains:

- 1--5 kHz: **0.182 dB MAE / 0.208 dB RMSE**;
- 630 Hz--6.3 kHz: **0.184 / 0.214 dB**.

No-HD2 v3 five-run median:

- 1--5 kHz: **0.527 / 0.571 dB**;
- 630 Hz--6.3 kHz: **0.454 / 0.532 dB**.

Individual 1--5 kHz runs were approximately A `0.472/0.541`, B `3.656/4.253` (clear environmental/capture outlier), C `0.641/0.701`, D `0.223/0.288`, E `0.544/0.676`. The microphone set is more variable than desired, but even the two ordinary non-outlier runs A/C are worse than the RX84 generic baseline. Run D approaches the baseline but does not establish a repeatable win.

Large WAVs remain on SP7; their hashes are retained in `artifacts/reviewed/2026-08-16-rpv4-macro84-winproducer-nohd2-v3-result.json`.

## Interpretation

This does **not** justify restoring generic HD2. Native Windows directly disproves HD2 on the SP11 WSA render path, and v3 proves Linux can operate safely with the same CFG0/SEC3 state. The result instead sharpens the remaining coupling problem: the macro producer can now be brought very close to the Windows active state, while Linux still differs at the WSA8845 consumer lifecycle.

Windows initializes both WSA8845 `DRE_CTL_1` registers to `0x00` and does not re-enable CSR fallback on ordinary PA start/stop. Native Linux's `mute_stream(..., 0)` unconditionally sets `CSR_GAIN_EN` before `GLOBAL_PA_EN`, producing the familiar active `DRE_CTL_1=0x0f`. The earlier isolated attempt to copy `DRE_CTL_1=0` on the old/generic producer generated unsafe noise and remains rejected. It must not be repeated by simply forcing a register.

The next safe task is code/lifecycle reconstruction: determine the minimal WSA8845 unmute change that reproduces Windows only when the now-correct producer/COMP path is already ready, validate ordering at zero/non-program audio, and only then consider a new coupled low-level candidate. DRE/CSR must be treated as a lifecycle semantic, not a mixer tweak.
