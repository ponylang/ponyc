## Fix underlying time source for Time.nanos() on macOS

Previously, we were using `mach_absolute_time` on macOS to get the number of ticks since boot, with the assumption that ticks increment at nanosecond intervals. However, as noted by [Apple](https://developer.apple.com/documentation/apple-silicon/addressing-architectural-differences-in-your-macos-code#Apply-Timebase-Information-to-Mach-Absolute-Time-Values), this assumption is flawed on Apple Silicon, and will also affect any binaries that were built on Intel, as the Rosetta 2 translator will not apply any time conversion.

The recommended replacement to `mach_absolute_time` is to use `clock_gettime_nsec_np` with a clock of type `CLOCK_UPTIME_RAW`.

