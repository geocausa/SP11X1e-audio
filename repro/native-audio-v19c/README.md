# FullIO v19c reproduction

Two reproduction gates are provided.

## Fast topology gate

`build-and-verify.sh` is the clean-clone reproduction gate for the promoted
FullIO topology. It compiles the tracked topology source and requires exact byte
identity with the accepted binary. It also checks the merger's pinned Golden
hash and collision-free capture object namespace, plus focused UbiG control
regressions when pytest is available.

```bash
./repro/native-audio-v19c/build-and-verify.sh
```

## Exact kernel + initrd gate

`build-kernel-initrd-and-verify.sh` is the heavy end-to-end gate. It starts from
the pristine Linux 7.1.5 baseline through the existing Golden-v33 clean
reproducer, applies only the production microphone patches 0072 + 0078, and
recreates the three promoted LPASS modules with the exact historical in-tree
Kbuild/MODPOST semantics.

It then reconstructs the accepted newc archive deterministically from the
verified Golden-v33 initrd plus those three rebuilt module payloads and requires
the complete FullIO v19c initrd to be byte-identical.

```bash
JOBS=12 ./repro/native-audio-v19c/build-kernel-initrd-and-verify.sh
```

Accepted module identities:

- `snd-soc-lpass-macro-common.ko` — SHA-256 `0698d60676385d7e1bd9459a8b57834809b4a8125e73c766425552687dd6683f`, srcversion `2EA7312A851E75A7C860F82`
- `snd-soc-lpass-va-macro.ko` — SHA-256 `161fe5e40e48d6797821414cd0d2e31a91271084264ecdd288f502dd02ffeb47`, srcversion `DC4373218C279E16F550900`
- `snd-soc-lpass-tx-macro.ko` — SHA-256 `19d4a65a03de6e120767874657072251b68e9383c8b2f637b3f912c22f1cd402`, srcversion `835AF5272E94DB266E85D55`

Accepted FullIO v19c initrd SHA-256:

`ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`

A successful heavy run ends with:

`FULLIO v19c KERNEL + INITRD EXACT REPRODUCTION PASS`

The exact raw module identity can be build-path-sensitive because compiler/source
location provenance may be embedded in these historical module ELFs. The raw
SHA gates are intentionally not normalized: if a different build path changes
historical provenance bytes, the heavy gate fails rather than silently claiming
byte identity.

The deployed live identity remains independently checked by:

```bash
./deploy/native-audio-v19c/verify-native-audio-v19c.sh --live
```
