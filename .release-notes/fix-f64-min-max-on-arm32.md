## Fix F64.min() and F64.max() on ARM32

`F64.min()` and `F64.max()` returned incorrect results on ARM32 — both returned the maximum of their two arguments regardless of which was called. They now produce correct results, preserving IEEE 754-2019 minimum/maximum semantics for NaN and signed zero.
