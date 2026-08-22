# SP11 native LADSPA candidate

`src/integration/sp11_ladspa_candidate.c` is the disposable promotion boundary between the public UbiG DSP library and the existing SP11 PipeWire filter-chain graph. It is **not** the Golden deployment and is not installed by the normal build.

The candidate contains source-owned Stage-A/Stage-B algorithms and raw-layout adaptation needed by the current corrected-v3 laboratory state graph. It contains no Windows PE loader, vendor DLL path, executable reference callback, or recovered endpoint payload initializer. Endpoint-specific state/model/tuning bytes remain owner-supplied through a private pack outside Git.

Build with:

```sh
make -C ubig candidate-ladspa
```

The candidate fails closed unless `UBIG_SP11_STAGEB_PACK` names a structurally valid corrected-v3 private pack. `SP11_VR_STAGEB_PACK` is accepted only as a temporary laboratory compatibility alias. Startup profile and Custom EQ can be supplied by `UBIG_PROFILE` / `UBIG_GEQ`; ordinary runtime control uses the public `ubig-control-v2` mmap ABI used by `ubigctl`, including profile, Custom EQ and endpoint postgain requests. Postgain has its own request/ack generation pair so event-driven volume feedback does not retrigger profile/GEQ application.

A private-pack control lifecycle gate is available as:

```sh
UBIG_SP11_STAGEB_PACK=/path/to/private.pack make -C ubig candidate-control-check
```

That gate verifies request/ack profile switching, entry into Custom with a non-flat 20-band curve, and a second Custom curve update **without changing profile or reconstructing the DSP state**. Private reference differentials remain outside Git.

Promotion rules:

- never replace the installed Golden plugin merely by building this target;
- keep the private pack outside the repository and fail closed when it is absent;
- preserve the existing visible-sink / hidden-engine PipeWire ordering and final Qualcomm endpoint-volume transaction;
- run the M6 fixed-profile, transition, chunking, lifecycle, xrun/NaN and long-run gates before switching the installed graph;
- retain an immediate rollback to Golden v32 throughout candidate testing.
