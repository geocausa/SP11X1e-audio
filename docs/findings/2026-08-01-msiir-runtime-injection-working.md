# Runtime MSIIR injection path — working (2026-08-01)

Status: **the user-space to DSP parameter injection path is built, deployed and
verified.** This was the missing link for the Dolby port.

Read this before doing anything with MSIIR, Dolby, or DSP tuning.

---

## 1. What was actually missing, and why earlier attempts failed

Windows' `DolbyAPOvlldp150` does **not** filter audio in user space. It:

1. analyses the stream,
2. computes MSIIR filter coefficients,
3. **injects them into the MSIIR modules already present in the DSP graph**
   (via `gsl_set_custom_config`),
4. and the DSP does the filtering, ahead of speaker protection.

Linux had no equivalent. Module parameters were only ever sent during graph
setup, so MSIIR sat at whatever the topology loaded for the entire life of the
stream. On this machine `0x489e` was loaded with unity (pass-through)
coefficients and never updated.

Earlier prototypes (LADSPA plugin, PipeWire filter-chain, Python processors)
filtered **samples** in user space. That is architecturally different: the DSP's
own MSIIR stayed flat, the processing happened in the wrong place, and the
result did not resemble Windows. The operator's own diagnosis of this was
correct: "it wasn't feeding properly back into the DSP".

The gap was never the algorithm. It was the delivery path.

---

## 2. What was built

### Kernel — `sound/soc/qcom/qdsp6/audioreach.c`

`audioreach_sp11_inject_module_param(apm, iid, param_id, payload, size)`

Builds an `APM_CMD_SET_CFG` packet addressed to a single module instance and
sends it to the **live** graph via `q6apm_send_cmd_sync()`. It deliberately
mirrors the existing `audioreach_gain_set_vol_ctrl()` rather than inventing a
new pattern.

**Allowlist.** Only MSIIR instances `0x489e` / `0x48a1`, and only params
`0x08001020` (enable), `0x08001021` (pregain), `0x08001022` (coefficients).
Anything else returns `-EPERM` and logs a warning. This matters: these modules
sit directly beside `SPEAKER_PROTECTION` (`0x4027`) and
`SPEAKER_PROTECTION_VI` (`0x4024`) in the same graph. Do not widen the
allowlist without understanding what the target module does.

### Kernel — `sound/soc/qcom/qdsp6/q6apm.c`

ALSA bytes control `SP11 MSIIR Inject`, registered in `q6apm_audio_probe()`.

Write format, little-endian, **after** the ALSA TLV header:

```text
u32 instance_id
u32 param_id
u32 payload_size
u8  payload[payload_size]
```

### User space — `01-audio/tools/`

| File | Purpose |
|---|---|
| `tlv_write.c` + `bin/tlv_write` | writes an ASoC bytes-TLV control |
| `sp11_msiir_inject.py` | probe, unity test, raw payload send |
| `sp11_msiir_filter.py` | builds stability-checked biquads and sends presets |

---

## 3. Verified results

All on kernel `7.1.5-sp11-audio-diag-observe+`, audio playing.

```text
control                numid=32, name='SP11 MSIIR Inject', type=BYTES,
                       values=1152, tlv_writable=1
disallowed iid 0x9999  rc=-1 EPERM
                       kernel: "SP11 inject refused: iid 0x9999
                                param 0x8001022 not allowlisted"
MSIIR 0x489e unity     rc=0
MSIIR 0x48a1 unity     rc=0
MSIIR both, real EQ    rc=0   (+4dB @250Hz Q0.9, -3dB @2.5kHz Q1.0)
```

Every layer is proven by this: user space reaches the kernel (the allowlist
warning shows the handler parsed the iid correctly), the allowlist protects the
protection modules, and the DSP itself returns success for the real instances.

**No `-22`.** June 2026 Ghidra work on the ADSP image found that MSIIR gates its
tuning parameters behind the CAPI-initialised flag at `context+0x68d`, set only
by the media-format init path, and predicted rejection otherwise. With the graph
running, parameters are accepted. That question is now settled empirically.

**Audio must be playing.** An idle graph is expected to reject the write.

---

## 4. Traps that cost time — do not repeat

1. **`amixer cset` cannot write these controls.** It attempts a read first and
   fails with `Cannot read the given element from control hw:0`. Use
   `snd_ctl_elem_tlv_write()`; that is what `tools/bin/tlv_write` does.

2. **`snd_soc_bytes_tlv_callback()` passes the WHOLE TLV buffer to `put`,
   including the 8-byte ALSA TLV header** (type, length). The handler must skip
   two `u32` words before reading its own header. Getting this wrong produced
   `rc=-22` with **no kernel message at all**, which looks exactly like a DSP
   rejection but is not.

   Diagnostic rule: **no kernel log line means the handler never ran** and the
   fault is on the Linux side. A DSP rejection reaches the driver and can be
   logged.

3. **`/tmp` is cleared on reboot.** Build helpers into the repo, not `/tmp`.

4. **Instance IDs have changed across topology rebuilds.** Historical documents
   reference `0x4b00`/`0x4b01` (June Linux) and `0x47ee`/`0x47ef` (Windows).
   The currently deployed instances are **`0x489e` and `0x48a1`**. Always check
   the live topology before targeting anything.

---

## 5. Deployed MSIIR state at time of writing

From the installed topology (`alsatplg -d`, param `0x08001022`):

| Instance | Payload | Content |
|---|---|---|
| `0x48a1` | 164 bytes | real biquads, e.g. `+0.7934 -1.5688 +0.7787 -1.9750 +0.9793` |
| `0x489e` | 96 bytes | header claims 2ch/6 stages, but supplies only 4 stages, all unity |

`0x489e` is the **last** stage before the SoundWire sink, and it was passing
audio through untouched. That is the slot Dolby would be driving on Windows.

Chain order: `data_logging.467a -> msiir.48a1 -> msiir.489e -> swr_sink.4675`.

---

## 6. Where this leaves the Dolby port

Everything needed now exists on the machine:

| Component | State |
|---|---|
| MSIIR modules in the DSP graph | deployed, 2 instances |
| Speaker protection + VI feedback | verified working, 8 kHz both amps |
| R0/T0 calibration | byte-identical to Windows (4.956Ω/38.7°C, 5.370Ω/37.0°C) |
| Graph calibration | 106 of 107 frames accepted |
| Decompiled VLLDP processor | all 17 callees documented, Python model exists |
| Bit-exact Dolby bass coefficients | 196 values in `bass_coefficients.h` |
| **Runtime injection path** | **working as of 2026-08-01** |

The remaining work is **integration**, not research: drive the existing VLLDP
model's output into this control.

Unresolved and worth settling early: the project contains two contradictory
claims about Dolby's behaviour. `SESSION_PROGRESS_20260604.md` says static
presets pushed once at toggle time, not adaptive. `CLAIMS_LEDGER_20260608`
says level- and program-dependent. If it is static, one captured coefficient
set finishes the job. If dynamic, the VLLDP model must run continuously.

---

## 7. Safety notes

* Speaker protection is live and verified, which is what makes bass-lift
  testing acceptable at all. Do not assume that holds after any topology or
  kernel change — re-verify `SP11 stage ... accepted` lines and the
  `VI feedback stream: rate=8000` lines first.
* `sp11_msiir_filter.py` stability-checks every biquad (poles inside the unit
  circle, Q30 fits int32) and refuses to send an unstable one. Keep that check
  if the tool is rewritten.
* Low **shelf** filters are deliberately not offered. A shelf lifts everything
  below its corner, which is exactly where these small drivers have the least
  excursion headroom. Peaking filters are bounded on both sides.
* This kernel change is in the **diagnostic kernel only**. It is not in
  `audio-vi`, which remains the known-good rollback entry.

---

## 8. Reproducing the test

```sh
# boot: Ubuntu SP11 7.1.5 AUDIO DIAGNOSTIC (observation only)
cd ~/Documents/SP11-PROJECT/01-audio

# start audio first, the graph must be live
speaker-test -D default -c 2 -t pink &

./tools/sp11_msiir_inject.py --probe          # expect numid + "graph running"
./tools/sp11_msiir_inject.py --unity          # expect rc=0
./tools/sp11_msiir_filter.py --list
./tools/sp11_msiir_filter.py --preset warmth --both
./tools/sp11_msiir_filter.py --preset flat    # restore
```

If `tools/bin/tlv_write` is missing:

```sh
gcc -o tools/bin/tlv_write tools/tlv_write.c -lasound
```
