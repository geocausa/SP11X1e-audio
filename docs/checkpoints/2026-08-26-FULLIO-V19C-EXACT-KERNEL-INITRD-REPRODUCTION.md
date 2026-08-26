# FullIO v19c exact kernel/initrd reproduction — 2026-08-26

Status: **ACCEPTED / P1 CLOSED**

The heavy reproduction path now starts from the pristine Linux 7.1.5 baseline,
replays the accepted Golden-v33 source chain, applies only production microphone
patches 0072 + 0078, and recreates the three FullIO/v18 LPASS modules plus the
promoted v19c initrd byte-for-byte.

Canonical command:

```bash
JOBS=12 ./repro/native-audio-v19c/build-kernel-initrd-and-verify.sh
```

A complete fresh invocation finished with exit code 0 and ended in:

`FULLIO v19c KERNEL + INITRD EXACT REPRODUCTION PASS`

Exact module results:

- common SHA-256 `0698d60676385d7e1bd9459a8b57834809b4a8125e73c766425552687dd6683f`, srcversion `2EA7312A851E75A7C860F82`
- VA SHA-256 `161fe5e40e48d6797821414cd0d2e31a91271084264ecdd288f502dd02ffeb47`, srcversion `DC4373218C279E16F550900`
- TX SHA-256 `19d4a65a03de6e120767874657072251b68e9383c8b2f637b3f912c22f1cd402`, srcversion `835AF5272E94DB266E85D55`

Exact promoted initrd SHA-256:

`ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`

The first clean-script attempt exposed a reproduction bug rather than a driver
bug: after rebuilding the patched VA/TX leaf objects, the script fed MODPOST the
stale pre-0078 Golden composite `snd-soc-lpass-{va,tx}-macro.o` objects. The
fixed recipe explicitly relinks those composites before MODPOST. The corrected
composites contain the shipped VA DMIC-clock provider and TX shared-clock
consumer functions, and the resulting modules match the accepted initrd bytes.

The initrd packer uses a pinned newc header/order manifest plus the verified
Golden-v33 payload set and freshly rebuilt module payloads. `zstd -19 -T0`
recreates the accepted stream framing exactly.

System suspend/resume was not tested and remains owned by its separate RE.
