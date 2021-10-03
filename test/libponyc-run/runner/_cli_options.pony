use "cli"

class _Options
  let path: String
  let ponyc: String
  let pony_path: String
  let test_lib: String
  let timeout: I64
  let debug: Bool
  let verbose: Bool

  new create(command: Command) =>
    path = command.option(_CliOptions._str_path()).string()
    ponyc = command.option(_CliOptions._str_ponyc()).string()
    pony_path = command.option(_CliOptions._str_pony_path()).string()
    test_lib = command.option(_CliOptions._str_test_lib()).string()
    timeout = command.option(_CliOptions._str_timeout()).i64()
    debug = command.option(_CliOptions._str_debug()).bool()
    verbose = command.option(_CliOptions._str_debug()).bool()

primitive _CliOptions
  fun _str_path(): String => "path"
  fun _str_ponyc(): String => "ponyc"
  fun _str_pony_path(): String => "pony_path"
  fun _str_test_lib(): String => "test_lib"
  fun _str_timeout(): String => "timeout"
  fun _str_debug(): String => "debug"
  fun _str_verbose(): String => "verbose"

  fun apply(env: Env): _Options ? =>
    let spec =
      CommandSpec.leaf("runner", "Test runner for Pony compiler tests", [
        OptionSpec.string(_str_path(), "Path to search for tests"
          where short' = 'p', default' = ".")
        OptionSpec.string(_str_ponyc(), "Path to the ponyc binary"
          where short' = 'c', default' = "ponyc")
        OptionSpec.string(_str_pony_path(), "Path to pass to ponyc --path"
          where short' = 'P', default' = "")
        OptionSpec.string(_str_test_lib(),
          "Directory containing the additional test libraries"
          where short' = 'L', default' = "test_lib")
        OptionSpec.i64(_str_timeout(), "Timeout (in seconds) for any one test"
          where short' = 't', default' = 5)
        OptionSpec.bool(_str_debug(), "Whether to compile in debug mode"
          where short' = 'd', default' = false)
        OptionSpec.bool(_str_verbose(), "Whether to print more progress info"
          where short' = 'v', default' = false)
      ], [
        ArgSpec.string("test_path", "Directory containing test directories"
          where default' = ".")
      ])? .> add_help()?

    let options =
      match CommandParser(spec).parse(env.args, env.vars)
      | let command: Command =>
        _Options(command)
      | let help: CommandHelp =>
        help.print_help(env.out)
        env.exitcode(0)
        error
      | let syntax_error: SyntaxError =>
        env.err.print(syntax_error.string())
        env.exitcode(1)
        error
      end
    options
