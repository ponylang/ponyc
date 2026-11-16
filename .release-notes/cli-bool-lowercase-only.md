## cli bool options and args now accept only lowercase true and false

`OptionSpec.bool` and `ArgSpec.bool` accepted any letter case for `true` and `false` — `TRUE`, `True`, `tRuE`, `FALSE`, and so on all parsed successfully. The documented grammar allows only lowercase `true` and `false`. These inputs now produce a `SyntaxError`.

Programs that pass non-lowercase boolean values to bool options or arguments will see parse errors where they previously got a result.

Before:

```pony
// All of these parsed successfully
CommandParser(cs).parse(["app"; "--flag=TRUE"])   // true
CommandParser(cs).parse(["app"; "--flag=True"])   // true
CommandParser(cs).parse(["app"; "--flag=FALSE"])  // false
```

After:

```pony
// All of these are now SyntaxError
CommandParser(cs).parse(["app"; "--flag=TRUE"])
CommandParser(cs).parse(["app"; "--flag=True"])
CommandParser(cs).parse(["app"; "--flag=FALSE"])

// Lowercase still works
CommandParser(cs).parse(["app"; "--flag=true"])   // true
CommandParser(cs).parse(["app"; "--flag=false"])  // false
```
