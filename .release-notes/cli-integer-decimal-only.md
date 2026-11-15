## cli integer options and args now accept only decimal digits

`OptionSpec.i64`, `OptionSpec.u64`, `ArgSpec.i64`, and `ArgSpec.u64` accepted hex prefixes (`0x10`, `0xff`), binary prefixes (`0b101`), and underscores (`1_000`, `_1`) in integer values. The documented grammar allows only an optional `-` followed by decimal digits. These inputs now produce a `SyntaxError`.

Programs that pass hex, binary, or underscore-containing values to integer options or arguments will see parse errors where they previously got a numeric result.

Before:

```pony
// All of these parsed successfully
CommandParser(cs).parse(["app"; "--count=0x10"])  // 16
CommandParser(cs).parse(["app"; "--count=0b101"]) // 5
CommandParser(cs).parse(["app"; "--count=1_000"]) // 1000
```

After:

```pony
// All of these are now SyntaxError
CommandParser(cs).parse(["app"; "--count=0x10"])
CommandParser(cs).parse(["app"; "--count=0b101"])
CommandParser(cs).parse(["app"; "--count=1_000"])

// Decimal digits still work
CommandParser(cs).parse(["app"; "--count=16"])   // 16
CommandParser(cs).parse(["app"; "--count=5"])    // 5
CommandParser(cs).parse(["app"; "--count=1000"]) // 1000
```
