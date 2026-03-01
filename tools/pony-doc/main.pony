use "cli"
use "files"
use "path:../lib/ponylang/pony_compiler/"
use ast = "pony_compiler"

use @get_compiler_exe_directory[Bool](
  output_path: Pointer[U8] tag,
  argv0: Pointer[U8] tag)
use @ponyint_pool_alloc_size[Pointer[U8] val](size: USize)
use @ponyint_pool_free_size[None](
  size: USize, p: Pointer[U8] tag)

actor Main
  """
  CLI entry point for pony-doc. Parses command-line arguments, compiles the
  target package, extracts the documentation IR, and generates MkDocs output.
  """
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf(
          "pony-doc",
          "Generate MkDocs documentation for Pony packages",
          [
            OptionSpec.string("output",
              "Output directory" where short' = 'o', default' = ".")
            OptionSpec.bool("include-private",
              "Include private types and methods" where default' = false)
            OptionSpec.bool("version",
              "Print version and exit" where short' = 'V', default' = false)
          ],
          [
            ArgSpec.string("package-dir",
              "Package directory to document (default: CWD)"
              where default' = ".")
          ])? .> add_help()?
      else
        env.err.print("internal error: invalid command spec")
        env.exitcode(1)
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
        env.exitcode(1)
        return
      end

    // Handle --version
    if cmd.option("version").bool() then
      env.out.print("pony-doc " + Version())
      return
    end

    // Get target package directory
    let target_dir = cmd.arg("package-dir").string()

    // Get output directory
    let output_dir_path = cmd.option("output").string()

    // Get include_private flag
    let include_private = cmd.option("include-private").bool()

    // Build package search paths
    let package_paths = _build_package_paths(env.vars)

    // Compile the target package
    let file_auth = FileAuth(env.root)
    let target_fp = FilePath(file_auth, target_dir)

    match \exhaustive\ ast.Compiler.compile(
      target_fp where package_search_paths = package_paths,
      limit = ast.PassTraits)
    | let program: ast.Program val =>
      // Extract documentation IR
      let doc_program = Extractor(program, include_private)

      // Generate output
      let output_fp = FilePath(file_auth, output_dir_path)
      try
        MkDocsBackend.generate(doc_program, output_fp, include_private)?
        env.out.print(
          "Documentation generated in "
            + output_dir_path + "/" + doc_program.name + "-docs/")
      else
        env.err.print("error: failed to write documentation output")
        env.exitcode(1)
      end
    | let errors: Array[ast.Error] val =>
      env.err.print("error: compilation failed")
      for err in errors.values() do
        env.err.print("  " + err.msg)
      end
      env.exitcode(1)
    end

  fun _build_package_paths(
    vars: (Array[String val] val | None))
    : Array[String val] val
  =>
    """
    Build the package search path list for AST compilation.

    Installation paths come first (pony-doc is co-installed with ponyc,
    so the standard library is at `../packages` or `../../packages`
    relative to the executable). PONYPATH entries follow.
    """
    // Extract PONYPATH entries before the recover block
    let pony_paths = _get_ponypath_entries(vars)

    recover val
      let paths = Array[String val]
      // Installation paths first
      match _find_exe_directory()
      | let dir: String val =>
        paths.push(Path.join(dir, "../packages"))
        paths.push(Path.join(dir, "../../packages"))
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
    if @get_compiler_exe_directory(buf, "pony-doc".cstring())
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
