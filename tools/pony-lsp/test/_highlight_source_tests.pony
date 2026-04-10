use "pony_test"
use "files"
use "collections"
use "pony_compiler"
use ".."
use "../workspace"

primitive _HighlightSourceTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_HighlightSourceFieldHasInitializerTest)

class \nodoc\ iso _HighlightSourceFieldHasInitializerTest is UnitTest
  """
  Unit tests for HighlightSource.field_has_initializer.

  Compiles `highlight_source_cases/fixture.pony` at PassFinaliser (the same
  pass the LSP uses), walks the AST to find field nodes in `_FieldInitCases`,
  and asserts the correct result for each case:

  - `with_init: U32 = 0`      → true  (has initializer)
  - `without_init: U32`        → false (no initializer)
  - `eq_in_comment: U32 // default=0` → false (= is inside a comment)
  - `flet_with_init: Bool = false`    → true
  - `flet_without_init: Bool`         → false
  """
  fun name(): String => "highlight_source/field_has_initializer"

  fun apply(h: TestHelper) =>
    let file_auth = FileAuth(h.env.root)
    let fixture_dir =
      Path.join(Path.dir(__loc.file()), "highlight_source_cases")
    let path = FilePath(file_auth, fixture_dir)

    // Build stdlib search paths the same way _LspTestServer does: find the
    // test binary's directory, then try ../packages and ../../packages.
    let exe_dir = Path.dir(try h.env.args(0)? else "" end)
    let search_paths: Array[String val] val = recover val
      [ Path.join(exe_dir, "../packages")
        Path.join(exe_dir, "../../packages") ]
    end

    match Compiler.compile(path, search_paths where limit = PassFinaliser)
    | let program: Program val =>
      let results = _collect_field_init_results(program)
      h.assert_eq[Bool](
        true,
        results.get_or_else("with_init", false),
        "with_init (var f: U32 = 0) should have initializer")
      h.assert_eq[Bool](
        false,
        results.get_or_else("without_init", true),
        "without_init (var f: U32) should have no initializer")
      h.assert_eq[Bool](
        false,
        results.get_or_else("eq_in_comment", true),
        "eq_in_comment (var f: U32 // default=0) should have no initializer")
      h.assert_eq[Bool](
        true,
        results.get_or_else("flet_with_init", false),
        "flet_with_init (let f: Bool = false) should have initializer")
      h.assert_eq[Bool](
        false,
        results.get_or_else("flet_without_init", true),
        "flet_without_init (let f: Bool) should have no initializer")
    | let errors: Array[Error] val =>
      h.fail(
        "Fixture compilation failed: " +
        try errors(0)?.msg else "(no message)" end)
    end

  fun _collect_field_init_results(
    program: Program val)
    : Map[String, Bool]
  =>
    """
    Walk all packages/modules in the program, find every field declaration
    (`tk_fvar` / `tk_flet`), and record `field_has_initializer` by field name.
    """
    let results = Map[String, Bool].create()
    for package in program.packages() do
      for module in package.modules() do
        let visitor = _FieldInitVisitor(results)
        module.ast.visit(visitor)
      end
    end
    results

class ref _FieldInitVisitor is ASTVisitor
  let _results: Map[String, Bool] ref

  new ref create(results: Map[String, Bool] ref) =>
    _results = results

  fun ref visit(ast: AST box): VisitResult =>
    match ast.id()
    | TokenIds.tk_fvar() | TokenIds.tk_flet() =>
      try
        let name = ast(0)?.token_value() as String
        _results(name) = HighlightSource.field_has_initializer(ast)
      end
    end
    Continue
