# SP11 isolated audio candidate preservation

Date: 2026-08-13 (Europe/London)

This checkpoint moves the reviewable sources for the three uninstalled kernel
candidates out of the unversioned `SP11-AUDIO-AUDIT` scratch directory and
into the canonical repository. It does not install, sign, load or arm any
candidate.

## Preserved candidates

| Candidate | Canonical path | SHA-256 | Live state |
|---|---|---|---|
| Windows SOFT_PAUSE lifecycle | `patches/0045-q6apm-sp11-windows-soft-pause-lifecycle.patch` | `9dc808bbbf4dbe5240bd4bec4282a0e6fd1b956c657258c2f0ba24489d0dd05e` | unsigned, uninstalled |
| WSA884x runtime-regcache optimization | `patches/0046-ASoC-wsa884x-avoid-full-cache-dirty-on-clock-stop.patch` | `b653431eb855d7dccc709841f2104fb20a519f15a7756732dac2ae2e37ae4ba2` | unsigned, uninstalled |
| Final DSP endpoint-volume actuator | `patches/0047-q6apm-add-SP11-final-endpoint-volume-Q28-control.patch` | `e604bdeb118a2961687380f9980da5d930a3885407629dca3db8f4717429c13c` | unsigned, uninstalled |

The final-volume diagnostic also preserves:

- `tools/sp11_final_volume_q28.py`: exact 104-byte Windows multichannel-gain
  body generator;
- `tools/sp11_volume_actuator_ab.py`: bounded A/B wrapper with safe actuator
  handover ordering; and
- focused tests under `tests/test_sp11_final_volume_q28.py` and
  `tests/test_sp11_volume_actuator_ab.py`.

## Build evidence

The scratch builds report the exact live release ABI
`7.1.5-sp11-cps-v3+ SMP preempt mod_unload modversions aarch64`:

- final-volume/soft-pause `snd-q6apm.ko`: source version
  `7F8E1452BC021273EECD2C7`, SHA-256
  `75aa626abb7253dcb37050fa844726e2ea72dd2529389dccb2761f886d3da440`;
- soft-pause `q6apm-dai.ko`: source version
  `552C36761955AE06C1AEF0A`, SHA-256
  `d0fe4e11dfec5d0daa3a67f0d92578fd44a8570a4032d2cb9e385bbcc7fca0e3`;
- WSA candidate `snd-soc-wsa884x.ko`: source version
  `B7F5D7D97DD31C77EFB6F01`, SHA-256
  `4ccf7565dd4e8457d61b3482c45cf4605305128d4ff1ad8b02a9665236442688`.

These uncompressed modules are evidence only. They are unsigned and differ
from the signed modules currently loaded by the accepted CPS-v3 kernel.

## Deployment gates

1. Never combine the three live experiments into one first boot.
2. Sign and stage each candidate in a separate rollback-safe boot entry.
3. Final `VOL_CTRL` A/B must avoid double attenuation and preserve the exact
   Windows taper, Dolby postgain and MSIIR/GainStep state.
4. SOFT_PAUSE must pass bounded pause/resume/STOP/reprepare regression.
5. WSA regcache must pass muted cold-start timing, attach/context-loss,
   suspend/resume and cached-control-write regression.
6. Do not promote the four-link headroom topology as a seek fix; its physical
   gate failed even though the topology is structurally correct.
