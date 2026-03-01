use "files"
use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  fun tag tests(test: PonyTest) =>
    test(_TestFromFile("json", "json_test.json", "json_out.txt"))

class \nodoc\ iso _TestFromFile is UnitTest
  let _example: String
  let _test: String
  let _expect: String

  new iso create(example: String, test: String, expect: String) =>
    (_example, _test, _expect) = (example, test, expect)

  fun name(): String =>
    _example

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let peg_file = FilePath(auth, "examples/" + _example + ".peg")
    let test_file = FilePath(auth, "test/" + _test)
    let expect_file = FilePath(auth, "test/" + _expect)
    match \exhaustive\ recover val PegCompiler(Source(peg_file)?) end
    | let p: Parser val =>
      let out = peg_run(h, p, Source(test_file)?)?
      with file = OpenFile(expect_file) as File do
        h.assert_eq[String](file.read_string(file.size()), out)
      end
    | let errors: Array[PegError] val =>
      for e in errors.values() do
        h.env.err.printv(PegFormatError.console(e))
      end
    end

  fun peg_run(h: TestHelper, p: Parser val, source: Source): String ? =>
    match recover val p.parse(source) end
    | (_, let r: ASTChild) => recover Printer(r) end
    | (let offset: USize, let r: Parser val) =>
      let e = recover val SyntaxError(source, offset, r) end
      h.env.out.writev(PegFormatError.console(e))
      error
    else error
    end
