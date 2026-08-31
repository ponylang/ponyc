use "files"
use "pony_test"
use ast = "pony_compiler"
use doc = ".."

primitive \nodoc\ _ASTTestHelper
  """
  Compiles a Pony source string through PassTraits and returns the
  extracted DocProgram for use in default-value tests.
  """
  fun extract(
    h: TestHelper,
    source: String val)
    : doc.DocProgram ?
  =>
    let auth = h.env.root
    let tmp =
      FilePath.mkdtemp(
        FileAuth(auth), "pony-doc-ast-test")?
    let pony_file =
      FilePath(
        FileAuth(auth), Path.join(tmp.path, "test.pony"))
    let file = File(pony_file)
    file.write(source)
    file.dispose()

    let pony_path = _get_ponypath(h.env.vars)

    let result =
      try
        let session =
          ast.CompileSession(
            tmp where package_search_paths = pony_path,
            limit = ast.PassParse)
        match session.program()
        | let program: ast.Program val =>
          if not session.continue_to(ast.PassTraits) then
            let err_msg = _format_errors(session.errors())
            session.dispose()
            h.fail(err_msg)
            error
          end
          session.dispose()
          doc.Extractor(program, true)
        else
          let err_msg = _format_errors(session.errors())
          session.dispose()
          h.fail(err_msg)
          error
        end
      else
        h.assert_true(tmp.remove(), "tmpdir cleanup failed")
        error
      end
    h.assert_true(tmp.remove(), "tmpdir cleanup failed")
    result

  fun _format_errors(errors: Array[ast.Error] val): String val =>
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
    match vars
    | let env_vars: Array[String val] val =>
      for pair in env_vars.values() do
        if pair.at("PONYPATH=") then
          return pair.substring(ISize(9))
        end
      end
    end
    ""
