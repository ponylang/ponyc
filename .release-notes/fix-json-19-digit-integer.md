## Fix JSONParser reading 19-digit integers as F64

`JSONParser.parse` returned F64 for integer literals with 19 or more digits, even when the value fit I64. A 19-digit value like `1000000000000000001` printed by `JSONPrinter.print` came back as `F64(1e18)`, losing the last digits. The parser now uses overflow detection instead of a digit-count threshold, so every integer that fits I64 stays I64.

```pony
// was: F64 1e+18 (lost the trailing 1)
// now: I64 1000000000000000001
JSONParser.parse(JSONPrinter.print(I64(1000000000000000001)))
```
