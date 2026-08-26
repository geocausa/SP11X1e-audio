# FullIO v19c reproduction

`build-and-verify.sh` is the clean-clone reproduction gate for the promoted
FullIO topology. It compiles the tracked topology source and requires exact byte
identity with the accepted binary. It also checks the merger's pinned Golden
hash and collision-free capture object namespace, plus focused UbiG control
regressions when pytest is available.

The kernel and initrd are intentionally unchanged from Native Audio v18. Their
accepted live hashes are checked by `deploy/native-audio-v19c/verify-native-audio-v19c.sh --live`.
