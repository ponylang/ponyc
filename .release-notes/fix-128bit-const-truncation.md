## Fix incorrect constant folding of I128 division and shift operations

Constant I128 division and remainder gave wrong results when the divisor was a 128-bit value whose low 64 bits were all ones — for example, `0xFFFF_FFFF_FFFF_FFFF` (which is 2^64−1, not −1). Such divisors were truncated to −1, producing the signed-overflow result instead of the actual quotient.

```pony
// Before: d was I128.min_value(), r was 0
// After: d and r are both -9223372036854775808
let d: I128 = I128.min_value() / 0xFFFF_FFFF_FFFF_FFFF
let r: I128 = I128.min_value() % 0xFFFF_FFFF_FFFF_FFFF
```

Constant 128-bit shift amounts above 2^64 were also accepted when they should have been rejected as exceeding the type width.

```pony
// Before: compiled without error, produced undefined behavior
// After: "shift amount greater than type width" compile error
let a: U128 = 1 << 0x1_0000_0000_0000_0005
```
