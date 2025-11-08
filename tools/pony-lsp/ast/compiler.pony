use "files"

primitive Compiler
  """
  Main entry point for obtaining a `Program` that wraps up the Pony `AST`
  produced by the underlying libponyc.

  This "Compiler" here is calling the libponyc function `program_load`
  and configures it to compile up to the `finaliser` pass with `quiet` verbosity.
  So this is not producing any Pony binary, ASM or LLVM IR, but it is giving you
  some convenient wrapper around the libponyc `ast_t` that represents the Pony program.
  """
  fun compile(
    path: FilePath,
    package_search_paths: (String box | ReadSeq[String val] box) = [])
  : (Program val^ | Array[Error] val^)
  =>
    """
    Compile the pony source code at the given `path` with the paths in `package_search_paths`
    to look for "used" packages. Those can be colon-separated lists of paths.

    It is commonly used with PonyPath, extracting the PONYPATH environment variable.
    """
    let pass_opt = _PassOpt.create()
    @pass_opt_init(pass_opt)
    pass_opt.verbosity = VerbosityLevels.quiet()
    pass_opt.limit = PassIds.finaliser()
    pass_opt.release = false

    @codegen_pass_init(pass_opt)
    // avoid calling package_init
    // get the search paths from the arguments
    match package_search_paths
    | let single: String box =>
      @package_add_paths(single.cstring(), pass_opt)
    | let multiple: ReadSeq[String val] box =>
      for search_path in multiple.values() do
        @package_add_paths(search_path.cstring(), pass_opt)
      end
    end

    // TODO: parse builtin before and keep it around, so we don't need to
    // process it over and over
    let program_ast = @program_load(path.path.cstring(), pass_opt)
    let res =
        if program_ast.is_null() then
          try
            let errors = pass_opt.check.errors()?
            errors.extract() // extracts an array of errors
          else
            recover val
              [
                Error.message("Compilation failed but libponyc produced no error messages.")
              ]
            end
          end
        else
          Program.create(AST(program_ast))
        end

    @package_done(pass_opt)
    @codegen_pass_cleanup(pass_opt)
    @pass_opt_done(pass_opt)
    res
