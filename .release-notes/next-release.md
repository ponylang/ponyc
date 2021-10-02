## Remove options package

Removes the options package, which was deprecated in version 0.26.0 in favor of the cli package.

An example program using the options package is immediately below, while the rewrite with the cli package follows it.

```pony
use "options"

actor Main
  let _env: Env
  // Some values we can set via command line options
  var _a_string: String = "default"
  var _a_number: USize = 0
  var _a_unumber: USize = 0
  var _a_float: Float = F64(0.0)

  new create(env: Env) =>
    _env = env
    try
      arguments()?
    end

    _env.out.print("The String is " + _a_string)
    _env.out.print("The Number is " + _a_number.string())
    _env.out.print("The UNumber is " + _a_unumber.string())
    _env.out.print("The Float is " + _a_float.string())

  fun ref arguments() ? =>
    var options = Options(_env.args)

    options
      .add("string", "t", StringArgument)
      .add("number", "i", I64Argument)
      .add("unumber", "u", U64Argument)
      .add("float", "c", F64Argument)

    for option in options do
      match option
      | ("string", let arg: String) => _a_string = arg
      | ("number", let arg: I64) => _a_number = arg.usize()
      | ("unumber", let arg: U64) => _a_unumber = arg.usize()
      | ("float", let arg: F64) => _a_float = arg
      | let err: ParseError => err.report(_env.out) ; usage() ; error
      end
    end

  fun ref usage() =>
    // this exists inside a doc-string to create the docs you are reading
    // in real code, we would use a single string literal for this but
    // docstrings are themselves string literals and you can't put a
    // string literal in a string literal. That would lead to total
    // protonic reversal. In your own code, use a string literal instead
    // of string concatenation for this.
    _env.out.print(
      "program [OPTIONS]\n" +
      "  --string      N   a string argument. Defaults to 'default'.\n" +
      "  --number      N   a number argument. Defaults to 0.\n" +
      "  --unumber     N   a unsigned number argument. Defaults to 0.\n" +
      "  --float       N   a floating point argument. Defaults to 0.0.\n"
      )
```

```pony
use "cli"

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf("run", "", [
          OptionSpec.string("string", "String argument"
            where short' = 't', default' = "default")
          OptionSpec.i64("number", "Number argument"
            where short' = 'i', default' = 0)
          OptionSpec.u64("unumber", "Unsigned number argument"
            where short' = 'u', default' = 0)
          OptionSpec.f64("float", "Float argument"
            where short' = 'c', default' = 0.0)
        ], [])? .> add_help()?
      else
        env.exitcode(-1)  // some kind of coding error
        return
      end

    let cmd =
      match CommandParser(cs).parse(env.args, env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
          ch.print_help(env.out)
          env.exitcode(0)
          return
      | let se: SyntaxError =>
          env.out.print(se.string())
          env.exitcode(1)
          return
      end

    let string = cmd.option("string").string()
    let number = cmd.option("number").i64()
    let unumber = cmd.option("unumber").u64()
    let float = cmd.option("float").f64()

    env.out.print("The String is " + string)
    env.out.print("The Number is " + number.string())
    env.out.print("The UNumber is " + unumber.string())
    env.out.print("The Float is " + float.string())
```

## Fix erratic cycle detector triggering on some Arm systems

Triggering of cycle detector runs was based on an assumption that our "tick" time source would monotonically increase. That is, that the values seen by getting a tick would always increase in value.

This assumption is true as long as we have a hardware source of ticks to use. However, on some Arm systems, the hardware cycle counter isn't available to user code. On these these, we fall back to use `clock_gettime` as a source of ticks. When `clock_gettime` is the source of ticks, the ticks are no longer monotonically increasing.

Usage of the cycle detector with the non-monotonically increasing tick sources would cause the cycle detector run very infrequently and somewhat erratically. This was most likely to be seen on hardware like the Raspberry Pi.

## Fix non-release build crashes on Arm

We've fixed a cause of "random" crashes that impacted at minimum, debug versions of Pony programs running on 32-bit Arm builds on the Raspberry Pi 4.

It's likely that the crashes impacted debug versions of Pony programs running on all Arm systems, but we don't have enough testing infrastructure to know for sure.

## Add Ubuntu 21.04 nightly builds and releases builds

We've added nightly and release builds of ponyc that are built on Ubuntu 21.04. We'll continue supporting them through the end of life of this non-LTS version of Ubuntu.

## Fix major source of runtime instability on non-x86 based platforms

We've replaced our existing code for handling [the ABA problem](https://en.wikipedia.org/wiki/ABA_problem) when running on Arm CPUs. The implementation we replaced was buggy and resulted in runtime instability including crashes and memory corruption.

## Update to LLVM 13.0.0

We've updated the LLVM used to build Pony to 13.0.0.

## Fix segfaults with debug builds on 64-bit Arm

Debug builds on 64-bit Arm platforms were segfaulting. The code generated when doing `ponyc --debug` wouldn't work and would eventually crash. We believe the source to be an LLVM bug but, aren't certain.

We've introduced a work around that fixes the segfault problem, but does some optimization of debug builds for 64-bit Arm platforms. Additional investigation is underway. You can track progress by [following issue #3874](https://github.com/ponylang/ponyc/issues/3874).

