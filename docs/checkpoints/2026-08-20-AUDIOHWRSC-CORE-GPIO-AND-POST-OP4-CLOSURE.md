# 2026-08-20 AudioHwRsc core/GPIO and post-op4 closure

Date: 2026-08-20 (Europe/London)
Branch: `agent/psycho-bass-20260818`
Status: host speaker-start/resource layer closed below qcaudminiport/qcaucd; continue at qcadsp HWD4/WRDMA state dependency

## Golden safety baseline

After all disposable experiments, SP11 was returned to Golden v31 and directly verified:

- kernel: `7.1.5-sp11-render-parity-v4+`
- q6apm srcversion: `687B16CF9C43B43E90C0746`
- persistent GRUB saved entry: `sp11-audio-golden-v31`
- `next_entry=` empty
- qcadsp SHA256: `921870a839ee2aba647b04598d62ed96f3d2d5dfbb2499fc842f9a6ff0e0da13`
- ADSP DT/devcfg SHA256: `544bd795cb06cf8dee8119ede2a667f01066b2f1b9e4348f1772d080e2026ff4`

Golden remains the only persistent baseline. No candidate was promoted.

## Correct native-Windows AudioHwRsc clock set

The direct native speaker-runtime oracle supersedes the earlier static inference around `0x312`.

The marked Windows playback interval requests:

- `0x30c` `TX_CORE_MCLK`
- `0x314` `WSA_CORE_TX_MCLK`
- `0x315` `WSA_CORE_TX_2X_MCLK`

It does **not** request `0x312` in that interval.

The real Windows-only omission was therefore `0x315`. A probe module voted `0x315` at 19.2 MHz successfully.

Exact zero-ring A/B, on the already strongest source-wake candidate (`0x105c=0x0005000f` + DP13 `0x1d54=3`) with forced CPS TAP3:

- without `0x315`: 392 CPS frames, 0 nonzero frames, 0 nonzero PCM bytes
- with `0x315`: 392 CPS frames, 0 nonzero frames, 0 nonzero PCM bytes

Conclusion: `0x315` is a genuine native-Windows resource vote absent from Golden, but it is not sufficient to deliver CPS samples into the CODEC_DMA_SOURCE ring.

## AudioHwRsc hardware-core family

The same native Windows playback interval contains `PARAM_ID_RSC_HW_CORE` (`0x08001032`) for hardware block ID 1, i.e. LPASS hardware core.

Golden showed no per-render `q6prm_vote_lpass_core_hw()` call because Linux already holds the relevant resources continuously. Live clock-summary evidence showed:

- `LPASS_HW_MACRO`: prepare/enable count 6/6
- `LPASS_HW_DCODEC`: prepare/enable count 6/6

Therefore Windows's dynamic core-ID-1 request is not an absent Linux resource. Golden covers the vote more aggressively via persistent holds. No hardware-core candidate is justified.

## AudioHwRsc endpoint DSP-GPIO family

Recovered qcadcm `AudioHwRscIoctl` case 7 proves custom parameter `0x080014f3` is an LPI GPIO configuration payload. Each record carries GPIO pin, direction, drive, function select, GPIO type, and pull.

The native speaker request contains two logical records for GPIO 10 and GPIO 11. Those map directly to Denali/Hamoa LPASS-LPI pinctrl:

- GPIO10 -> `wsa_swr_clk`
- GPIO11 -> `wsa_swr_data`

Golden live pinmux confirmed both pins owned by `6b10000.soundwire` with those functions. Denali/Hamoa `wsa_swr_active` specifies drive-strength 2 for both pins, matching the active Windows GPIO payload's drive value 2.

Conclusion: the endpoint-GPIO request is a Windows dynamic representation of WSA SoundWire pin state already held by Linux pinctrl. It is not a hidden feedback dataplane attach command.

## qcaudminiport post-op4 path

`qcaudminiport` speaker start (`FUN_14007a4c0`) performs qcaucd operation 4 through `FUN_14006d8d0`. After successful op4 it conditionally checks miniport state at `+0x108` and may call `FUN_14006ceb8`.

`FUN_14006ceb8` dispatches qcaucd operation 8. qcaucd op8 maps to internal command `0x202`; reciprocal op9 maps to `0x203`.

Recovered semantics establish this as retained per-endpoint scalar/gain state:

- event/type `0x4a`
- per-endpoint cached scalar
- set path `0x202`
- query path `0x203`
- scalar mapping table is 0,3,6,...,60 for input values 0..20
- backend programs a narrow codec field rather than SoundWire master WRDMA/source geometry

The miniport setters that arm `+0x108` clamp/cache this same per-channel scalar.

Most importantly, prior controlled native-Windows runtime instrumentation already covered both qcaudminiport op8 and its resulting WSA register writes during an audible ~10 s 997 Hz speaker render. Neither fired. A normal speaker-start lifecycle reached op4 with **no op8 invocation**.

Disposition: keep the qcaudminiport op8 / qcaucd `0x202` retained-gain path closed as the missing VI/CPS feed activation.

## Host-layer closure

The ordinary native speaker-start path is now accounted for through:

1. qcaudminiport START
2. qcaucd op4 WSA owner/resource lifecycle
3. AudioHwRsc core family: covered persistently on Linux
4. AudioHwRsc clock family: real `0x315` omission tested and rejected as sufficient fix
5. AudioHwRsc endpoint GPIO family: live Linux pinctrl parity
6. WSA producer/slave transport and master geometry: previously parity-closed or individually rejected
7. conditional post-op4 op8: retained gain state and absent in the real render lifecycle

There is no remaining evidenced HLOS speaker-start/resource operation that plausibly attaches VI/CPS samples to the bit-identical qcadsp CODEC_DMA_SOURCE path.

## Remaining fault boundary

The strongest surviving boundary is now below the exhausted host lifecycle:

`bit-identical qcadsp -> HWD4 WSA CODEC_DMA_SOURCE -> LPAIF/WSA WRDMA state dependency -> zero sample ring`

Already proven:

- HWD4 opens for the correct WSA source instance
- normal HWD signal interrupts repeatedly trigger source processing
- the source topology emits hundreds of correctly framed diagnostic periods when `0x105c + 0x1d54` wakes packetization
- those periods contain byte-for-byte zero PCM
- producer-side WSA8845 sensing, slave DP5/DP6 transport, WSA RX macro, host resource clocks/core/GPIO, graph structure/start, and firmware/devcfg parity are closed

Next work must identify an external or retained state dependency consumed by the HWD4/WRDMA open/start/read path, not invent another qcaudminiport/qcaucd lifecycle operation.

Promotion gate remains nonzero 8 kHz VI / 24 kHz CPS PCM on Linux during acoustically proven render, with Golden v31 preserved as persistent fallback.
