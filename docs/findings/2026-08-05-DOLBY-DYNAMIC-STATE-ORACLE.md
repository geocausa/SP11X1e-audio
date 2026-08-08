# SP11 Dynamic VLLDP state oracle — 2026-08-05

The archive contains multiple Windows Dynamic-profile captures with the same
997 Hz WAV byte-for-byte (`SHA-256
48b85ce14e8b1e7f38e7e5fe84c44eb1dcf09982ffce8f67759c83eb2090b582`).
DAX RPC state records `active_profile=5`, Dolby enabled and volume leveler
enabled. Their `child1+0xc0c` 20-band vectors are highly repeatable.

A new diagnostic, `dolby-port/sp11_vlldp_state_oracle.c`, runs the exact current
Linux VLLDP->VR bridge on that WAV and can compare the live VLLDP `+0xc0c`
vector at every host callback against a selected Windows capture.

A raw-amplitude scan showed the closest cold-start match around input scale
~2.2, but this must not be treated as a fitted Windows gain. The archive's
stronger June 16 evidence proves the capture labeled `PRE_audio_cold` already
contained non-zero warm analyzer/optimizer history even while Windows master
volume was 0% and muted. Therefore the saved `+0xc0c` vectors are history
boundaries, not deterministic cold-start targets.

Earlier June native replay evidence is consistent with this: when the original
Windows optimizer function is fed the captured analyzer/ring state, its
predicted `+0xb60` vector is within about 3 integer units MAE of Windows. The
failed PRE->POST synthetic-tone replay was explicitly attributed to the missing
real warm-up/history sequence, not a per-stage math failure.

Conclusion: use these captures as internal-state/history evidence and static
configuration oracles, not as a cold-stream amplitude calibration target.
