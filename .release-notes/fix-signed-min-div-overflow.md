## Fix wrapping division of signed minimum value by -1

Dividing a signed integer's minimum value by -1 with the wrapping `/` operator returned 0 instead of wrapping to the minimum value. The same was true of floored division (`fld`). For example:

```pony
let x = I64.min_value() / -1  // was 0, now -9223372036854775808
let y = I64.min_value().fld(-1)  // also now -9223372036854775808
```

The mathematical result of `MIN_VALUE / -1` is one past `MAX_VALUE`, so wrapping arithmetic should produce `MIN_VALUE` — the same way `MAX_VALUE + 1` wraps to `MIN_VALUE`. This now works correctly for all signed integer types, including `I128`.

Using literal constants (e.g., `I8(-128) / I8(-1)`) previously produced a spurious compile error ("constant divide or rem overflow") instead of folding to the correct value. That is also fixed.

The checked variants (`divc`, `fldc`) now return the correct wrapped value alongside the overflow flag, rather than returning 0 with the flag set.

Remainder (`%`, `%%`) was not affected — it already returned 0, which is the correct result.
