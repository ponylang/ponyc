use "files"
use "pony_test"

use "../pony_compiler"

class \nodoc\ iso _ASTEndPosRenamedTypeParam is UnitTest
  """
  The compiler renames the type parameters of a method copied from a trait.
  The end position of the renamed ID covers only the name in the source.
  """
  fun name(): String => "ast/end-pos-renamed-type-param"

  fun apply(h: TestHelper) ? =>
    let source_dir = Path.join(Path.dir(__loc.file()), "inherited_typeparam")
    let pony_path = PonyPath(h.env) as String
    match \exhaustive\ Compiler.compile(
      FilePath(FileAuth(h.env.root), source_dir),
      [pony_path] where limit = PassExpr)
    | let p: Program =>
      let file = ((p.module() as Module).file() as SourceFile)
      let holder = _child_named(file.ast, "Holder")?
      let pick = _child_named(holder(4)?, "pick")?
      let id = pick(2)?(0)?(0)?
      h.assert_true(
        (id.token_value() as String).at("$m$"),
        "expected a renamed type parameter")
      let start = id.position()
      match \exhaustive\ id.end_pos()
      | let e: Position =>
        h.assert_eq[USize](start.line(), e.line())
        // `Res` is three characters long.
        h.assert_eq[USize](start.column() + 2, e.column())
      | None =>
        h.fail("no end position")
      end
    | let e: Array[Error] val =>
      h.fail("compilation failed")
    end

  fun _child_named(parent: AST box, name': String): AST box ? =>
    for child in parent.children() do
      try
        let id = child(if child.id() == TokenIds.tk_fun() then 1 else 0 end)?
        if (id.token_value() as String) == name' then return child end
      end
    end
    error
