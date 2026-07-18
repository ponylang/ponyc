## Correct the signature of `FloatingPoint.ldexp`

`FloatingPoint.ldexp` scales a floating-point value by a power of two (the inverse of `frexp`), but it took the value to scale as an explicit first argument and never used the receiver it was called on. Operations on a float elsewhere in `FloatingPoint` work on the receiver.

The value to scale has moved from the first argument to the receiver. `ldexp` now scales the value it is called on.

Before:

```pony
// the receiver (here F64(0)) was never used; `mantissa` was the value scaled
let scaled = F64(0).ldexp(mantissa, exponent)
```

After:

```pony
let scaled = mantissa.ldexp(exponent)
```
