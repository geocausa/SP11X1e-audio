# 2026-08-25 MicArray EP16 SH_MEM_PUSH_MODE runtime closure

Windows is the oracle for this work. This checkpoint records the ordinary default MicArray host-transport transaction observed live on SP11 Windows after recycling only `audiodg.exe`, which causes the Windows Audio Engine to recreate its shared WaveRT capture stream without restarting qcaud/qcadcm/qcasd/qcaucd or the audio device.

## AllocateBufferV2 request

At qcadcm `AudioDspIoctl`, Windows issued opcode `0x26` (`AllocateBufferV2`) from `qcasd8380+0x307b4` with the exact five-dword input:

```text
BufferMode      = 2
ReqBuffSize     = 0x00000f00 = 3840 bytes
NumBuffers      = 1
WatermarkCount  = 2
GraphType       = 2
```

This removes the final ambiguity around the default MicArray ring depth and notification/watermark count. The two equal-step watermark levels are therefore 1920 and 3840 bytes.

## SH_MEM_PUSH_MODE SET_CFG

Immediately before qcadcm submits the in-band SPF packet at `qcadcm8380+0x5b980`, the complete 64-byte packet was captured:

```text
00000000000000000000000030000000
DC4000000A1000081C00000000000000
0080000001000000000F0000285087B0
0090000001000000785087B000000000
```

Decoded:

```text
MIID                         = 0x40dc
module                       = SH_MEM_PUSH_MODE / 0x07001007
PARAM_ID_SH_MEM_PULL_PUSH_CFG= 0x0800100a
body size                    = 0x1c
ring DSP address             = 0x00000001_00008000
ring size                    = 0x00000f00 = 3840
ring map handle              = 0xb0875028
position DSP address         = 0x00000001_00009000
position map handle          = 0xb0875078
```

The runtime addresses/map handles are allocator outputs and must not be hard-coded on Linux. The structure and size are ABI facts.

## Watermark registration

Immediately before qcadcm submits GSL ioctl `0x11` at `qcadcm8380+0x86698`, the exact 28-byte registration payload was captured:

```text
DC400000 1C100008 0C000000 01000000
02000000 80070000 000F0000
```

Decoded:

```text
MIID            = 0x40dc
watermark event = 0x0800101c
body size       = 0x0c
register        = 1
num levels      = 2
level[0]        = 0x780 = 1920
level[1]        = 0xf00 = 3840
```

## End-to-end Windows host transport

```text
ordinary shared MicArray open
  -> qcasd AudioDsp path
  -> qcadcm AllocateBufferV2 opcode 0x26
     { mode=2, size=3840, buffers=1, watermarks=2, graph=2 }
  -> one shared 3840-byte audio ring + one position buffer
  -> SG44 IID 0x40dc SH_MEM_PUSH_MODE
     PID 0x0800100a with runtime DSP addresses/map handles
  -> register PID/event 0x0800101c levels {1920,3840}
  -> host capture
```

This is now a measured Windows contract, not an inferred Linux design. A one-to-one Linux implementation should allocate runtime addresses/handles natively but reproduce the same module ID, parameter/event IDs, ring size, single-buffer model, two watermark levels, graph direction, and transaction semantics.

Raw KD log: `artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-ep16-shmem-push-runtime-kd.log`.
