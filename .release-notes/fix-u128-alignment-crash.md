## Fix runtime crash in optimized builds for types with 128-bit fields

Programs containing a class or struct with a `U128` or `I128` field could crash with a segmentation fault at runtime when compiled in release (optimized) mode, even though the same program ran correctly when compiled with `--debug`. The crash happened when such an object was used in a way that let the compiler place it on the stack.

These objects are now correctly aligned in optimized builds, and the crash no longer occurs.
