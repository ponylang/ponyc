## String float parsing no longer uses the C library

String.f32 and String.f64 now parse in pure Pony instead of calling the C library's strtof and strtod, so results no longer depend on the C library or the LC_NUMERIC locale. On Windows, correct rounding to nearest, ties to even was verified by a differential fuzz against the C runtime and by the test suite; other platforms were not tested as part of this change. Most inputs parse to the same value as before.
On Windows, hexadecimal inputs at the boundary of the smallest normal f64 (for example "0x0.fffffffffffff8p-1022") previously rounded down to the largest subnormal; they now round to nearest.
NaN literals with a payload, like "nan(123)", are still accepted; the payload is now ignored and the result is the canonical quiet NaN on every platform, instead of platform-dependent NaN bits.
