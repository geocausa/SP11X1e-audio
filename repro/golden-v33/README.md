# Golden v33 clean reproduction

Golden v33 is one source patch over the already closed Golden-v32 clean
reproduction. The recipe first runs `../golden-v32/build-and-verify.sh`; only
after all v32 source/srcversion/runtime-digest gates pass does it apply patch
0072 and rebuild the WSA macro in the promoted scoped Kbuild context.

A v33 pass requires:

- post-0072 `lpass-wsa-macro.c` SHA-256
  `4826cd591f3af594575474219f7f2215a297db29b647ccb15049ac33ba237d0b`;
- WSA macro srcversion `3FAA616CDE10DDBF9D90D6F`;
- unchanged Golden-v32 identities for WSA8845, SoundWire, X1E and q6apm;
- v33 WSA runtime ELF digest
  `c2f40153537bbcd309cc208d1c488846fc3bfbb8c4822a8ac49c880da010cd99`.

The Golden-v32 overlay/config/base inputs are reused by reference; they are not
duplicated here.
