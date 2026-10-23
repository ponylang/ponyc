## Fix String.f64 and String.f32 rejecting subnormal numbers

`String.f64` and `String.f32` rejected strings representing subnormal floating-point values, including the output of `F64.string()` for subnormals. Parsing "5e-324", "1e-310", or "1e-45" raised an error even though these are representable IEEE 754 values. `F64.from_bits(1).string()` could not be parsed back by `String.f64`.

```pony
"5e-324".f64()?                      // was: error, now: 4.94066e-324
"1e-310".f64()?                      // was: error, now: 1e-310
"1e-45".f32()?                       // was: error, now: 1.4013e-45
F64.from_bits(1).string().f64()?     // was: error, now: 4.94066e-324
```
