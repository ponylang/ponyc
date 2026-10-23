## Fix F64.f32() and U128.f32() returning inf for values that round to F32.max_value()

The safe `f32()` conversion on `F64` and `U128` returned `inf` for values slightly above `F32.max_value()` that IEEE 754 round-to-nearest-even rounds to `F32.max_value()`. The unsafe `f32_unsafe()` and `String.f32()` already returned `F32.max_value()` for the same inputs, so `"3.402823567e38".f32()?` returned `F32.max_value()` but `"3.402823567e38".f64()?.f32()` returned `inf`.

```pony
let above = F64.from_bits(F32.max_value().f64().bits() + 1)
above.f32()         // was: inf       now: F32.max_value()
above.f32_unsafe()  // was: F32.max_value() (unchanged)
```
