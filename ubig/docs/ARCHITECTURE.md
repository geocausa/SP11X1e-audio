# UbiG architecture v1

## Data path

```text
application audio
      |
      v
PipeWire adapter
      |
      v
+---------------------------+
| libubig-core              |
|                           |
| input scheduling          |
| -> UbiG Stage A           |
| -> UbiG Stage B           |
| -> output                 |
+---------------------------+
      |
      v
Golden v32 AudioReach / WSA speaker path
```

The engine is deliberately independent of PipeWire/LADSPA. Adapters convert host buffers to the stable UbiG engine ABI.

## Control path

```text
UbiG Control GUI ----+
                     |
ubigctl --------------+--> ubigd (planned)
                              |
                              | desired state
                              v
                     versioned shared control page
                              |
                         block-boundary poll
                              |
                              v
                         libubig-core
                              |
                         ack generation
```

During M0/M1 `ubigctl` can write the control page directly so the ABI can be tested before `ubigd` exists. Once `ubigd` is introduced, direct writers become a debugging mode only.

## Control-page contract

The page contains a fixed header plus desired and acknowledged state. Writers update the desired payload, publish a monotonically increasing `request_generation`, and the engine reads a stable snapshot at a process boundary. After a successful in-place transition the engine publishes `active_profile`, status/error and `ack_generation`.

A profile switch is a retune event, not an engine reconstruction event. Adaptive history is preserved unless a future behavioral specification explicitly marks a state region as reset-on-retune.

## Profiles

Public UbiG names:

- Dynamic
- Movie
- Music
- Game
- Voice
- Course
- Custom

The initial SP11 profile contract retains recovered device behavior but UbiG owns the public naming and API. `Custom` carries a 20-band signed target vector. The current recovered range is `[-192, +192]` in the native profile unit; presentation in dB belongs to the controller/UI and must use the proven conversion rather than guessing.

## Realtime rules

`ubig_engine_process()` must:

- allocate nothing
- perform no file operations
- perform no syscalls for control
- take no blocking mutex
- emit no logs
- not depend on environment variables
- remain deterministic for identical engine state, input and request sequence

All expensive retune preparation is precomputed outside the sample loop when possible; the final state application occurs at a block boundary.

## Adapter contract

The first target is exactly 48,000 Hz, 2 channels, float32. Unsupported formats fail construction rather than silently resampling.

The decoded 256-frame accumulator is implemented as its own independently testable component. The 432-domain outer scheduler will be implemented separately once its full observed contract is transcribed into UbiG specs.

## Safety / rollout

No default `install` target exists in early UbiG milestones. `make` and `make check` build local artifacts only. A live adapter is explicitly named `lab` until waveform and lifecycle gates are complete.
