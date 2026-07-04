## Fix json package losing precision when serializing floating-point values

The `json` package serialized a floating-point number with only six significant digits, so the output did not represent the original value. Parsing that output back — including with the package's own parser — returned a different number, so the package could not round-trip its own floats. A float now serializes with enough precision to read back as exactly the same `F64`, while values that need fewer digits still print compactly.

```pony
JsonPrinter.print(F64(3.141592653589793))
// before: 3.14159             (a different number)
// after:  3.141592653589793
```
