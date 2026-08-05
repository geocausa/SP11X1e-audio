# SP11 native Windows Dolby profiles — 2026-08-05

## Status

The original-code Linux bridge now supports the seven useful SP11 speaker
profile families selected at plugin activation with `SP11_DOLBY_PROFILE`:

- dynamic
- movie
- music
- game
- voice
- onlinecourse
- personalize (covers the three static Personalize slots; user GEQ is separate)

Profile values come from the archived SP11 OEM tuning file
`DAX3_SPEAKER_TUNING_MSHW0486_REV0D.xml`; processing is still performed by the
original ARM64 Windows `DolbyAPOvlldp150.dll` and `DolbyApoVr.dll` code.

## VLLDP split

Game, Voice, Online Course and Personalize use the same VLLDP base tuning as
Dynamic. Movie and Music share the alternate four-group compressor layout:

```text
group0 = 2,-256,12980,3,20,64
group1 = 7,-160,16366,10,20,64
group2 = 16,0,32767,10,20,0
group3 = 20,0,32767,10,20,0
channel deviation = 96
slow gain enable = 1
band0 slow gain mix = 103
```

The exact original setters are:

```text
0x18001CB90  compressor channel deviation
0x18001CC50  compressor slow-gain enable
0x18001CB20  compressor slow-gain mix level
```

Dynamic-family values are explicitly restored to `0`, `0`, and `256` on each
cold rebuild rather than relying on constructor residue.

The XML also says `speaker-peq-enable=0` for Movie/Music versus `1` for the
other profiles. No proven VLLDP callable setter has yet been bound to that key.
The corresponding `speaker-peq-filters` payload is empty on this SP11 tuning,
so no unproven function address is used for it.

## VR/DAP profile controls

The already recovered original scalar/structured handlers now receive each
profile's OEM values for leveler, dialog enhancer, IEQ target/amount, MI
steering, surround decoder/boost, virtualizer angles, VolMax boost, output
processing mode, 20-band IEQ target and regulator table.

Important profile distinctions include:

- Dynamic: mode 11, leveler amount 5, dialog 5, IEQ Balanced amount 10,
  surround boost 96, all MI steering enabled.
- Movie: mode 11, leveler amount 0, dialog 2, IEQ disabled/Warm target,
  surround boost 72, front/surround angles 16, VolMax 104.
- Music: mode 1, leveler amount 0, dialog off, IEQ disabled/Warm target,
  surround decoder off, surround boost 24.
- Game: mode 11, dialog off, IEQ off, surround boost 0.
- Voice: mode 1, leveler off, dialog amount 8, surround decoder off.
- Online Course: mode 1, leveler amount 0, dialog 5, VolMax 64.
- Personalize: mode 11, leveler amount 3, dialog 10, surround boost 48.

The two XML `output-mode-partial-*-virtualizer-enable` booleans are not exposed
through a proven direct handler in the recovered VR dispatch work. Dynamic had
already been validated without separately writing them, and mode selection is
applied through the original output-mode function. They remain a documented
profile-parity detail rather than a guessed write.

## Offline validation

A 50,000-frame deterministic test was run for all seven profiles. Every profile
was bit-identical across host chunk patterns 1, 64, 480, 1024, 127/353 and a
mixed irregular pattern. Reference hashes were distinct:

```text
dynamic       9d4b1534fa1c02b6
movie         fe84c0bdb078650d
music         de07320c2411d1bd
game          3ed5531aebf3b3ba
voice         9350232215c4d780
onlinecourse  d8ae8062d25f7877
personalize   e178bdf5e8cd8157
```

The complete 29.45-second known-input stimulus (1,413,600 frames) was also run
through every profile at 480-frame host chunks. All seven completed with zero
NaN/Inf samples.

Regression check against the previously installed Dynamic-only plugin at
200,000 frames produced the same bit-exact reference hash for old and new:

```text
01ec0adc40a8905b
```

Therefore adding profile selection does not alter the established Dynamic path.
