# SP11 native LADSPA candidate

`src/integration/sp11_ladspa_candidate.c` is the disposable promotion boundary between the public UbiG DSP library and the existing SP11 PipeWire filter-chain graph. It is **not** the Golden deployment and is not installed by the normal build.

The candidate contains source-owned Stage-A/Stage-B algorithms and raw-layout adaptation needed by the current corrected-v4 laboratory state graph. It contains no Windows PE loader, vendor DLL path, executable reference callback, or recovered endpoint payload initializer. Endpoint-specific state/model/tuning bytes remain owner-supplied through a private pack outside Git.

Build with:

```sh
make -C ubig candidate-ladspa
```

The candidate fails closed unless `UBIG_SP11_STAGEB_PACK` names a structurally valid corrected-v4 private pack. `SP11_VR_STAGEB_PACK` is accepted only as a temporary laboratory compatibility alias. Startup profile and Custom EQ can be supplied by `UBIG_PROFILE` / `UBIG_GEQ`; ordinary runtime control uses the public `ubig-control-v2` mmap ABI used by `ubigctl`, including profile, Custom EQ and endpoint postgain requests. Postgain has its own request/ack generation pair so event-driven volume feedback does not retrigger profile/GEQ application.

A private-pack control lifecycle gate is available as:

```sh
UBIG_SP11_STAGEB_PACK=/path/to/private.pack make -C ubig candidate-control-check
```

That gate verifies request/ack profile switching, **PCM divergence after an otherwise-identical Dynamic→Movie retarget**, entry into Custom with a non-flat 20-band curve, and a second Custom curve update **without changing profile or reconstructing the DSP state**. Private reference differentials remain outside Git.

The v4 private pack is reproducible from the owner's existing v3 pack and `DolbyAPOVR.dll` with `tools/build_stageb_v4_pack.py`. The tool copies only the immutable caller-owned Stage-B data window needed by the native Leveler/multiband/profile path; the generated payload remains outside Git and is mapped read-only/non-executable by the candidate.

The existing Windows-taper, final volume-transaction and MSIIR/CKV helpers can now share the candidate control page without changing the Golden default. Their legacy layout remains the default for the installed Windows bridge, while a candidate deployment supplies the `ubig-control-v2` path and `--control-format ubig-v2`. The Python writer and public C control API serialize creation/updates with the same file lock, so endpoint postgain may be queued before LADSPA instantiation without losing the request when the candidate opens the page. The MSIIR reader auto-detects the UbiG v2 header and consumes the same desired postgain used by the final transaction.

Promotion rules:

- never replace the installed Golden plugin merely by building this target;
- keep the private pack outside the repository and fail closed when it is absent;
- preserve the existing visible-sink / hidden-engine PipeWire ordering and final Qualcomm endpoint-volume transaction;
- run the M6 fixed-profile, transition, chunking, lifecycle, xrun/NaN and long-run gates before switching the installed graph;
- retain an immediate rollback to Golden v32 throughout candidate testing.

Disposable deployment is tracked under `deploy/ubig-candidate/`. Its prepare path is non-activating; the switch path records an exact rollback snapshot before replacing the active filter-chain fragment. The package has been exercised in an isolated HOME/runtime with a fake user-systemd boundary through prepare → activate → rollback, restoring the original fragment byte-for-byte.
