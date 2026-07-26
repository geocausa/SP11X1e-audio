# Windows root-splitter peers are capture-side branches

## Finding

The three external destinations of root `SPLITTER 4002` are not physical
speaker outputs and are not part of the speaker-only playback baseline. Each
destination is an `MFC` input owned by a Windows capture-key GKV row:

| Splitter edge | Peer subgraph | Static selector |
|---|---|---|
| `4002:5 -> 4747:2` | `b000008c` | capture SPEECH, endpoint 2 |
| `4002:9 -> 47c9:2` | `b000009a` | capture SPEECH, endpoint 7 |
| `4002:11 -> 4730:2` | `b000008a` | capture COMMUNICATIONS, endpoint 2 |

This removes the old ambiguity that these ports might represent additional
amplifiers, physical speaker sides, or post-DMA hardware routing.

## Evidence boundary

The classification combines three independently reviewed inputs:

1. the static cross-bundle owner index binds all three destination IIDs to
   `MODULE_ID_MFC` in the subgraphs above;
2. their owning GKV rows use keys `01000008..0100000d`, the capture stream,
   mix and endpoint schema;
3. the already verified miniport/QCADCM translation maps processing value `5`
   to SPEECH and value `6` to COMMUNICATIONS.

Across four complete recovered QGPR traces, all 13 `GRAPH_START` lists are
accounted for. None contains `b000008a`, `b000008c`, or `b000009a`; the same
is true of the recovered stop lists. The captures therefore do not prove a
runtime lifetime for these optional capture graphs, but they do prove that
the speaker render families start and stop without listing them.

Their signal direction and capture-mode ownership are consistent with
playback-reference inputs used by capture processing. Calling their precise
algorithmic role “AEC” remains an inference until the unresolved modules in
those capture bundles are identified.

## Linux disposition

For the first speaker-only structural baseline:

- keep the root splitter's hardware continuation exactly as recorded;
- preserve ports 5, 9 and 11 as documented optional branch identities;
- do not create their MFC peers or external connection records;
- restore them only as part of a separately verified microphone SPEECH or
  COMMUNICATIONS graph.

This is a scope decision, not deletion from the canonical Windows model. The
reviewed machine-readable closure is
`artifacts/reviewed/windows-root-splitter-capture-peers.json`.
