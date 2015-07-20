use "options"

actor Main
  new create(env: Env) =>
    // defaults to fatal error, to continue parsing supply 'false' as second
    // param.
    var opt = Options(env)

    opt
      .add("version", "v")
      .add("debug", "d")
      .add("strip", "s")
      .add("path", "p", StringArgument)
      .add("output", "o", StringArgument) 
      .add("library", "l")
      .add("safe", None, StringArgument, Optional)
      .add("ieee-math")
      .add("restrict")
      .add("cpu", None, StringArgument)
      .add("features", None, StringArgument)
      .add("triple", None, StringArgument)
      .add("stats")
      .add("pass", "r", StringArgument)
      .add("ast", "a")
      .add("trace", "t")
      .add("width", "w", I64Argument)
      .add("immerr")
      .add("verify")
      .add("bnf")
      .add("antlr")
      .add("antlrraw")

    for option in opt do
      match option
      | ("version", None) => env.out.print("Print version!")
      | ("debug", None) => env.out.print("Output will be not optimised!")
      | ("strip", None) => env.out.print("Strip debug symbols!")
      | ("path", let arg: String) => env.out.print("Search path: " + arg)
      | ("output", let arg: String) => env.out.print("Output path: " + arg)
      | ("library", None) => env.out.print("A library will be produced!")
      | ("safe", None) => env.out.print("C-FFI: builtin")
      | ("safe", let arg: String) => env.out.print("C-FFI: " + arg)
      | ("ieee-math", None) => env.out.print("Enforce IEEE 754 math")
      | ("restrict", None) => env.out.print("Fortran pointer semantics!")
      | ("cpu", let arg: String) => env.out.print("Target CPU: " + arg)
      | ("features", let arg: String) => env.out.print("Features: " + arg)
      | ("triple", let arg: String) => env.out.print("Target triple: " + arg)
      | ("stats", None) => env.out.print("Print compilation stats!")
      | ("pass", let arg: String) => env.out.print("Pass limit: " + arg)
      | ("ast", None) => env.out.print("Printing an AST!")
      | ("trace", None) => env.out.print("Enable trace parse!")
      | ("width", let arg: I64) => env.out.print("AST width: " + arg.string())
      | ("immerr", None) => env.out.print("Report errors immediately!")
      | ("verify", None) => env.out.print("Verify LLVM IR!")
      | ("bnf", None) => env.out.print("Output human readable BNF!")
      | ("antlr", None) => env.out.print("Produce ANTLR output!")
      | ("antlrraw", None) => env.out.print("ANTLRRAW")
      | let failure: ParseError => failure.report(env.out) ; env.exitcode(-1)
      end
    end
