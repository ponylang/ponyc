use "options"

actor Main
  new create(env: Env) =>
    var opt = Options(env)

    opt
      .usage_text(
        """
        ponyc [OPTIONS] <package directory>

        The package directory defaults to the current directory.

        Often needed options:
        """
        )
      .add("opt", "O", "Optimise the output.", None)
      .add("path", "p", "Add an additional search path.", StringArgument)
      .add("output", "o", "Write output to this directory.", StringArgument)
      .usage_text("Rarely needed options:")
      .add("ieee-math", None, "Force strict IEEE 754 compliance.", None)
      .add("cpu", "c", "Set the target CPU.", StringArgument)
      .add("features", "f", "CPU features to enable or disable.", StringArgument)
      .add("triple", None, "Set the target triple.", None)
      .usage_text("Debugging options:")
      .add("pass", "r", "Restrict phases.", StringArgument)
      .param("parse", None)
      .param("parsefix", None)
      .param("sugar", None)
      .param("scope1", None)
      .param("name", None)
      .param("flatten", None)
      .param("traits", None)
      .param("scope2", None)
      .param("expr", None)
      .param("ir", "Output LLVM IR.")
      .param("bitcode", "Output LLVM bitcode.")
      .param("asm", "Output assembly")
      .param("obj", "Output an object file.")
      .param("all", "The default: generate an executable.")
      .add("ast", "a", "Output an abstract syntax tree.", None)
      .add("trace", "t", "Enable parse trace.", None)
      .add("width", "w", "Width to target when printing the AST.", I64Argument)
      .add("immerr", None, "Report errors immediately rather than deferring.",
        None)

    for option in opt do
      //Since short option names are optional, matching is based on the long opt.
      match option
      | ("opt", None) =>
          env.stdout.print("Output will be optimised!")
      | ("path", var arg: String) =>
          env.stdout.print("Search path: " + arg)
      | ("output", var arg: String) =>
          env.stdout.print("Output path: " + arg)
      | ("ieee-math", None) =>
          env.stdout.print("Enforce IEEE 754 math")
      | ("cpu", var arg: String) =>
          env.stdout.print("Target CPU: " + arg)
      | ("features", var arg: String) =>
          env.stdout.print("Feature settings: " + arg)
      | ("triple", var arg: String) =>
          env.stdout.print("Target triple: " + arg)
      | ("pass", var arg: String) =>
          env.stdout.print("Restrict phase to: " + arg)
      | ("ast", None) =>
          env.stdout.print("Printing an AST!")
      | ("trace", None) =>
          env.stdout.print("Enable trace parse!")
      | ("width", var arg: I64) =>
          env.stdout.print("Target AST width: " + arg.string())
      | ("immerr", None) =>
          env.stdout.print("Errors will be reported immediately!")
      | ParseError => opt.usage()
      end
    end
