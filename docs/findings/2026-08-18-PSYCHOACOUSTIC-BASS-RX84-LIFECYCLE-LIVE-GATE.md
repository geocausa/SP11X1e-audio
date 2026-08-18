# Golden v31 psychoacoustic bass — RX84 lifecycle userspace live gate

Date: 2026-08-18
Status: GREEN for lifecycle and physical reproduction; broader regression/listening promotion pending.

## Candidate

Commit `f4f1f26` adds a v31-capability-gated producer gain lifecycle to
`sp11_windows_volume_transaction_sync.py`.

The policy is deliberately not a permanent idle mixer edit:

- v31 is identified by the exact prior/new-CKV `Volume Only` capability;
- graph idle: RX0/RX1 remain Golden baseline value 81 (-3 dB);
- first successful protected graph/DSP handover: write RX0/RX1 value 84 (0 dB)
  once, after the producer is active;
- ordinary live volume/mute events do not rewrite the controls;
- producer idle resets the controls to 81 and the lifecycle flag;
- a later wake reapplies 84 once;
- legacy/v28/CPS kernels never enter this policy.

This ordering is required because an idle-time ALSA mixer value does not prove
the active producer register survived POST_PMU/regmap restoration.

## Regression tests

Focused test suite: 26/26 passed.

Broader volume/Dolby suite: 65 tests + 6 subtests passed.

The exact subprocess form `amixer -D hw:0 cset name=...` was no-op validated on
the live machine before deployment.

## Deployment provenance

Previous installed synchronizer SHA-256:

`6416ff7fd2dfd683c3f4e192aa6f9463ca6c5cfe94cb249b2cd02cc9671f2d04`

Candidate installed/source SHA-256:

`8f9787d54d0c4d74dd51713e81d411b9041cb4e03638a8c820c8aa245c5b3de2`

Timestamped rollback copy:

`~/.local/bin/sp11-windows-volume-transaction-sync.pre-rx84-20260818-145759`

No kernel, initrd, DTB, GRUB or persistent saved-entry change was made.

## Lifecycle proof

At service restart with PCM closed:

- `ckv_delta=prior-new`
- `wsa_rx=windows-0db-active`
- RX0/RX1 both remained 81.

First digital-zero wake:

1. exact endpoint DSP handover completed;
2. one `wsa_rx_active=84 (0 dB native-Windows producer)` record appeared;
3. both live controls read 84 while PCM was RUNNING;
4. once the graph genuinely closed, one `wsa_rx_active=81` appeared and both
   controls returned to 81.

An independent second wake reproduced exactly:

`RX81 idle -> RX84 active -> RX81 idle`.

No ordinary volume/mute event generated a repeated RX84 write while the same
producer lifecycle remained active.

## Integrated physical transfer gate

The deterministic `psycho-bass-transfer-v2.wav` source was replayed with:

- fresh Movie filter generation at 10% bootstrap;
- exact source SHA-256
  `28461206bc70e017196088025cd3f4c9ac397f5ea3a18ccd18e412b11ea352c8`;
- service-managed active RX84 only (no manual `amixer` gain write);
- 25% endpoint handover proven before probe content;
- SP7 WASAPI RAW fixed geometry;
- simultaneous Linux post-Dolby capture.

Evidence:

- stage SHA-256 `87562fb75687454e5dc09b61261509e01ca2ed15fa80656b6f4100755666e9ac`
- post-Dolby SHA-256 `bcefdba65c61c5437032cec1e2c98bf584e3ba6e415b983cf022c6edeed97066`
- SP7 RAW SHA-256 `9b17b149a5807a2948fe9778c908cb5e793cff7e3541e2b99aad51883bdcadd1`
- SP7 physical analysis CSV SHA-256
  `f89862422832a4cfdf076ae6524acbc9f3ea302687f86c65cdefb33989e55cbb`

The integrated service capture was compared directly with the earlier
corrected manual-RX84-active capture. For stereo 75/100/150-Hz blocks with
source amplitude >=0.1:

- median integrated-minus-manual fundamental: about -0.037 dB;
- mean absolute difference: about 0.062 dB across 12 repeated blocks.

For 100 Hz across both cycles and source amplitudes 0.05/0.10/0.20, median
integrated-minus-manual fundamental was about -0.029 dB.

Thus the lifecycle service reproduces the manually proven RX84 active producer
state to much better than the physical measurement uncertainty.

The only kernel message during the graph wake was the already accepted startup
`0x01001006` status-3 calibration record. There were no new WSA/PA/SoundWire/
XRUN/runtime transaction faults attributable to the candidate.

After recorder/graph teardown:

- PCM closed;
- RX0/RX1 returned to 81;
- visible endpoint returned to 6%.

## Decision

The userspace lifecycle mechanism is GREEN. It is directly tied to a native
Windows producer register state and physically reproduces the manual RX84
oracle. Before merging into Golden/main, repeat the pathological 40-Hz volume
control gate and a bounded normal-program/mute/seek gate at active RX84 to prove
that restoring Windows producer gain does not regress v31's CKV/mute/seek
closures.
