## Fix crashes in optimized builds for objects with 128-bit integer fields

Programs that used objects containing a `U128` or `I128` field could crash at runtime when compiled normally, even though the exact same program ran correctly when compiled with `--debug` (`-d`). For example, a small value type wrapping a `U128` that was created and compared inside a loop would segfault in an optimized build.

These crashes no longer occur. Objects with 128-bit integer fields are now correctly aligned in all build configurations.
