# Cumulative core build regression

## Result

The sixth audio-v3 boot did not test patch `0019` as intended. The deployed
`snd-q6apm` module had been relinked with a `q6apm.o` object from before
patches `0017` and `0019`. It therefore regressed the already validated graph
lifecycle reply handler and did not contain the uncached position-map packet.

This was a build-artifact error, not a new DSP behavior.

## Runtime proof

An isolated 48 kHz, signed 16-bit stereo hardware-parameter probe sent no
audio samples. Live probes correlated the synchronous command and callback:

- Linux sent `GRAPH_START` (`0x01001002`);
- 7.193 ms later the same graph client received a basic response for
  `GRAPH_START` with status zero;
- the DSP concurrently emitted pull watermark event `0x0800101c` at about
  100 Hz, proving that its 10 ms pull period was active;
- the callback and waiter referenced the same result field at graph offset
  `0x60`;
- the loaded callback returned with that field still zero, and the waiter
  expired after five seconds.

The DSP therefore accepted and ran the graph. The host discarded its success
reply.

## Binary proof

The source `q6apm.c` was dated 29 July and contained all three lifecycle cases.
The linked `q6apm.o` was dated 28 July. Disassembly of the deployed module
confirmed that `graph_callback()` did not accept `0x01001002`, and the module
did not contain the `SP11 stage GRAPH_START accepted` marker.

This explains why the fourth boot had validated patch `0017`, while the sixth
boot again timed out: the later `0019` deployment had accidentally replaced
the working cumulative core with a stale link product.

## Corrected cumulative build

The QDSP6 directory was forcibly rebuilt from the exact patched V3 source.
Machine-code inspection now confirms both required contracts:

1. `graph_callback()` accepts `GRAPH_START`, `GRAPH_STOP` and `GRAPH_FLUSH`;
2. the position-map header is packed as `0x0000000200010003`, which decodes to
   pool 3, one region and `property_flag = 0x2`. The PCM and protection OOB
   maps remain cached with property flag zero.

The corrected module:

- source version: `B81C31D91BEE0320DA11F97`;
- vermagic: `7.1.5-sp11-audio-v3`;
- signer: the existing V3 build key, SHA-512;
- compressed SHA-256:
  `f7dfe0c86b957db22cd5c857be66ff5272af23f263ca790f7f5f94f3947b6365`.

All 71 repository tests pass. Patches `0017`, `0018` and `0019` each pass
strict checkpatch with zero errors, warnings or checks. The targeted ARM64
QDSP6 build with `W=1` completes successfully.

The stale deployed module is preserved at
`02-kernel/v3-runtime-backups/pre-corrected-0017-0019-core/`.
The V3 initramfs does not embed `snd-q6apm`, so replacing the signed root
module and refreshing module dependencies is sufficient.

## Next acceptance gate

After one V3 reboot:

1. loaded `snd_q6apm` must report source version
   `B81C31D91BEE0320DA11F97`;
2. the kernel must report `SP11 stage GRAPH_START accepted` with no
   `0x01001002` timeout;
3. a muted zero-data probe must show the DSP-owned position counter and ALSA
   hardware pointer advancing before any audible playback is attempted.
