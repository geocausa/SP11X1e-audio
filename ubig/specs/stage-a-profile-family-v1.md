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
