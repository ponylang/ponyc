use "cli"
use "collections"

class val _Options
  let test_path: String
  let compiler_path: String
  let output: String
  let pony_path: String
  let test_lib: String
  let timeout_s: U64
  let debug: Bool
  let debugger: String
  let verbose: Bool
  let max_parallel: USize
  let only: Set[String] val
  let exclude: Set[String] val

  new create(command: Command) =>
    test_path = command.arg(_CliOptions._str_test_path()).string()

    compiler_path = command.option(_CliOptions._compiler_path()).string()
    output = command.option(_CliOptions._str_output()).string()
    pony_path = command.option(_CliOptions._str_pony_path()).string()
    test_lib = command.option(_CliOptions._str_test_lib()).string()
    timeout_s = command.option(_CliOptions._str_timeout_s()).u64()
    debug = command.option(_CliOptions._str_debug()).bool()
    debugger = command.option(_CliOptions._str_debugger()).string()
    verbose = command.option(_CliOptions._str_verbose()).bool()
    max_parallel = USize.from[U64](
      command.option(_CliOptions._str_max_parallel()).u64())
    only =
      recover
        let only' = Set[String]
        let only_str = command.option(_CliOptions._str_only()).string()
        only'.union(only_str.split(",").values())
        only'
      end
    exclude =
      recover
        let exclude' = Set[String]
        let exclude_str = command.option(_CliOptions._str_exclude()).string()
        exclude'.union(exclude_str.split(",").values())
        exclude'
      end

primitive _CliOptions
  fun _str_test_path(): String => "test_path"
  fun _compiler_path(): String => "compiler"
  fun _str_output(): String => "output"
  fun _str_pony_path(): String => "pony_path"
  fun _str_test_lib(): String => "test_lib"
  fun _str_timeout_s(): String => "timeout_s"
  fun _str_debug(): String => "debug"
  fun _str_debugger(): String => "debugger"
  fun _str_verbose(): String => "verbose"
  fun _str_max_parallel(): String => "max_parallel"
  fun _str_only(): String => "only"
  fun _str_exclude(): String => "exclude"

  fun apply(env: Env): _Options ? =>
    let spec =
      recover val
        CommandSpec.leaf("runner", "Test runner for Pony compiler tests", [
          OptionSpec.string(_compiler_path(), "Path to the ponyc binary"
            where short' = 'c', default' = "ponyc")
          OptionSpec.string(_str_output(), "Output directory for test programs"
            where short' = 'o', default' = ".")
          OptionSpec.string(_str_pony_path(), "Path to pass to ponyc --path"
            where short' = 'p', default' = "")
          OptionSpec.string(_str_test_lib(),
            "Directory containing the additional test libraries"
            where short' = 'L', default' = "test_lib")
          OptionSpec.u64(_str_timeout_s(),
            "Timeout (in seconds) for any one test"
            where short' = 't', default' = 60)
          OptionSpec.bool(_str_debug(), "Whether to compile in debug mode"
            where short' = 'd', default' = false)
          OptionSpec.string(_str_debugger(), "Debugger to use when running"
            where short' = 'g', default' = "")
          OptionSpec.bool(_str_verbose(), "Whether to print more progress info"
            where short' = 'v', default' = false)
          OptionSpec.u64(_str_max_parallel(),
            "Maximum number of tests to run in parallel"
            where short' = 'P', default' = 1)
          OptionSpec.string(_str_only(),
            "Comma-separated list of test names to run"
            where short' = 'O', default' = "")
          OptionSpec.string(_str_exclude(),
            "Comma-separated list of directories to exclude"
            where short' = 'e', default' = "")
        ], [
          ArgSpec.string("test_path", "Directory containing test directories"
            where default' = ".")
        ])? .> add_help()?
      end

    let options =
      recover val
        match \exhaustive\ CommandParser(spec).parse(env.args, env.vars)
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
      end
    options
