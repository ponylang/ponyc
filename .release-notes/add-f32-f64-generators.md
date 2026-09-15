## Add Generators.f32 and Generators.f64 to PonyCheck

PonyCheck now has built-in `F32` and `F64` generators. Both accept `from` and `to` parameters (defaulting to `0.0` and `1.0`) and normalize argument order, matching the integer generator API.

```pony
// Generate F64 values in [0.0, 1.0] (the default)
let gen = Generators.f64()

// Generate F32 values in [-100.0, 100.0]
let gen = Generators.f32(where from = -100.0, to = 100.0)
```

The generators work across the full type range, including `Generators.f64(where from = -F64.max_value(), to = F64.max_value())`. They error on NaN inputs and shrink generated values toward zero (or the nearest bound) automatically.

Previously, floating-point generation required `Generators.repeatedly` with a lambda, which produced values that could not be shrunk.
