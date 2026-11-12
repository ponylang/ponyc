use "files"
use "pony_test"
use "../pony_compiler"

use @source_open_string[USize](text: Pointer[U8] tag)
use @source_close[None](source: USize)
use @ast_source[USize](ast: USize)
use @ast_setpos[None](ast: USize, source: USize, line: USize, pos: USize)

class \nodoc\ iso _ASTSourceFile is UnitTest
  fun name(): String => "ast/source_file"

  fun apply(h: TestHelper) ? =>
    let dir = Path.join(Path.dir(__loc.file()), "simple")
    let session =
      CompileSession(
        FilePath(FileAuth(h.env.root), dir),
        PonyPath(h.env) as String where limit = PassParse)
    try
      let program = session.program() as Program
      let module = program.module() as Module
      let file = module.file() as SourceFile
      h.assert_eq[String](
        Path.join(dir, "main.pony"),
        file.ast.source_file() as String)
      h.assert_true(program.ast.source_file() is None)

      // USize carries opaque C pointers without naming private wrapper types.
      let original_source = @ast_source(file.ast.raw.usize())
      let original_position = file.ast.position()
      let source = @source_open_string("actor Main".cstring())
      h.assert_ne[USize](0, source)
      @ast_setpos(file.ast.raw.usize(), source, USize(1), USize(1))
      h.assert_false(file.ast.source().is_none())
      h.assert_true(file.ast.source_file() is None)
      @ast_setpos(
        file.ast.raw.usize(),
        original_source,
        original_position.line(),
        original_position.column())
      h.assert_eq[USize](original_source, @ast_source(file.ast.raw.usize()))
      @source_close(source)
    else
      session.dispose()
      error
    end
    session.dispose()
