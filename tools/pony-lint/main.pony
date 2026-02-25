use "cli"
use "collections"
use "files"
use "path:../lib/ponylang/json-ng/"
use "path:../lib/ponylang/pony_compiler/"

use @get_compiler_exe_directory[Bool](
  output_path: Pointer[U8] tag,
  argv0: Pointer[U8] tag)
use @ponyint_pool_alloc_size[Pointer[U8] val](size: USize)
use @ponyint_pool_free_size[None](
  size: USize, p: Pointer[U8] tag)

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
    let all_ast_rules: Array[ASTRule val] val = recover val
      let r = Array[ASTRule val]
      r.push(TypeNaming)
      r.push(MemberNaming)
      r.push(AcronymCasing)
      r.push(FileNaming)
      r.push(PackageNaming)
      r
    end
    let registry = RuleRegistry(all_rules, all_ast_rules, config)

    // Build package search paths: installation paths first (prevents
    // PONYPATH from overriding builtin, per ponylang/ponyc#3779),
    // then PONYPATH.
    let package_paths = _build_package_paths(env.vars)

    // Run linter
    let cwd = Path.cwd()
    let linter = Linter(registry, file_auth, cwd, package_paths)
    (let diags, let exit_code) = linter.run(targets)

    // Output diagnostics to stdout
    for diag in diags.values() do
      env.out.print(diag.string())
    end

    env.exitcode(exit_code())

  fun _build_package_paths(
    vars: (Array[String val] val | None))
    : Array[String val] val
  =>
    """
    Build the package search path list for AST compilation.

    Installation paths come first (pony-lint is co-installed with
    ponyc, so the standard library is at `../packages` or
    `../../packages` relative to the executable). PONYPATH entries
    follow.
    """
    // Extract PONYPATH entries before the recover block
    let pony_paths = _get_ponypath_entries(vars)

    recover val
      let paths = Array[String val]
      // Installation paths first
      match _find_exe_directory()
      | let dir: String val =>
        paths.push(Path.join(dir, "../packages"))
        paths.push(
          Path.join(dir, "../../packages"))
      end
      // Then PONYPATH
      for p in pony_paths.values() do
        paths.push(p)
      end
      paths
    end

  fun _get_ponypath_entries(
    vars: (Array[String val] val | None))
    : Array[String val] val
  =>
    """Extract PONYPATH entries as an array of paths."""
    match vars
    | let env_vars: Array[String val] val =>
      for pair in env_vars.values() do
        if pair.at("PONYPATH=") then
          let pony_path: String val =
            pair.substring(ISize(9))
          return Path.split_list(pony_path)
        end
      end
    end
    recover val Array[String val] end

  fun _find_exe_directory(): (String val | None) =>
    """
    Find the directory containing the currently running executable
    using the same platform-specific mechanism as ponyc.
    """
    let buf_size: USize = 4096
    let buf = @ponyint_pool_alloc_size(buf_size)
    if @get_compiler_exe_directory(buf, "pony-lint".cstring())
    then
      let result = recover val
        String.copy_cstring(buf)
      end
      @ponyint_pool_free_size(buf_size, buf)
      result
    else
      @ponyint_pool_free_size(buf_size, buf)
      None
    end
