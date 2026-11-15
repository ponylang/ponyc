## cli float options and args now accept only decimal notation

`OptionSpec.f64` and `ArgSpec.f64` accepted scientific notation (`1e5`, `1.5e-3`), infinity (`Inf`, `-Inf`), NaN (`NaN`), hex floats (`0x1.0p10`), and leading whitespace (`  42`). The documented grammar allows only an optional `-` followed by decimal digits with an optional decimal point. These inputs now produce a `SyntaxError`.

Before:

```pony
// All of these parsed successfully
CommandParser(cs).parse(["app"; "--ratio=1e5"])     // 100000.0
CommandParser(cs).parse(["app"; "--ratio=Inf"])      // inf
CommandParser(cs).parse(["app"; "--ratio=NaN"])      // nan
CommandParser(cs).parse(["app"; "--ratio=0x1.0p10"]) // 1024.0
```

After:

```pony
// All of these are now SyntaxError
CommandParser(cs).parse(["app"; "--ratio=1e5"])
CommandParser(cs).parse(["app"; "--ratio=Inf"])
CommandParser(cs).parse(["app"; "--ratio=NaN"])
CommandParser(cs).parse(["app"; "--ratio=0x1.0p10"])

// Decimal notation still works
CommandParser(cs).parse(["app"; "--ratio=100000.0"]) // 100000.0
CommandParser(cs).parse(["app"; "--ratio=42"])        // 42.0
CommandParser(cs).parse(["app"; "--ratio=-1.5"])      // -1.5
```
