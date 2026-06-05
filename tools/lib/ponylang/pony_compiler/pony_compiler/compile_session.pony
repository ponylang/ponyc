use "files"

class CompileSession
  """
  Resumable compilation session that owns the `_PassOpt` lifecycle.

  Unlike `Compiler.compile()`, which creates a pass_opt, calls `program_load`,
  and immediately tears down the pass_opt, `CompileSession` preserves the
  compilation state so it can be resumed to a higher pass level via
  `continue_to()`. This enables workflows like: compile to `PassParse` to
  inspect the AST, then continue to `PassExpr` to get type information.

  Ownership: `CompileSession` owns the `_PassOpt`. `Program` owns the AST
  (freed in `Program._final()`). The session holds a reference to the
  `Program val` to prevent it from being GC'd while the session exists.

  Callers must call `dispose()` when done to release the pass_opt resources.
  Pony's `_final()` runs with a `box` receiver, which prevents passing the
  `_PassOpt` struct to cleanup FFI functions that require `ref`.
  """
  let _pass_opt: _PassOpt
  var _program: (Program val | None)
  let _raw: Pointer[_AST] val
  var _disposed: Bool

  new create(
    path: FilePath,
    package_search_paths: (String box | ReadSeq[String val] box) = [],
    user_flags: ReadSeq[String val] box = [],
    release: Bool = false,
    limit: PassId = PassAll,
    verbosity: VerbosityLevel = VerbosityQuiet)
  =>
    """
    Compile the source at `path` up to `limit`, retaining the pass_opt for
    later resumption. Arguments mirror `Compiler.compile()`.
    """
    _disposed = false
    _pass_opt = _PassOpt.create()
    @pass_opt_init(_pass_opt)
    _pass_opt.verbosity = verbosity()
    _pass_opt.limit = limit()
    _pass_opt.release = release
    for user_flag in user_flags.values() do
      @define_userflag(_pass_opt.userflags, user_flag.cstring())
    end

    @codegen_pass_init(_pass_opt)
    match package_search_paths
    | let single: String box =>
      @package_add_paths(single.cstring(), _pass_opt)
    | let multiple: ReadSeq[String val] box =>
      for search_path in multiple.values() do
        @package_add_paths(search_path.cstring(), _pass_opt)
      end
    end

    _raw = @program_load(path.path.cstring(), _pass_opt)
    _program =
      if _raw.is_null() then
        None
      else
        Program.create(AST(_raw, _pass_opt.strtab))
      end

  fun program(): (Program val | None) =>
    """
    Returns the compiled program, or `None` if compilation failed.
    """
    _program

  fun ref errors(): Array[Error] val =>
    """
    Returns compilation errors from the pass_opt's typecheck state.

    Useful when `program()` returns `None` or `continue_to()` fails.
    """
    try
      let errs = _pass_opt.check.errors()?
      errs.extract()
    else
      recover val
        [Error.message(
          "Compilation failed but libponyc produced no error messages.")]
      end
    end

  fun ref continue_to(limit: PassId): Bool =>
    """
    Resume compilation from the current pass level to `limit`.

    Returns `true` on success, `false` on failure. On failure, call
    `errors()` to retrieve error details.
    """
    if _raw.is_null() then return false end
    _pass_opt.limit = limit()
    @ast_passes_program(_raw, _pass_opt)

  fun ref dispose() =>
    """
    Release pass_opt resources. Must be called when the session is no longer
    needed. Safe to call multiple times.
    """
    if not _disposed then
      _disposed = true
      @package_done(_pass_opt)
      @codegen_pass_cleanup(_pass_opt)
      // If a Program was produced, it owns the interned-string table and frees
      // it in its _final, so detach it here before pass_opt_done so the table
      // is not freed while the Program's AST still references its strings. (It
      // is held by the AST until then; resumable passes (continue_to) needed it
      // on the pass_opt, which is why this happens at dispose rather than at
      // creation.) If compilation failed there is no Program to own it, so
      // leave it attached and let pass_opt_done free it, else it would leak.
      match _program
      | let _: Program val => _pass_opt.strtab = Pointer[_StrTable]
      end
      @pass_opt_done(_pass_opt)
    end
