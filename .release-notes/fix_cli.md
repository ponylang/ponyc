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
