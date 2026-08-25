# 2026-08-25 MicArray EP16 Linux push transport build gate

Windows is the oracle. This checkpoint freezes the first Linux implementation of the measured SP11 MicArray EP16 host transport.

## Windows contract implemented

The fresh-boot EP16 oracle established:

- `AllocateBufferV2`: mode 2, one 3840-byte ring, two watermarks, graph type 2.
- SH_MEM_PUSH_MODE is IID `0x40dc`, MID `0x07001007`.
- `PARAM_ID_SH_MEM_PULL_PUSH_MODE_CFG = 0x0800100a` binds the circular audio ring and separate position page.
- watermark event `0x0800101c` uses levels `{1920, 3840}`.
- position page ABI is `{frame_counter, index, timestamp_us_lsw, timestamp_us_msw}`; `index` is the DSP byte producer cursor modulo the ring.
- EP16 goes from OPEN directly to START; no GRAPH_PREPARE was observed.
- capture format is 48 kHz, signed 16-bit, stereo, interleaved.
- Windows does not queue RD_SHARED_MEM read buffers for this endpoint.

## Patch 0076

`patches/0076-ASoC-q6apm-SP11-add-Windows-EP16-push-capture-transport.patch`

SHA256:

```text
a1bd2f3510d077f35c761bbef620a4e52c85c95cde8815104079776a7df4a019
```

The patch:

- detects SH_MEM_PUSH_MODE capture graphs independently of the existing protected-render pull mode;
- allocates/maps the uncached DSP position page for push graphs;
- configures IID `0x40dc` with the measured 3840/1920-byte circular transport;
- registers the measured watermark event;
- does not queue conventional RD_SHARED_MEM reads;
- bypasses GRAPH_PREPARE for push mode;
- drives the ALSA capture pointer from the DSP-owned position-page `index` using the measured frame-counter consistency rule;
- constrains the path to the measured 48 kHz/S16/stereo format;
- does not invent a media-format command for SH_MEM_PUSH_MODE itself.

The PCM format remains owned by the normal graph format path, including the measured EP16 PCM_CNV terminal.

## Static hygiene

Final patch:

```text
checkpatch: 0 errors, 0 warnings, 405 lines checked
patch --dry-run: PASS
```

## Combined build gate

Applied together to the exact Golden-v33 kitchen:

```text
0072 ASoC lpass-va-macro SP11 Windows DMIC divider
0073 ASoC lpass-va-macro SP11 Windows VA sequencing
0074 ASoC audioreach SH_MEM_PUSH_MODE topology
0075 ASoC lpass-tx-macro multichannel capture state fix
0076 ASoC q6apm SP11 EP16 push capture transport
```

The complete series built successfully. Candidate modules from the final source-body revision:

```text
snd-q6apm.ko
SHA256 44a4dbe887adbe78ac461c42a8b19b5667dff35ab7f844ab8238f8f90ab90724
srcversion CA0C1C65C785B369BB8B76C
vermagic 7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64

snd-soc-lpass-tx-macro.ko
SHA256 ba70360d68fb9de604f404889ef461bfca06d534c60163a520eef0f18a7e4950
srcversion 92B19C694B21C9DE32F7E15
vermagic 7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64

snd-soc-lpass-va-macro.ko
SHA256 30ee0704348c0e7e78aadead8e381987447351d0e46db387e949ee0dd9c65978
srcversion 81ECE21DD3204E5F3BBFB67
vermagic 7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64
```

All vermagic strings exactly match the running Golden-v33 kernel.

## Golden restoration gate

After the combined build, independent byte-for-byte recursive comparisons passed for:

```text
SRC_QDSP6=PASS
OUT_QDSP6=PASS
SRC_CODECS=PASS
OUT_CODECS=PASS
ROOT_Module.symvers=PASS
ROOT_modules.order=PASS
ROOT_.modules.order.cmd=PASS
```

Golden-v33 was therefore not mutated by the candidate build.

## Remaining lifecycle boundary

This patch intentionally does not change final PCM-close ownership. The fresh Windows oracle independently proved that EP16 continues producing after the recording application exits, so client close and endpoint graph teardown are not the same boundary. Persistent graph ownership is the next separate patch/review target rather than being mixed into the host-transport implementation.
