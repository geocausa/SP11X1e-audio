# SP11 render-parity main/branch integration checkpoint — 2026-08-17

## Canonical state

`main` and `agent/render-parity-20260812` are intentionally kept in lockstep at this checkpoint. The previous `main` tip (`849350d`) had **zero unique commits** and was a strict ancestor of the render-parity branch, so integration was a pure fast-forward rather than a content merge.

Current canonical head when this checkpoint was written:

- `main`: `1c0ce9a` or later descendant of this checkpoint;
- `agent/render-parity-20260812`: same canonical line;
- `origin/main`: same canonical line.

The render line contains the v28 DP2/COMP `OffsetCtrl2=0x07` physical-static closure, exact Windows WSA8845 63/10/6 lifecycle work, v28 seek physical evidence, W02 loopback-boundary reclassification, and all associated reviewed artifacts.

## Aug-15 WSA lifecycle branch audit

Branch audited: `origin/agent/wsa-lifecycle-trace-20260815`.

Although Git reports nine commits outside `main` because the branch has parallel history, content-aware comparison shows the useful payload is already represented on canonical `main`:

- qcadcm hardware-resource boot-zero JSON/log: byte-identical;
- qcadsp codec-resource-vs-ACDB JSON: byte-identical;
- direct WSA-macro MMIO/WHEA124 rejection: byte-identical;
- WSA-macro compander provenance: byte-identical final artifact;
- ACDB driver-data recovery finding: byte-identical;
- qcadcm boot-zero finding: byte-identical;
- WSA boot lifecycle collector/service/final trace script: byte-identical;
- WSA boot lifecycle test: byte-identical.

Differences are canonical improvements rather than missing payload:

- `tools/acdb_driver_data_inventory.py` on main adds robust package/local import handling;
- its test differs only by cleanup;
- `QCADSP-CODEC-RESOURCE-VS-ACDB.md` differs only in formatting;
- `WSA-HARDWARE-LIFECYCLE-REOPEN.md` on main contains the later passive CPS-v3 producer-ordering closure.

The older branch's CSR-lifecycle erratum is substantively superseded by stronger canonical evidence: passive producer ordering, the exact CPS-v3 one-bit `CSR_GAIN_EN` A/B, and the DP2 `OffsetCtrl2=0x07` coupled-prerequisite closure carried into v28.

**Decision:** no rescue/cherry-pick required; preserve branch as historical provenance only.

## Aug-8 ASAR dual-lane checkpoint audit

Branch audited: `origin/agent/aug8-asar-dual-lane-checkpoint`.

The old checkpoint/finding prose is historical and partially superseded by later state-pinned ASAR work on canonical main. In particular, later CAPONode/buffer provenance proves the frozen ordinary-stereo ASAR edge is byte-exact unity and the VirtualSurround edge is byte-transparent for the frozen stereo blocks. The old dual-lane checkpoint must not be reintroduced as current architecture truth.

One genuinely useful public tool was absent from main:

`tools/windows/Probe-DolbyActivationFactory.ps1`

It activates a supplied private WinRT DLL/class via `DllGetActivationFactory`, performs arbitrary requested `QueryInterface` calls, and reports subobject/vtable RVAs. It embeds no proprietary DLL or fixture. The exact historical blob was rescued from the branch and committed to canonical history as:

`1c0ce9a tools: rescue Dolby activation factory probe`

The existing `tools/windows/Record-WindowsLoopback.ps1` was already byte-identical on main.

**Decision:** tool rescued; stale checkpoint prose remains archive-only.

## CPS/DP6 runtime-closure branch audit

Branch audited: `origin/agent/cps-dp6-runtime-closure-20260810`.

Eight parallel-history commits touched the CPS-v3 source recovery and Windows render-family work. Exact blob classification found:

- **47 paths byte-identical on main**;
- **6 paths different because canonical main contains later corrections/integration changes**;
- **3 paths intentionally absent**.

The intentionally absent paths are:

1. `artifacts/reviewed/sp11-audio-kernel-lineage-20260811.bundle` — deliberately retained only on the historical archive branch; canonical `docs/findings/2026-08-11-exact-23aa077-source-recovered.md` records its SHA-256 and explicitly says not to duplicate it into the integration tree;
2. `docs/audit/2026-08-12-sp11-deployed-linux-speaker-render-parity-ledger.md` — superseded by `docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md`;
3. `docs/runbooks/2026-08-12-chat-transfer-render-parity-handoff.md` — historical chat-transfer snapshot superseded by later canonical findings/runbooks/ledger state.

Canonical code deliberately moved away from the branch's monolithic DEFAULT+NOTIFICATION topology builder. The exact render-family evidence/manifests/build helpers themselves are retained byte-identically, while the current default topology builder contains later four-control-link POPLESS headroom closure. Do not regress it by copying the old branch builder over main.

**Decision:** no additional rescue required.

## Active / divergent worktrees — do not delete

These worktrees/branches remain physically checked out and must not be removed as “old” merely because main now contains much of their evidence:

- `/home/geoca/Documents/SP11-PROJECT/01-audio` — `agent/audio-v2-clean-rebuild`; local worktree has commits beyond its remote tip;
- `/home/geoca/Documents/SP11-PROJECT/04-dolby-re-work` — `agent/dolby-completion-2026-08-05`;
- `/home/geoca/Documents/SP11-PROJECT/05-audio-integration` — `agent/audio-integration-protection-20260810`;
- `/home/geoca/Documents/SP11-AUDIO-AUDIT/worktree-7c6ad09` — detached historical worktree.

`origin/agent/asar-linux-breakthrough-20260809` and the Dolby-completion branch also contain research harness history and, in places, private/proprietary fixtures or built binaries. They are **not** candidates for wholesale merging into publishable main. Any future rescue must be source/doc-by-source/doc and must exclude private vendor content.

## Branch deletion policy from this checkpoint

No remote branch is deleted by this checkpoint. Content integration is now clean enough that deletion can be a separate housekeeping action, but active worktrees and archive-only provenance remain useful recovery anchors. Prefer preserving them until their local worktrees are explicitly retired.

## Speaker-parity completion state

At this checkpoint:

- v28 W03 physical broadband static: GREEN;
- W02: retained as a non-blocking dedicated Windows WASAPI-loopback identity research item, not a speaker/device-stream gate;
- L03 objective v28 seek behavior: clean, including SP7-external physical evidence and repeated zero-fault deterministic seek runs;
- L03 final subjective gate: operator listening verdict remains pending;
- persistent GRUB fallback remains `sp11-audio-cps-v3` until the final subjective gate is explicitly closed or the operator chooses otherwise.
