# MicArray qcaucd VA DMIC0 physical-register mapping closure

## Result

The qcaucd logical DMIC register used by the live SP11 MicArray path is now mapped all the way to the X1E80100 physical MMIO block and to the exact Linux register definition.

```text
qcaucd logical register 0x3084
  -> FUN_140020348 register RMW helper
  -> FUN_14001bce8 raw register transport
  -> logical top nibble 0x3000 selects base DAT_14001bf68
  -> DAT_14001bf68 = 0x06D41000
  -> physical = 0x06D41000 + 0x00003084
  -> physical = 0x06D44084
```

X1E80100 `hamoa.dtsi` defines:

```text
lpass_vamacro: codec@6d44000
  reg = <0 0x06d44000 0 0x1000>
```

Therefore:

```text
0x06D44084 = lpass_vamacro + 0x84
```

Linux `sound/soc/codecs/lpass-va-macro.c` names this exact offset:

```c
#define CDC_VA_TOP_CSR_DMIC0_CTL        (0x0084)
#define CDC_VA_DMIC_EN_MASK             BIT(0)
#define CDC_VA_DMIC_CLK_SEL_MASK        GENMASK(3, 1)
#define CDC_VA_DMIC_CLK_SEL_SHFT        1
```

This is an exact physical-register identity, not a bitfield analogy.

## Live Windows state

The already captured Windows MicArray lifecycle programs:

```text
VA DMIC0_CTL @ 0x06D44084
configured byte = 0x04
running byte    = 0x05
```

So while running:

```text
bit0      = 1     DMIC enabled
bits[3:1] = 010   selector 2
```

qcaucd derives selector 2 from compact global resource type `0x1D`, live value `8`:

```text
raw 4  -> selector 0
raw 6  -> selector 1
raw 8  -> selector 2  <- SP11
raw 12 -> rejected
raw 16 -> selector 4
raw 32 -> selector 5
```

The symbolic name of qcaucd compact type `0x1D` remains unknown; the earlier cross-driver `RPMH_COMMIT` interpretation is invalid and remains superseded by commit `6d755d1`.

## Linux comparison

Linux uses the same physical VA DMIC0 register in `va_dmic_clk_enable()`.

For Romulus the DTS currently supplies:

```text
qcom,dmic-sample-rate = <4800000>
```

Linux validates that against:

```text
VA_MACRO_MCLK_FREQ = 9600000
```

and therefore chooses enum selector 0 (`DIV2`), which would write:

```text
configured byte = 0x00
running byte    = 0x01
```

That differs from Windows (`0x04` / `0x05`) at the exact same physical register.

However Linux also requests its external `mclk` at `2 * VA_MACRO_MCLK_FREQ = 19.2 MHz`. Until the effective Windows VA clock source/rate is closed, this selector difference must not yet be called a PDM-frequency mismatch: Windows DIV4 and Linux DIV2 could still be equivalent if their divider input clocks differ by 2x.

## qcaucd VA top-register gate sequence

The same qcaucd dispatcher also operates the VA block top registers:

```text
logical 0x3000 -> VA + 0x0000 MCLK_CONTROL
logical 0x3004 -> VA + 0x0004 FS_CNT_CONTROL
logical 0x3008 -> VA + 0x0008 SWR_CONTROL
logical 0x3080 -> VA + 0x0080 TOP_CFG0
```

Its enable path writes MCLK/FS enable and TOP_CFG0 bit1, closely matching Linux `va_clk_rsc_fs_gen_request()`.

## Next target

Close the Windows VA clock source/rate used while MicArray is active. This determines whether the exact-register selector difference (`Windows 2`, `Linux 0`) is:

1. an intentional compensation for different source clocks, or
2. a genuine Linux DMIC clock-divider parity bug.

No Linux patch should be made until that distinction is resolved.

## Safety

All evidence for this closure is static/read-only plus the previously captured MicArray lifecycle. No device/service restart, reboot, registry mutation, or audio-setting mutation was performed.
