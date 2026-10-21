## Fix Format.float ignoring align and fill parameters

`Format.float` accepted `align` and `fill` parameters but ignored them — output was always right-aligned with spaces regardless of the values passed. `Format.int` with the same arguments worked correctly.

```pony
Format.float[F64](1.5 where width = 8, align = AlignLeft, fill = '*',
  prec = 2, fmt = FormatFix)
// was: "    1.50"
// now: "1.50****"
```
