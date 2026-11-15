## Fix binary_codec Conditional encoding None as empty when flag is true

`Conditional[A](true, codec).encode(None)` wrote zero bytes and returned success instead of returning `EncodeViolation`. A `Seq` containing the conditional field produced output with the field missing. The decoder expected the field, read subsequent bytes in its place, and either failed with `InsufficientData` or misaligned every field after it.

The encoder now returns `EncodeViolation("Flag is true but value is None")`.
