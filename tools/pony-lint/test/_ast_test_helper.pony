use "files"
use "pony_test"
use ast = "pony_compiler"
use lint = ".."

primitive \nodoc\ _ASTTestHelper
  """
  Compiles a Pony source string and returns the Program and SourceFile for use
  in AST rule tests.

  Writes the source to a temporary directory, compiles it, and returns the
  results. The temporary directory persists (acceptable for tiny test files).

  The PONYPATH environment variable must be set to locate the builtin
  package (e.g., the ponyc packages/ directory). Tests run from a build
  directory, not an installed location, so executable-relative discovery
  is not used here.

  The optional `pass` parameter controls the compilation pass level.
  When `PassParse` (the default), uses `Compiler.compile()` directly. For
  later passes, uses `CompileSession` to compile to `PassParse` first, then
  resumes to the requested level.
  """
  fun compile(
    h: TestHelper,
    source: String val,
    filename: String val = "test.pony",
    pass: ast.PassId = ast.PassParse)
    : (ast.Program val, lint.SourceFile val) ?
  =>
    let auth = h.env.root
    let tmp =
      FilePath.mkdtemp(
        FileAuth(auth), "pony-lint-ast-test")?
    let pony_file =
      FilePath(
        FileAuth(auth), Path.join(tmp.path, filename))
    let file = File(pony_file)
    file.write(source)
    file.dispose()

    let sf =
      lint.SourceFile(pony_file.path, source, tmp.path)

    let pony_path = _get_ponypath(h.env.vars)

    match \exhaustive\ pass
    | ast.PassParse =>
      match \exhaustive\ ast.Compiler.compile(
        tmp where package_search_paths = pony_path,
        limit = ast.PassParse)
      | let program: ast.Program val =>
        (program, sf)
      | let errors: Array[ast.Error] val =>
        h.fail(_format_errors(errors))
        error
      end
    else
      let session =
        ast.CompileSession(
          tmp where package_search_paths = pony_path,
          limit = ast.PassParse)
      match session.program()
      | let program: ast.Program val =>
        if not session.continue_to(pass) then
          let err_msg = _format_errors(session.errors())
          session.dispose()
          h.fail(err_msg)
          error
        end
        session.dispose()
        (program, sf)
      else
        let err_msg = _format_errors(session.errors())
        session.dispose()
        h.fail(err_msg)
        error
      end
    end

  fun _format_errors(errors: Array[ast.Error] val): String val =>
    """Format compilation errors into a failure message."""
    recover val
      let s = String
      s.append("AST compilation failed:")
      for err in errors.values() do
        s.append("\n  ")
        s.append(err.msg)
      end
      s
    end

  fun _get_ponypath(
    vars: (Array[String val] val | None))
    : String val
  =>
    """Extract PONYPATH from environment variables."""
    match vars
    | let env_vars: Array[String val] val =>
      for pair in env_vars.values() do
        if pair.at("PONYPATH=") then
          return pair.substring(ISize(9))
        end
      end
    end
    ""
