# UbiG Stage A math helpers v1

Status: **DECODED + direct-function bit-exact proven.**

## log2 approximation

Reference anchor: `0x1800247c0`.

Input is a positive float amplitude in the active Stage-A path. The function:

1. extracts the original exponent;
2. replaces exponent bits with 126 to normalize the mantissa;
3. evaluates a fixed fused quadratic using exact constants `0x402aaaab` and `0x3faaaaab`;
4. adds the exponent offset.

The helper feeds the compressor-side scalar path.

## scaled exp2 approximation

Reference anchor: `0x180023d20`.

The vector helper multiplies each input by a scalar, floors to an integer part,
forms the fractional remainder, evaluates the exact cubic with coefficient bits
`0x3f2fb000`, `0x3e827800`, `0x3d714000`, and reconstructs the integer power-of-two contribution using float exponent arithmetic.

The ARM64 reference processes four lanes at a time. UbiG intentionally uses a
scalar loop because lanes are independent; exact arithmetic ordering is retained.

## Oracle result

Direct comparison against the two original ARM64 functions passed 200,000+ fixed/random log2 calls and 1,000,000 exp2 vector lanes with exact float32 bit identity.
