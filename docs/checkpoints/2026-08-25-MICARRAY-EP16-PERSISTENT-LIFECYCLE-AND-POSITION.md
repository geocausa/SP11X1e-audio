# 2026-08-25 MicArray EP16 persistent lifecycle and position-buffer closure

Windows is the oracle for this work. This checkpoint records a fresh-boot SP11 Windows run in which KD stopped on qcadcm load before normal endpoint startup, then traced the ordinary visible MicArray EP16 allocation/lifecycle and sampled its shared producer-position page.

## Fresh EP16 AllocateBufferV2

At qcadcm `AudioDspIoctl`, the GraphType-2 allocation was captured directly from `qcasd8380+0x307b4`:

```text
BufferMode      = 2
ReqBuffSize     = 0x00000f00 = 3840 bytes
NumBuffers      = 1
WatermarkCount  = 2
GraphType       = 2
```

This independently reproduces the earlier host-transport oracle on a clean boot.

The returned host mappings for this run were:

```text
audio ring host VA = 0xffffbc02_5c862000
position host VA   = 0xffffbc02_59dfd000
```

These addresses are runtime allocator outputs, not constants.

## EP16 lifecycle

The fresh MicArray graph emitted:

```text
0x01001000 GRAPH_OPEN
0x01001002 GRAPH_START
```

The START packet contains exactly the EP16 row20 subgraphs:

```text
0xb0000044  SG44 host terminal
0xb0000040  SG40 common processing
0xb0000041  SG41 physical ingress
```

There is no `0x01001001 GRAPH_PREPARE` between EP16 OPEN and START. A one-to-one Linux EP16 path must therefore not insert the generic AudioReach PREPARE transaction merely because the existing Linux PCM path does so.

## Position buffer ABI and live behavior

Linux's existing AudioReach ABI definition matches the Windows 16-byte shared position page:

```text
+0x00 u32 frame_counter
+0x04 u32 index                 // current read/write index in bytes
+0x08 u32 timestamp_us_lsw
+0x0c u32 timestamp_us_msw
```

Windows live samples while EP16 was producing:

```text
sample              frame_counter   index      timestamp LSW
active 1            0x00009585      0x0cc0     0x15ddaf4e
active 2            0x0002a9fa      0x0480     0x1dfd9856
active 3            0x00038b5c      0x0300     0x216dff26
post-client-exit     0x0009cbb3      0x0840     0x39d952fe
```

The index remains below the 3840-byte (`0x0f00`) ring size and wraps while `frame_counter` increases monotonically. This is direct runtime proof that, for EP16 SH_MEM_PUSH_MODE, `index` is the DSP producer byte cursor in the shared circular ring.

The existing AudioReach consistency rule is appropriate: read `frame_counter`, then position fields, then `frame_counter` again and accept the index only when the counter is unchanged across the read.

## Persistent endpoint lifetime

The crucial manufacturer behavior is that EP16 is not one-WASAPI-client = one-DSP-graph.

After the 20-second default MicArray recorder had exited, the same position page later advanced from:

```text
frame_counter 0x00038b5c
```

to:

```text
frame_counter 0x0009cbb3
```

with the byte cursor continuing to move. The DSP capture producer therefore remained STARTed after the application client was gone.

Earlier in the same oracle campaign, recycling `audiodg.exe`, `Audiosrv`, `AudioEndpointBuilder`, and finally the qcasd child device did not produce a new qcadcm GraphType-2 AllocateBufferV2/OPEN sequence. The qcadcm endpoint graph lifetime is below those client/service boundaries.

Consequences for the Linux implementation:

1. use the measured SH_MEM_PUSH_MODE ABI and producer cursor directly;
2. do not route EP16 through RD_SHARED_MEM queued-capture semantics;
3. OPEN should transition directly to START, with no GRAPH_PREPARE;
4. do not assume PCM client close should STOP/CLOSE the DSP endpoint graph merely to fit Linux's generic lifecycle;
5. runtime address/map-handle allocation remains native to Linux, while module IDs, ring size, watermarks, graph identities and lifecycle semantics remain Windows-derived.

Raw KD log:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-ep16-freshboot-lifecycle-position-kd.log`

Normalized oracle:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-ep16-freshboot-lifecycle-position.json`

Raw log SHA256:

`7e7a7787b9aaca093172c4dc85e09c927aa60dc8788422f218ac1ad9cb2859c8`
