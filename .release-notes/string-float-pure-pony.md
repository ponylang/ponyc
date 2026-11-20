## String float parsing is now correctly rounded on every platform

String.f32 and String.f64 now parse in pure Pony instead of calling the C library's strtof and strtod. Results are correctly rounded to nearest, ties to even, on every platform, and no longer depend on the C library or the LC_NUMERIC locale. Most inputs parse to the same value as before.
On Windows, hexadecimal inputs at the boundary of the smallest normal f64 (for example "0x0.fffffffffffff8p-1022") previously rounded down to the largest subnormal; they now round to nearest, matching the other platforms.
NaN literals with a payload, like "nan(123)", are still accepted; the payload is now ignored and the result is the canonical quiet NaN on every platform, instead of platform-dependent NaN bits.
