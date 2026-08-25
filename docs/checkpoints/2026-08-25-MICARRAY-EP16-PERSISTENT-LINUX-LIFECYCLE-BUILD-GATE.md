# 2026-08-25 MicArray EP16 persistent Linux lifecycle build gate

Windows is the oracle. The fresh-boot MicArray trace proved that EP16 remains a live producer after the recording application exits: the same position page continued advancing from frame counter `0x00038b5c` to `0x0009cbb3` with no replacement AllocateBufferV2, GRAPH_OPEN or GRAPH_START. AudioSrv and EndpointBuilder recycling likewise did not create a replacement EP16 graph.

That means application close is a host-client detach boundary, not the DSP endpoint STOP/CLOSE boundary.

## Patch 0077

`patches/0077-ASoC-q6apm-SP11-persist-EP16-push-endpoint-graph.patch`

SHA256:

```text
5f15c2650a950be9c22697542acbee1910d2634fd5cd07c33b968195ab2e7bfc
```

The implementation keeps one component-owned persistent SH_MEM_PUSH_MODE capture endpoint per graph id. The persistent object owns:

- the `q6apm_graph` wrapper;
- its GPR client port;
- the underlying APM graph reference;
- the already configured circular ring;
- the position page;
- the watermark registration.

Each ALSA application open attaches a transient `q6apm_dai_rtd` through an RCU-protected pointer. The watermark callback resolves only that current client. Application close first marks the client stopped, clears the RCU pointer, waits for in-flight callbacks with `synchronize_rcu()`, and then frees the transient runtime object without issuing GRAPH_STOP or GRAPH_CLOSE.

A later ALSA client reuses the same running producer and position page. It does not resend the circular-buffer configuration and does not reset the DSP producer cursor.

The actual Linux cleanup boundary is q6apm DAI device removal. Persistent endpoint clients are detached, then a started graph is STOPped and CLOSEd before devres tears down the ASoC PCM object and its fixed shared-memory ring mapping.

This lifecycle change is limited to SH_MEM_PUSH_MODE capture and does not alter conventional RD_SHARED_MEM capture or protected-render pull behavior.

## Static gate

Final patch:

```text
checkpatch: 0 errors, 0 warnings, 257 lines checked
patch --dry-run: PASS
```

## Exact Golden-v33 build gate

Applied to the exact Golden-v33 qdsp6 tree after `0074` and `0076`:

```text
0074 ASoC audioreach SH_MEM_PUSH_MODE topology
0076 ASoC q6apm EP16 Windows push transport
0077 ASoC q6apm EP16 persistent endpoint ownership
```

Build result:

```text
q6apm-dai.ko
SHA256 d6be51e284ea1eb96f9631528e4cf87ae551f5f14521d183a914344e41e2999e
srcversion 0B512DC50172B2313FC6C32
vermagic 7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64

snd-q6apm.ko
SHA256 44a4dbe887adbe78ac461c42a8b19b5667dff35ab7f844ab8238f8f90ab90724
srcversion CA0C1C65C785B369BB8B76C
vermagic 7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64
```

The unchanged `snd-q6apm.ko` hash is expected because `0077` modifies only `q6apm-dai.c`; the shared q6apm transport from `0076` is unchanged.

## Golden restoration gate

After the build:

```text
SRC_QDSP6=PASS
OUT_QDSP6=PASS
ROOT_Module.symvers=PASS
ROOT_modules.order=PASS
ROOT_.modules.order.cmd=PASS
```

The Golden-v33 kitchen was restored byte-for-byte.

## Runtime acceptance target

A dedicated boot candidate should now prove all of the following without modifying Golden-v33:

1. first MicArray open configures EP16 push transport and starts the graph;
2. capture data is non-zero and the DSP position page advances;
3. first application close does not issue EP16 GRAPH_STOP/GRAPH_CLOSE;
4. a second application open logs `SP11 EP16 rebound to persistent push graph` and captures from the same advancing producer;
5. only endpoint/device teardown performs the final STOP/CLOSE.
