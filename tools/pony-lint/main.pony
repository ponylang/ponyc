use "cli"
use "collections"
use "files"
use "path:../lib/ponylang/pony_compiler/"

use @get_compiler_exe_directory[Bool](
  output_path: Pointer[U8] tag,
  argv0: Pointer[U8] tag)
use @strlen[USize](s: Pointer[U8] tag)

actor Main
  """
  CLI entry point for pony-lint. Parses command-line arguments, loads
  configuration, builds the rule registry, and runs the linter.

  The AST phase processes one package per behavior call so GC can collect
  each `Program val` (and its C-level AST) before the next compilation.
  Without this, repos with many packages (e.g., 35 example programs each
  importing the same library) accumulate all ASTs in memory at once.
  """
  let _env: Env
  var _linter: Linter
  var _all_diags: Array[Diagnostic val]
  var _has_error: Bool
  let _remaining: Array[_PackageInfo val]
  var _file_info: Map[String, _FileInfo val] val

  new create(env: Env) =>
    _env = env
    // Placeholders — overwritten below on the happy path, needed for
    // early-return branches where the actor fields must be initialized.
    _linter =
      Linter(
        RuleRegistry(
          recover val Array[TextRule val] end,
          recover val Array[ASTRule val] end,
          LintConfig.default()),
        FileAuth(env.root),
        "")
    _all_diags = Array[Diagnostic val]
    _has_error = false
    _remaining = Array[_PackageInfo val]
    _file_info = recover val Map[String, _FileInfo val] end

    let cs =
      try
        CommandSpec.leaf(
          "pony-lint",
          "A linter for Pony source files",
          [
            OptionSpec.string_seq(
              "disable",
              "Disable a rule or category (repeatable)"
              where short' = 'd')
            OptionSpec.string(
              "config",
              "Path to config file" where short' = 'c', default' = "")
            OptionSpec.bool(
              "version",
              "Print version and exit" where short' = 'V', default' = false)
            OptionSpec.string(
              "explain",
              "Print rule description and documentation URL"
              where default' = "")
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
      match \exhaustive\ CommandParser(cs).parse(env.args, env.vars)
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

    // Build rule arrays (needed by --explain and normal linting)
    let all_rules: Array[TextRule val] val =
      recover val Array[TextRule val]
        .> push(LineLength)
        .> push(TrailingWhitespace)
        .> push(HardTabs)
        .> push(CommentSpacing)
        .> push(IndentationSize)
    end
    let all_ast_rules: Array[ASTRule val] val =
      recover val Array[ASTRule val]
        .> push(TypeNaming)
        .> push(MemberNaming)
        .> push(AcronymCasing)
        .> push(FileNaming)
        .> push(PackageNaming)
        .> push(PublicDocstring)
        .> push(MatchSingleLine)
        .> push(MatchCaseIndent)
        .> push(PartialSpacing)
        .> push(PartialCallSpacing)
        .> push(DotSpacing)
        .> push(BlankLines)
        .> push(DocstringFormat)
        .> push(DocstringLeadingBlank)
        .> push(PackageDocstring)
        .> push(PreferChaining)
        .> push(OperatorSpacing)
        .> push(LambdaSpacing)
        .> push(AssignmentIndent)
        .> push(ControlStructureAlignment)
        .> push(TypeAliasFormat)
        .> push(ArrayLiteralFormat)
        .> push(MethodDeclarationFormat)
        .> push(TypeParameterFormat)
        .> push(CallArgumentFormat)
        .> push(ExhaustiveMatch)
    end

    // Handle --explain
    let explain_id = cmd.option("explain").string()
    if explain_id.size() > 0 then
      for rule in all_rules.values() do
        if rule.id() == explain_id then
          env.out.print(ExplainHelpers.format(
            rule.id(), rule.description(), rule.default_status()))
          return
        end
      end
      for rule in all_ast_rules.values() do
        if rule.id() == explain_id then
          env.out.print(ExplainHelpers.format(
            rule.id(), rule.description(), rule.default_status()))
          return
        end
      end
      env.err.print("error: unknown rule '" + explain_id + "'")
      env.exitcode(ExitError())
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

    (let config, let root_dir) =
      match \exhaustive\
        ConfigLoader.from_cli(cli_disabled, config_path, file_auth)
      | (let c: LintConfig, let r: String val) => (c, r)
      | let err: ConfigError =>
        env.err.print("error: " + err.message)
        env.exitcode(ExitError())
        return
      end

    // Validate config keys against known rules
    let known_keys =
      recover val
        let s = Set[String]
        for rule in all_rules.values() do
          s.set(rule.id())
          s.set(rule.category())
        end
        for rule in all_ast_rules.values() do
          s.set(rule.id())
          s.set(rule.category())
        end
        s
      end
    match config.validate(known_keys)
    | let err: ConfigError =>
      env.err.print("error: " + err.message)
      env.exitcode(ExitError())
      return
    end

    // Build rule registry
    let registry = RuleRegistry(all_rules, all_ast_rules, config)

    // Build package search paths: installation paths first (prevents
    // PONYPATH from overriding builtin, per ponylang/ponyc#3779),
    // then PONYPATH.
    let argv0 = try env.args(0)? else "" end
    let package_paths = _build_package_paths(env.vars, argv0)

    // Run text phase synchronously
    let cwd = Path.cwd()
    _linter = Linter(registry, file_auth, cwd, package_paths, root_dir)
    let text_result = _linter.run_text_phase(targets)
    _has_error = text_result.has_error
    _file_info = text_result.file_info

    // Copy text diagnostics into mutable accumulator
    for d in text_result.diagnostics.values() do
      _all_diags.push(d)
    end

    // Copy package list into mutable array for shift()
    for pkg in text_result.packages.values() do
      _remaining.push(pkg)
    end

    // Start AST phase — one package per behavior call
    if _remaining.size() > 0 then
      _process_next_package()
    else
      _finish()
    end

  be _process_next_package() =>
    """
    Compile and lint one package. GC can run between behavior calls,
    collecting the previous iteration's `Program val` and its C-level AST.
    """
    let pkg =
      try _remaining.shift()?
      else
        _finish()
        return
      end
    (let diags, let had_error) =
      _linter.run_ast_package(
        pkg.dir, pkg.file_paths, _file_info, pkg.registry)
    for d in diags.values() do
      _all_diags.push(d)
    end
    if had_error then _has_error = true end

    if _remaining.size() > 0 then
      _process_next_package()
    else
      _finish()
    end

  fun ref _finish() =>
    """
    Sort diagnostics, print them, and set the exit code.
    """
    Sort[Array[Diagnostic val], Diagnostic val](_all_diags)

    for diag in _all_diags.values() do
      _env.out.print(diag.string())
    end

    let exit_code: ExitCode =
      if _has_error then
        ExitError
      elseif _all_diags.size() > 0 then
        ExitViolations
      else
        ExitSuccess
      end
    _env.exitcode(exit_code())

  fun _build_package_paths(
    vars: (Array[String val] val | None),
    argv0: String val)
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
      match _find_exe_directory(argv0)
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
    """
    Extract PONYPATH entries as an array of paths.
    """
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

  fun _find_exe_directory(argv0: String val): (String val | None) =>
    """
    Find the directory containing the currently running executable
    using the same platform-specific mechanism as ponyc.
    """
    // Hand C a Pony-allocated buffer to fill, then keep it as the result
    // String: it is a Pony object start to finish, never foreign memory.
    let buf = recover Array[U8] .> undefined(USize(4096)) end
    if @get_compiler_exe_directory(buf.cpointer(), argv0.cstring()) then
      buf.truncate(@strlen(buf.cpointer()))
      String.from_array(consume buf)
    else
      None
    end
