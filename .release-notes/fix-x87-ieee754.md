## Fix floating-point arithmetic giving wrong results on 32-bit x86 without SSE2

On 32-bit x86 targets whose CPU lacked SSE2, floating-point arithmetic could produce incorrect results that violated IEEE 754. ponyc now unconditionally enables SSE2 for all 32-bit x86 targets.
