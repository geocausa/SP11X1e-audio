# SP11 Linux CPS source/binary recovery runbook

Date: 2026-08-11 (Europe/London)

## Purpose

Recover the exact SP11 Linux kernel lineage containing local commit `23aa077`
(`ASoC: qcom: add dedicated SP11 CPS feedback backend`) or, if the source tree
no longer exists, recover the exact rejected CPS-Lab kernel modules/initrd for
binary inspection.

This is now the prerequisite for the next Linux CPS candidate. Do not build a
replacement patch against upstream or another local tree first.

## Current offline filesystem identity

Read-only Windows partition/EFI inspection established:

- physical disk: Windows disk 0 (`HFS512GEJ3X108N-SKhynix`, GPT);
- Linux filesystem partition: disk 0 partition 5;
- GPT type: Linux filesystem
  `{0fc63daf-8483-4772-8e79-3d69d8477de4}`;
- partition size: `293778489344` bytes (~293.8 GB);
- Windows drive letter: none;
- EFI System Partition: disk 0 partition 1, currently `Z:`;
- EFI `grub.cfg` searches Linux filesystem UUID:
  `33e842b7-0434-4749-b03a-299bdcdb8b9f`;
- GRUB prefix: `/boot/grub` on that filesystem.

Do **not** mount/attach disk 0 partition 5 from Windows merely for this
recovery. It is the Linux partition on the same live system disk. Recover from
inside SP11 Linux after a normal Linux boot instead.

No Windows disk write or mount operation was performed to obtain these facts.

## Safety / boot policy

The recovery operation is evidence collection only.

- Do not arm or boot the rejected `sp11-audio-cps-lab` candidate merely to
  recover files.
- Do not change saved GRUB state.
- Do not replace the accepted `sp11-audio-clean` artifacts.
- Do not build or install a kernel during source recovery.
- Do not change SoundWire/WSA runtime state.
- Do not use physical MMIO.

If the user boots SP11 Linux normally, begin with filesystem/source discovery
before any audio playback or module replacement.

## Expected exact identifiers

Required source commit:

- short ID: `23aa077`;
- subject: `ASoC: qcom: add dedicated SP11 CPS feedback backend`.

Rejected CPS-Lab binary hashes recorded in Git:

- kernel Image:
  `8D856BA606DCEDD8BB8389A7524B52B0AD49145F3E3C902DA45EE82D9EBEAF03`;
- `soundwire-qcom`:
  `B4524693A5458C5E672D248DA6D77C8DAD7ABED6DCBEC31E81C55225DE65AE0F`;
- `snd-soc-wsa884x`:
  `3106227BAC14FB342EB6ADF841F52C81E9B33E846CCFE4698E9753F39D8BF78B`;
- `snd-soc-lpass-wsa-macro`:
  `44E352CB610CF8FC122140EFBB02C25A4865707B709459CC22C58EA3F6DB3BCE3`;
- `snd-q6dsp-common`:
  `91C133CD030A23C7C075C480B19C2A77480570025F0616C85DE60CD062E58B30`;
- `q6apm-lpass-dais`:
  `F842B8BE5B78192597DA6A1ADBD2144895A0CE558C8DAD3AB194ADDCEA39F5B2`;
- `snd-soc-x1e80100`:
  `55766F4880EB0C4D36AECD0FBB16187E0C99A023ECA8B6FEB79F1E6565F20E36`;
- observation `snd-q6apm`:
  `C1523746A091801B7D40B1DCCFE6DA8DBF1D257D79E7E80C0574369AFF512193`;
- CPS-Lab DTB:
  `AB72A157824291BAAFA4E3B37AF45819097C19A02F25F45B9BC47FA6145060D4`;
- CPS-Lab topology:
  `F385A5D83127CF8F83DAB0CBC86F418514F9C8839F2DA6AAC97E3E2EE782D121`;
- CPS-Lab initrd:
  `F2663CCEB9FD8B7AB380B075C9A57C72A4B7431CE3827FAB62F50EBEF1914D61`.

These hashes are the recovery gate: same filename is not enough.

## Phase 1 — verify the Linux filesystem and current boot, read-only

After SP11 Linux comes online:

```sh
findmnt -no SOURCE,FSTYPE,UUID /
findmnt -no SOURCE,FSTYPE,UUID /boot 2>/dev/null || true
uname -a
cat /proc/cmdline
```

Expected root/boot search UUID is
`33e842b7-0434-4749-b03a-299bdcdb8b9f` unless the installation layout changed.
If UUIDs differ, record the actual layout before proceeding; do not assume the
Windows partition number changed equivalently.

## Phase 2 — locate every plausible kernel Git tree

Search likely locations first without crossing unrelated mounted filesystems:

```sh
for root in "$HOME" /home /root /usr/src /opt /work /src /var/tmp /tmp; do
    [ -d "$root" ] || continue
    find "$root" -xdev -type d -name .git -print 2>/dev/null
 done | sed 's#/.git$##' | sort -u
```

For every candidate tree, record without modifying it:

```sh
git -C "$tree" status --short --branch
git -C "$tree" rev-parse HEAD
git -C "$tree" remote -v
git -C "$tree" cat-file -t 23aa077 2>/dev/null || true
git -C "$tree" log --all --oneline --decorate --grep='dedicated SP11 CPS feedback backend' -n 20
```

If `23aa077` resolves, immediately capture:

```sh
git -C "$tree" show --no-ext-diff --stat --summary 23aa077
git -C "$tree" show --no-ext-diff --format=fuller --find-renames 23aa077 -- \
    drivers/soundwire/qcom.c \
    drivers/soundwire/stream.c \
    sound/soc/codecs/wsa884x.c \
    sound/soc/qcom \
    arch/arm64/boot/dts/qcom
```

Do not reset, checkout, clean, rebase or otherwise alter the recovered tree.

## Phase 3 — preserve the exact source lineage

Once the correct tree is positively identified, record:

```sh
git -C "$tree" rev-parse 23aa077^{commit}
git -C "$tree" rev-parse HEAD
git -C "$tree" status --porcelain=v2 --branch
git -C "$tree" describe --always --dirty --tags 2>/dev/null || true
```

Preferred preservation order:

1. create a **read-only evidence copy/bundle** outside the source tree;
2. preserve the commit and the reachable local branch history needed to
   reconstruct it;
3. separately save `git show 23aa077` and any later uncommitted/committed CPS
   changes;
4. hash every exported artifact;
5. only then begin source review.

If using `git bundle`, first inspect refs and choose the minimum refs that retain
the local SP11 history. Do not blindly bundle secrets or unrelated private
branches into the public engineering repo.

## Phase 4 — source audit before any edit

Read
`docs/findings/2026-08-11-linux-cps-per-slave-transport-gap.md` first.

Audit these exact questions in the recovered tree:

### Qualcomm master compute path

`drivers/soundwire/qcom.c` / `qcom_swrm_compute_params()`:

- Does slave DP6 still get `offset1` exclusively from
  `ctrl->pconfig[slave->m_port_map[p_rt->num]]`?
- Does `23aa077` already add any per-slave CPS transport override?
- Does any local code already distinguish the two WSA8845 identities or the
  two speaker DT nodes after both map to physical master port 13?

### WSA884x slave-port capabilities

`sound/soc/codecs/wsa884x.c`:

- Is CPS DP6 still `SDW_DPN_SIMPLE`?
- Is CPS native `ch_mask` still `0x03`?
- Is `read_only_wordlength` still true?
- Did local CPS work add any transport-param or slave-register path?

### SoundWire core programming semantics

`drivers/soundwire/stream.c`:

- Is SampleCtrl2/HCtrl/BlockCtrl3 still skipped for SIMPLE DPNs?
- Is there any local capability extension that changes this for WSA884x?

### Dedicated CPS backend

Inspect `23aa077` itself:

- separate graph/DAI/backend plumbing changes from SoundWire transport changes;
- preserve the confirmed 24 kHz TX1/backend work;
- do not assume its transport design is correct merely because the backend
  registers/prepares.

## Phase 5 — if source is gone, recover exact binaries instead

Search for exact CPS-Lab artifacts by filename first:

```sh
find /boot /usr/lib/modules /lib/modules /home /root /var/tmp /tmp -xdev \
  -type f \( -name 'soundwire-qcom*.ko*' -o -name 'snd-soc-wsa884x*.ko*' \
  -o -name '*cps*lab*.dtb' -o -name '*initr*' -o -name 'Image*' \) \
  -print 2>/dev/null
```

Hash candidates and compare to the exact identifiers above. Do not infer from
names or timestamps.

Also inspect GRUB entries and `/boot` only to locate referenced candidate files;
do not change them.

If the exact CPS-Lab initrd is recovered, extract it to a temporary evidence
directory and verify the embedded module hashes. The initrd itself is an
especially valuable fallback because the recorded candidate validation says it
contained all rebuilt modules.

## Phase 6 — binary fallback analysis

If exact source cannot be recovered but exact modules can:

- copy the exact module files to the evidence workspace;
- preserve compressed originals and SHA-256 first;
- record `modinfo` output;
- decompress copies only;
- record ELF build IDs, symbols, section layout and BTF/DWARF availability;
- compare `soundwire-qcom` against the tracked/upstream baseline around
  `qcom_swrm_compute_params()`;
- compare WSA884x CPS DPN property initialization and CPS `hw_params` path;
- do not load the modules merely for analysis.

A binary diff can answer whether `23aa077` changed the two source-level gaps
without booting the rejected candidate.

## Phase 7 — only then design the replacement

If the recovered exact tree still has the baseline gaps, implement/review a
normal SoundWire solution that preserves:

- one physical WSA master port 13;
- both WSA8845 slave DP6 masks `0x03`;
- left slave OffsetCtrl1 `0`;
- right slave OffsetCtrl1 `25`;
- 800-clock / 24 kHz slave transport (`SampleCtrl1/2 = 0x1f/0x03`);
- HCtrl `0xff`;
- BlockCtrl3 `0x00` where capability semantics require it;
- accepted 48 kHz render/PBR and 8 kHz VISENSE behavior;
- no direct MMIO or raw slave-register workaround.

Do not add physical master port 14. Do not restore split masks `0x01`/`0x02`.
Do not blindly change SIMPLE to FULL without reviewing the exact additional
slave register writes it enables.

## Current status

At creation of this runbook:

- SP11 Windows online;
- SP11 Linux PiSlave offline;
- Linux partition present but intentionally unmounted in Windows;
- no exact source/commit copy found in SP11 Windows user storage or EFI volume;
- no exact CPS-Lab module copy found in searched SP7 local/ignored artifact
  locations;
- no source build, kernel/module install, GRUB mutation or reboot performed.
