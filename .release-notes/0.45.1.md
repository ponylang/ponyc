## Fix underlying time source for Time.nanos() on macOS

Previously, we were using `mach_absolute_time` on macOS to get the number of ticks since boot, with the assumption that ticks increment at nanosecond intervals. However, as noted by [Apple](https://developer.apple.com/documentation/apple-silicon/addressing-architectural-differences-in-your-macos-code#Apply-Timebase-Information-to-Mach-Absolute-Time-Values), this assumption is flawed on Apple Silicon, and will also affect any binaries that were built on Intel, as the Rosetta 2 translator will not apply any time conversion.

The recommended replacement to `mach_absolute_time` is to use `clock_gettime_nsec_np` with a clock of type `CLOCK_UPTIME_RAW`.

## Fix `cli` package from mangling option arguments with equal signs

The current command parser in the `cli` package cuts option arguments of `String` type at the first equal sign. This release fixes the problem for long options (`--option=foo=bar`) and for short options such as `-O=foo=bar`. Short options such as `-Ofoo=bar` will continue to raise an "ambiguous args" error.

The code below shows the bug, with the option argument being cut short at the first equal sign. The code below prints "foo" instead of the complete value, "foo=bar". The same is true when uses the short version of the option, like `-t=foo=bar`.

```pony
use "cli"

actor Main
  new create(env: Env) =>
    try
      let cs =
        CommandSpec.leaf("simple", "", [
          OptionSpec.string("test" where short' = 't')
        ])?
      
      let args: Array[String] = [
        "ignored"; "--test=foo=bar"
      ]
      let cmdErr = CommandParser(cs).parse(args) as Command
      env.out.print(cmdErr.option("test").string())
    end
```

