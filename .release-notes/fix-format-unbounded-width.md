## Fix _FormatInt allocation hint overflow when prec defaults to -1

`_FormatInt.u8`, `u16`, `u32`, and `u64` computed their initial `String` allocation as `(prec + 1).max(width.max(31))`. When `prec` was the default `-1` (`USize.max_value()`), `prec + 1` wrapped to 0, producing an allocation hint of `width.max(31)` regardless of the requested precision. The `u128` variant already used the resolved `prec'` and was unaffected.

All four methods now use `prec'` (the sentinel-resolved value) in their allocation hint, matching `u128`.
