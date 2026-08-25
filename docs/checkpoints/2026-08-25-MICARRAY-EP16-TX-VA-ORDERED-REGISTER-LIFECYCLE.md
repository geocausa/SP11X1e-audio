# MicArray EP16 TX + VA ordered register lifecycle

Status: **closed from live Windows KD**.

A fresh Windows boot stopped at the `qcaucd8380.sys` load event before endpoint activity. A nonblocking breakpoint on `qcaucd8380+0x20348` logged masked register writes across the TX logical window (`0x0000..0x0fff`) and VA logical window (`0x3000..0x30ff`). The second capture run is a self-contained 50-write startup/teardown sequence (`n=49..98`).

## Startup ordering

```text
49  VA  3000 bit0=1
50  VA  3004 bit0=1
51  VA  3080 bit1=1
52  TX  0000 bit0=1
53  TX  0004 bit0=1
54  TX  0100 [7:4]=2   # lane/DMIC select
55  TX  0104 [1:0]=0
56  TX  0408 bit0=1
57  TX  0404 bit7=1
58  TX  0108 [7:4]=1
59  TX  010c [1:0]=0
60  TX  0488 bit0=1
61  TX  0484 bit7=1
62  VA  3094 bit7=0
63  VA  3084 [3:1]=2   # DIV4 selector
64  VA  3084 bit0=1    # DMIC run
65  VA  3094 bit7=0
66+ TX path mute/rate/clock programming
```

Thus Windows does **not** acquire VA DMIC0 at the beginning of TX startup. It first enables shared VA/TX gates and programs the two TX input lanes, then acquires VA DMIC0, then enables the TX data paths.

## Teardown ordering

```text
80..87 TX runtime-stop preamble
88     VA 3084 bit0=0      # DMIC run clear
89..94 TX path clock/rate stop
95     TX 0000 bit0=0
96     TX 0004 bit0=0
97     VA 3000 bit0=0
98     VA 3004 bit0=0
```

The manufacturer ownership boundary is therefore explicit: the TX capture path consumes a shared VA DMIC clock resource. Linux should acquire that resource **after TX input/mode selection and before TX path enable**, and release it **before final TX path/gate teardown**. Keeping a fake VA capture stream alive would be architecturally wrong.

## Artifacts

- Raw KD log: `artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-va-tx-ordered-kd.log`
- Normalized sequence: `artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-va-tx-ordered.json`
- Raw SHA-256: `0fe9a747171cd3f45b1d833e57190218b3b74bab164b8acbaabc08316a695f87`

No device restart or register mutation was used; the trace observed the ordinary Windows default MicArray path.
