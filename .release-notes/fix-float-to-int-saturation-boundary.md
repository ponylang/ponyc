## Fix float-to-integer saturation at the representability boundary

Safe float-to-integer conversions saturate out-of-range values to the target type's min or max. When the integer max was not exactly representable in the source float type, the float representation rounded up to the next power of two, and a float value at that exact boundary was not caught by the saturation check. The value fell through to the raw conversion, which is undefined behavior in LLVM and produced the target type's min on x86-64.

`F32(2147483648).i32()` returned `I32.min_value()` instead of `I32.max_value()`. The same problem affected `F64(9223372036854775808).i64()`, `F32(170141183460469231731687303715884105728).i128()`, `F32(4294967296).u32()`, and `F64(18446744073709551616).u64()`.

All float-to-integer conversions now saturate correctly at the boundary.
