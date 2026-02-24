use "cli"
use "collections"
use "files"
use "path:../lib/ponylang/json-ng/"

actor Main
  """
  CLI entry point for pony-lint. Parses command-line arguments, loads
  configuration, builds the rule registry, and runs the linter.
  """
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf(
          "pony-lint",
          "A linter for Pony source files",
          [
            OptionSpec.string_seq("disable",
              "Disable a rule or category (repeatable)"
              where short' = 'd')
            OptionSpec.string("config",
              "Path to config file" where short' = 'c', default' = "")
            OptionSpec.bool("version",
              "Print version and exit" where short' = 'V', default' = false)
          ],
          [
            ArgSpec.string_seq("paths", "Paths to lint (default: CWD)")
          ])? .> add_help()?
      else
        env.err.print("internal error: invalid command spec")
        env.exitcode(ExitError())
        return
      end

    let cmd =
      match CommandParser(cs).parse(env.args, env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
        ch.print_help(env.out)
        return
      | let se: SyntaxError =>
        env.err.print(se.string())
        env.exitcode(ExitError())
        return
      end

    // Handle --version
    if cmd.option("version").bool() then
      env.out.print("pony-lint " + Version())
      return
    end

    // Get target paths
    let raw_paths = cmd.arg("paths").string_seq()
    let targets: Array[String val] val =
      if raw_paths.size() == 0 then
        recover val [as String val: "."] end
      else
        recover val
          let result = Array[String val]
          for p in raw_paths.values() do
            result.push(p)
          end
          result
        end
      end

    // Load configuration
    let file_auth = FileAuth(env.root)
    let cli_disabled: Array[String] val =
      recover val
        let result = Array[String]
        for d in cmd.option("disable").string_seq().values() do
          result.push(d)
        end
        result
      end
    let cp = cmd.option("config").string()
    let config_path: (String | None) =
      if cp.size() > 0 then cp else None end

    let config =
      match ConfigLoader.from_cli(cli_disabled, config_path, file_auth)
      | let c: LintConfig => c
      | let err: ConfigError =>
        env.err.print("error: " + err.message)
        env.exitcode(ExitError())
        return
      end

    // Build rule registry with all rules
    let all_rules: Array[TextRule val] val = recover val
      let r = Array[TextRule val]
      r.push(LineLength)
      r.push(TrailingWhitespace)
      r.push(HardTabs)
      r.push(CommentSpacing)
      r
    end
    let registry = RuleRegistry(all_rules, config)

    // Run linter
    let cwd = Path.cwd()
    let linter = Linter(registry, file_auth, cwd)
    (let diags, let exit_code) = linter.run(targets)

    // Output diagnostics to stdout
    for diag in diags.values() do
      env.out.print(diag.string())
    end

    env.exitcode(exit_code())
