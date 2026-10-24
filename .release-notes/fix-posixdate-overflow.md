## Fix PosixDate returning wrong fields for very large seconds values

`PosixDate` produced wrong fields when constructed with seconds values beyond year 2147483647. At the boundary, the year wrapped to a negative number; further past it, month and day held indeterminate values that varied between builds. Constructing with both seconds and nanoseconds at `I64.max_value()` also produced a negative year.

```pony
// was: year -2147483648, month 1, day 1
// now: year 2147483647, month 12, day 31 (clamped to representable max)
PosixDate(67767976233532800)

// was: year 219250468, month/day varied between builds
// now: year 2147483647, month 12, day 31
PosixDate(I64.max_value())
```

Values beyond the representable range are now clamped to the last representable date, the same way negative values are already clamped to zero.
