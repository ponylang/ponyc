## Fix U128 to F32 conversion returning infinity for values near F32.max_value()

On platforms where the compiler uses the Pony source implementation for 128-bit conversions (Windows MSVC), converting a U128 value slightly above `F32.max_value().u128()` to F32 incorrectly returned infinity instead of `F32.max_value()`. The intermediate F64 representation could be one ULP above `F32.max_value().f64()`, which tripped an overly strict overflow check. The F64-to-F32 conversion already handles rounding correctly on its own.
