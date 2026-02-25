use "files"
use "pony_test"
use ast = "pony_compiler"
use lint = ".."

primitive \nodoc\ _ASTTestHelper
  """
  Compiles a Pony source string at PassParse and returns the Program and
  SourceFile for use in AST rule tests.

  Writes the source to a temporary directory, compiles it, and returns the
  results. The temporary directory persists (acceptable for tiny test files).

  The PONYPATH environment variable must be set to locate the builtin
  package (e.g., the ponyc packages/ directory). Tests run from a build
  directory, not an installed location, so executable-relative discovery
  is not used here.
  """
  fun compile(h: TestHelper, source: String val,
    filename: String val = "test.pony")
    : (ast.Program val, lint.SourceFile val)?
  =>
    let auth = h.env.root
    let tmp = FilePath.mkdtemp(
      FileAuth(auth), "pony-lint-ast-test")?
    let pony_file = FilePath(
      FileAuth(auth), Path.join(tmp.path, filename))
    let file = File(pony_file)
    file.write(source)
    file.dispose()

    let sf = lint.SourceFile(
      pony_file.path, source, tmp.path)

    let pony_path = _get_ponypath(h.env.vars)

    match ast.Compiler.compile(
      tmp where package_search_paths = pony_path,
      limit = ast.PassParse)
    | let program: ast.Program val =>
      (program, sf)
    | let errors: Array[ast.Error] val =>
      let msg = recover val
        let s = String
        s.append("AST compilation failed:")
        for err in errors.values() do
          s.append("\n  ")
          s.append(err.msg)
        end
        s
      end
      h.fail(msg)
      error
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
