## Fix incorrect rounding of float literals near the extremes of F64's range

Float literals near the extremes of F64's representable range were silently rounded to the wrong value. The most visible case: `2.2250738585072014e-308` (the smallest normal F64) compiled to zero.

```pony
// Before: compiled to 0x0 (zero)
// After:  compiled to 0x0010000000000000 (correct)
let x: F64 = 2.2250738585072014e-308
```

Float literal conversion is now correctly rounded per IEEE 754. A float literal and its string-parsed equivalent (via `String.f64()`) now produce the same bits.
