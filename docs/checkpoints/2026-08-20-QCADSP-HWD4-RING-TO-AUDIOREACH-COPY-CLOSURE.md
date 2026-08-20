# 2026-08-20 qcadsp HWD4 ring -> AudioReach copy closure

Date: 2026-08-20 (Europe/London)
Branch: `agent/psycho-bass-20260818`
Status: CODEC_DMA_SOURCE software copy/staging layer closed; remaining fault boundary is upstream of the HWD4 WSA DMA ring

## Golden safety baseline

SP11 remained on Golden v31 throughout this static RE pass:

- kernel: `7.1.5-sp11-render-parity-v4+`
- persistent GRUB saved entry: `sp11-audio-golden-v31`
- `next_entry=` empty
- no candidate boot or module replacement was performed

## Exact CODEC_DMA_SOURCE factory direction

AudioReach defines:

- `MODULE_ID_CODEC_DMA_SINK = 0x07001023`
- `MODULE_ID_CODEC_DMA_SOURCE = 0x07001024`

The qcadsp registry record for `0x07001024` points to:

- static-properties entry: `0xb0598674`
- init entry: `0xb0598678`

The source init trampoline is explicit:

```text
b0598678  immext #0xffffff40
b059867c  assign R2,#0x2 ; jump 0xb05985e8
```

`forced_b05985e8()` stores that third argument at CODEC_DMA instance `+0x20`.
Therefore CODEC_DMA_SOURCE runs with direction/state value **2**, not 1.

## Exact process branch used by CODEC_DMA_SOURCE

The recovered CAPI process function at `0xb005ebcc` branches on instance `+0x20`:

- value 1 -> `FUN_b0064b40()`
- otherwise -> the `FUN_b005f224()` path

Because CODEC_DMA_SOURCE is initialized with value 2, it takes:

```text
CAPI process
  -> FUN_b005f224(instance + 0x10, ...)
  -> FUN_b005f40c(HWD handle at subobject +0x80)
```

This is the HWD **ring-read** path.

## HWD4 ring object and hardware position

`FUN_b005f40c()` operates directly on the allocated HWD object:

- `+0x24` DMA ring virtual base
- `+0x28` DMA ring physical base
- `+0x2c` software consumer/read cursor
- `+0x14` ring size
- `+0x20` period/transfer word count
- `+0x4c` HWD interface ID
- `+0x50` direction
- `+0x54` allocated DMA channel

It first calls the HWD dispatch (`FUN_b005d1bc`) to obtain the live hardware DMA position, computes unread data relative to the software cursor and ring base, handles wraparound, and only then consumes a period.

The ring is not a synthetic AudioReach staging allocation. HWD construction proves:

1. `FUN_b027c968()` allocates the DMA buffer and stores its virtual address at HWD `+0x24`.
2. `FUN_b0039d2c()` converts that buffer to the physical address stored at HWD `+0x28`.
3. `FUN_b027bd48()` places HWD `+0x28` into the DMA-programming descriptor (`local_48`).
4. That descriptor is passed through `FUN_b006a4c8()` to the selected hardware backend.

Thus HWD `+0x24` is the software mapping of the same DMA ring whose physical base is programmed into the WSA LPAIF DMA engine.

## WSA HWD identity

Previously recovered devcfg makes the identity exact:

```text
AR hardware interface type = 2 = LPAIF_WSA
CODEC_DMA descriptor HWD interface = 4
HWD4 main base = 0x06b80000
HWD4 DMA window = 0x06b8e000
HWD4 IRQ window = 0x06b9a000
backend type = 0x50000000
```

So the ring-read path above is specifically the SP11 WSA LPAIF DMA backend, not a generic or accidentally selected interface.

## Exact ring -> framework copy callback

During HWD open, the source direction (`instance +0x20 == 2`) selects `FUN_b005a110` as HWD callback `+0x64` and stores the CODEC_DMA format context at HWD `+0x6c`.

`FUN_b005f40c()` performs cache maintenance on the unread ring region, then invokes HWD `+0x64` with:

1. format/context
2. current DMA-ring address
3. AudioReach output-buffer descriptors

`FUN_b005a110()` is a deterministic ring-to-framework converter. It reads the interleaved DMA words directly from the supplied ring address, deinterleaves/converts them into the per-channel framework buffers, and updates the output lengths.

There is no additional CODEC_DMA sample staging buffer between HWD4 and the module output.

The complementary callback `FUN_b005b2ac` is the opposite direction (framework -> DMA ring) and belongs to the sink/write path; it is not the CODEC_DMA_SOURCE read path.

## Correction: instance +0x348 is not sample storage

The earlier tentative interpretation of CODEC_DMA instance `+0x348` as a sample pointer is rejected.

The process path uses the 64-bit value at `+0x348/+0x34c` in the timestamp/timing calculation path (`FUN_b005f370` -> `FUN_b0061ef8`) and writes it into framework metadata. It is not the WSA sample ring.

## Consequence for the zero-payload result

The earlier forced TAP3 experiment proved hundreds of correctly framed CPS periods while every PCM byte remained zero.

This new RE closes the possibility that CODEC_DMA_SOURCE is merely reading a wrong software staging buffer after HWD4 interrupts. Its source process path consumes the actual HWD4 DMA ring and immediately converts/copies that ring into AudioReach output buffers.

Combined with the bit-identical Windows/Linux qcadsp and ADSP devcfg, correct source configuration, successful HWD4 open, and repeated normal HWD interrupts, the remaining fault is now upstream of the CODEC_DMA software-copy layer:

```text
WSA8845 / SoundWire feedback producer
    -> WSA SoundWire master receive path
    -> LPAIF_WSA WRDMA producer
    -> HWD4 DMA ring
    -> [closed: qcadsp ring-read + FUN_b005a110 copy]
    -> AudioReach / DATA_LOGGING
```

The next work should reconstruct the exact HWD4 WRDMA producer register state and identify which upstream mux/route/state can leave the DMA engine interrupting while the programmed ring contains no meaningful VI/CPS data.

Promotion gate remains nonzero VI/CPS PCM on Linux. Golden v31 remains the only persistent baseline.
