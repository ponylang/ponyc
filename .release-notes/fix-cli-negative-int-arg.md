## Fix CommandParser rejecting negative number positional arguments

`CommandParser` rejected negative numbers as positional arguments, reporting them as unknown short options. `app -5` with an `ArgSpec.i64` positional returned `SyntaxError: unknown short option at: '5'` instead of parsing `-5` as the argument value. The same problem affected `ArgSpec.f64` with values like `-1.5`.

Negative numbers as positional arguments now parse directly. The `--` separator and named-option forms (`-n -5`, `--num=-5`) continue to work.

```pony
let spec = CommandSpec.leaf("app", "", Array[OptionSpec](), [
  ArgSpec.i64("n", "")
])?
let cmd = CommandParser(spec).parse(["app"; "-5"]) as Command
cmd.arg("n").i64() // was: SyntaxError, now: -5
```
