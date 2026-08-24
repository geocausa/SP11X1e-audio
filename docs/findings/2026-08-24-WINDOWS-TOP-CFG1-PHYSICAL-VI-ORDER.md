# Windows TOP_CFG1 closes the physical VI ordering mismatch

Native Windows first-valid TAP2 is `V,I,V,I`. Golden v32 first-valid TAP2 is
`I,V,I,V`. Both are immediately valid, ruling out a startup-validity explanation
for the ordering difference.

Windows `qcaucd` physically writes WSA macro `TOP_CFG1=0x03` after each enabled
VI pair. Linux carried `0x03` as a regmap default, but the SP11 cache policy can
skip the physical write. Materializing that write on `microsoft,denali` changes
the **pre-SPVI** Linux producer to `V,I,V,I` from its first packet.

The downstream q6apm/SP_VI `[2,1,4,3]` reorder is rejected. Once the producer was
also corrected it became a double correction and reproduced the right-amp
`err0=0x20`/static failure at 40/50%. Golden v33 retains ordinary q6apm mapping
and fixes the contract only at the WSA producer.
