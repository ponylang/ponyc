## Fix json package emitting invalid JSON for non-finite floating-point values

The `json` package serialized a non-finite floating-point number — infinity or NaN — as the bare word `inf`, `-inf`, `nan`, or `-nan`. None of those are valid JSON, so the output could not be parsed back, including by the package's own parser. A non-finite value now serializes as `null`.

```pony
let inf = F64(1e308) * F64(10)
JsonPrinter.print(JsonObject.update("v", inf))
// before: {"v":inf}   (not valid JSON)
// after:  {"v":null}
```

Relatedly, parsing a numeric literal outside `F64` range — such as `1e999`, or an integer of several hundred digits — used to succeed, producing one of these non-finite values. Such a literal is now rejected as a parse error.

```pony
JsonParser.parse("1e999")
// before: a non-finite F64 that cannot be serialized back to valid JSON
// after:  a JsonParseError, "Number out of range"
```

Number parsing is also more accurate at the extremes: a literal that is mathematically zero but written with a large exponent (`0e309`) previously parsed to NaN, and some in-range literals with very long digit runs parsed to infinity or a slightly wrong value — these now parse correctly.
