## cli float options and args now accept only decimal notation

`OptionSpec.f64` and `ArgSpec.f64` accepted scientific notation (`1e5`, `1.5e-3`), infinity and NaN literals (`Inf`, `NaN`), hex floats (`0x1.0p10`), leading whitespace (`  42`), and bare-dot forms (`.5`, `5.`). The documented grammar allows only an optional `-`, decimal digits, and an optional `.` followed by more decimal digits. These inputs now produce a `SyntaxError`.

Programs that pass scientific notation, infinity, NaN, hex floats, or bare-dot values to float options or arguments will see parse errors where they previously got a numeric result.

Before:

```pony
// All of these parsed successfully
CommandParser(cs).parse(["app"; "--ratio=1e5"])      // 100000.0
CommandParser(cs).parse(["app"; "--ratio=Inf"])       // inf
CommandParser(cs).parse(["app"; "--ratio=.5"])        // 0.5
CommandParser(cs).parse(["app"; "--ratio=0x1.0p10"])  // 1024.0
```

After:

```pony
// All of these are now SyntaxError
CommandParser(cs).parse(["app"; "--ratio=1e5"])
CommandParser(cs).parse(["app"; "--ratio=Inf"])
CommandParser(cs).parse(["app"; "--ratio=.5"])
CommandParser(cs).parse(["app"; "--ratio=0x1.0p10"])

// Decimal notation still works
CommandParser(cs).parse(["app"; "--ratio=100000.0"])  // 100000.0
CommandParser(cs).parse(["app"; "--ratio=0.5"])       // 0.5
CommandParser(cs).parse(["app"; "--ratio=1024.0"])    // 1024.0
```
