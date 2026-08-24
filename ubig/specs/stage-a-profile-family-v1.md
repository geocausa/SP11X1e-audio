# UbiG SP11 Stage-A profile-family contract v1

Status: **DECODED + directly differential-gated**.

The SP11 VLLDP/Stage-A profile contract contains two distinct staged family states. They are retained as `DEVICE_TUNING` even though controlled audio-boundary tests establish that the family choice is bit-transparent for the SP11 48 kHz stereo render path.

## Recovered staged state

Common family (Dynamic, Game, Voice, Course, Custom):

- compressor groups: `{20, 0, 32767, 10, 20, 0}`
- channel deviation: `0`
- slow-gain enable: `0`
- slow-gain mix: `256`

Movie/Music family:

- compressor group 0: `{2, -256, 12980, 3, 20, 64}`
- compressor group 1: `{7, -160, 16366, 10, 20, 64}`
- compressor group 2: `{16, 0, 32767, 10, 20, 0}`
- compressor group 3: `{20, 0, 32767, 10, 20, 0}`
- channel deviation: `96`
- slow-gain enable: `1`
- slow-gain mix: `103`

The exact values are also stored in `sp11-stage-a-dynamic-tuning-v1.json` and generated into the native tuning module.

## Audio-boundary result

A private VLLDP-only differential constructed fresh Dynamic, Movie and Music instances with the original profile setters, then rendered identical UbiG-generated inputs through Stage A only. Dynamic, Movie and Music were bit-identical on all tested samples for five independent 16,000-frame stimuli:

- nominal multitone/noise program: hash `14b5579e78167f23`
- full-scale random noise: hash `bd231465eec991b1`
- impulse sequence: hash `7aa1fdbc590e1477`
- large DC sequence: hash `97239b8b3dd7c1e7`
- hot multitone: hash `c1bb925977132193`

Each profile comparison had zero differing float32 samples and zero maximum error. Direct captures at the central multiband-compressor boundary also showed identical external tuning arrays, row buffers and nested compressor state through the tested warm calls.

This is evidence for **Stage-A audio equivalence**, not full-profile equivalence. Movie and Music remain acoustically distinct in the downstream Stage-B/VR profile controls.

## Lifecycle requirement

The reference changes family state in place without reconstructing the VLLDP object. UbiG therefore:

1. keeps the common vs Movie/Music family payload as explicit state;
2. changes the family marker during `ubig_engine_set_profile()`;
3. does not reconstruct or clear Stage-A filterbank, compressor, limiter or adapter history;
4. continues using the one exact SP11 Stage-A audio configuration for all seven public profiles.

The public engine regression cold-starts every profile and sweeps all seven profile transitions while comparing against an untouched Dynamic Stage-A instance. Output must remain bit-identical.

## Endpoint postgain runtime lane

The SP11 endpoint postgain is part of the common Stage-A compressor runtime vector, not a separate final amplitude multiplier. `FUN_18001D280` maps the raw integer to float32 in this exact operation order:

`float(raw) * 2^-15 * 0x1.f81f82p-1 * 16`

and writes the result to VLLDP state `+0x65C`. The compressor parent call at `0x180020458` passes `state+0x658` as its five-float runtime vector, making postgain precisely `runtime[1]`. The valid recovered endpoint domain is raw `[-1200,0]`, corresponding to DAX's 1/16-dB control units.

UbiG exposes this as `ubig_engine_set_postgain()`. The engine owns a mutable runtime-vector copy so a postgain change does not reconstruct Stage A or clear any persistent history. The zero value is the original native Stage-A baseline. Private original-VLLDP differential testing is bit-exact from 0 through -1200 across representative values and arbitrary host chunk schedules; the public multi-value lifecycle hash is `118d9bc2d1524da1`.
