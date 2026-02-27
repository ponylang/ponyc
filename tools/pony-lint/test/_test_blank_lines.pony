use "pony_test"
use ast = "pony_compiler"
use lint = ".."

// --- Within-entity tests ---

class \nodoc\ _TestBlankLinesNoBlankBeforeFirstField is UnitTest
  """Entity with no blank line before first field is clean."""
  fun name(): String => "BlankLines: no blank before first field is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  let x: U32 = 0\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesBlankBeforeFirstField is UnitTest
  """Entity with blank line before first field is flagged."""
  fun name(): String => "BlankLines: blank before first field flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "\n" +
      "  let x: U32 = 0\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("first body content"))
          else
            h.fail("could not access diagnostic")
          end
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesDocstringNoBlank is UnitTest
  """Entity with docstring and no blank line before first field is clean."""
  fun name(): String =>
    "BlankLines: docstring then field without blank is clean"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  let x: U32 = 0\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesMultiLineHeaderClean is UnitTest
  """Multi-line entity header with provides, no blank before first member."""
  fun name(): String => "BlankLines: multi-line header is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "interface Greetable\n" +
      "  fun greet(): String\n" +
      "\n" +
      "class Foo is\n" +
      "  Greetable\n" +
      "  fun greet(): String => \"hi\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesConsecutiveFieldsClean is UnitTest
  """Consecutive fields with no blank lines is clean."""
  fun name(): String => "BlankLines: consecutive fields no blank is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  let x: U32 = 0\n" +
      "  let y: U32 = 1\n" +
      "  var z: U32 = 2\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesConsecutiveFieldsBlank is UnitTest
  """Consecutive fields with blank line is flagged."""
  fun name(): String => "BlankLines: blank between fields flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  let x: U32 = 0\n" +
      "\n" +
      "  let y: U32 = 1\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("between fields"))
          else
            h.fail("could not access diagnostic")
          end
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesFieldToMethodOneBlank is UnitTest
  """Field to method with 1 blank line is clean."""
  fun name(): String => "BlankLines: field->method 1 blank is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  let x: U32 = 0\n" +
      "\n" +
      "  fun apply(): U32 => x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesFieldToMethodNoBlank is UnitTest
  """Field to method with 0 blank lines is flagged."""
  fun name(): String => "BlankLines: field->method 0 blanks flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  let x: U32 = 0\n" +
      "  fun apply(): U32 => x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("1 blank line"))
          else
            h.fail("could not access diagnostic")
          end
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesMultiLineMethodsOneBlank is UnitTest
  """Consecutive multi-line methods with 1 blank line is clean."""
  fun name(): String => "BlankLines: methods with 1 blank is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun foo(): U32 =>\n" +
      "    U32(1)\n" +
      "\n" +
      "  fun bar(): U32 =>\n" +
      "    U32(2)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesMultiLineMethodsNoBlank is UnitTest
  """Consecutive multi-line methods with 0 blank lines is flagged."""
  fun name(): String => "BlankLines: methods with 0 blanks flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun foo(): U32 =>\n" +
      "    U32(1)\n" +
      "  fun bar(): U32 =>\n" +
      "    U32(2)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("1 blank line"))
          else
            h.fail("could not access diagnostic")
          end
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestBlankLinesOneLineMethodsNoBlank is UnitTest
  """Consecutive one-line methods with 0 blank lines (exception) is clean."""
  fun name(): String => "BlankLines: one-liner methods 0 blanks is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun foo(): U32 => U32(1)\n" +
      "  fun bar(): U32 => U32(2)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.BlankLines)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

// --- Between-entity tests ---
class \nodoc\ _TestBlankLinesBetweenEntitiesOneBlank is UnitTest
  """Two entities with 1 blank line between them is clean."""
  fun name(): String => "BlankLines: 1 blank between entities is clean"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile(
      "test.pony",
      "class Foo\n" +
      "  fun apply(): None => None\n" +
      "\n" +
      "class Bar\n" +
      "  fun apply(): None => None\n",
      ".")
    let entities = recover val
      Array[(String val, ast.TokenId, USize, USize)]
        .> push(("Foo", ast.TokenIds.tk_class(), 1, 2))
        .> push(("Bar", ast.TokenIds.tk_class(), 4, 5))
    end
    let diags = lint.BlankLines.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestBlankLinesBetweenEntitiesNoBlank is UnitTest
  """Two multi-line entities with 0 blank lines is flagged."""
  fun name(): String => "BlankLines: 0 blanks between entities flagged"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile(
      "test.pony",
      "class Foo\n" +
      "  fun apply(): None => None\n" +
      "class Bar\n" +
      "  fun apply(): None => None\n",
      ".")
    let entities = recover val
      Array[(String val, ast.TokenId, USize, USize)]
        .> push(("Foo", ast.TokenIds.tk_class(), 1, 2))
        .> push(("Bar", ast.TokenIds.tk_class(), 3, 4))
    end
    let diags = lint.BlankLines.check_module(entities, sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_true(diags(0)?.message.contains("between entities"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestBlankLinesBetweenEntitiesTooMany is UnitTest
  """Two entities with 2+ blank lines is flagged."""
  fun name(): String => "BlankLines: 2+ blanks between entities flagged"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile(
      "test.pony",
      "class Foo\n" +
      "  fun apply(): None => None\n" +
      "\n" +
      "\n" +
      "class Bar\n" +
      "  fun apply(): None => None\n",
      ".")
    let entities = recover val
      Array[(String val, ast.TokenId, USize, USize)]
        .> push(("Foo", ast.TokenIds.tk_class(), 1, 2))
        .> push(("Bar", ast.TokenIds.tk_class(), 5, 6))
    end
    let diags = lint.BlankLines.check_module(entities, sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestBlankLinesOneLinerEntitiesNoBlank is UnitTest
  """Two one-line primitives with 0 blank lines (exception) is clean."""
  fun name(): String => "BlankLines: one-liner entities 0 blanks is clean"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile(
      "test.pony",
      "primitive Red\n" +
      "primitive Green\n" +
      "primitive Blue\n",
      ".")
    let entities = recover val
      Array[(String val, ast.TokenId, USize, USize)]
        .> push(("Red", ast.TokenIds.tk_primitive(), 1, 1))
        .> push(("Green", ast.TokenIds.tk_primitive(), 2, 2))
        .> push(("Blue", ast.TokenIds.tk_primitive(), 3, 3))
    end
    let diags = lint.BlankLines.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestMaxLineVisitor is ast.ASTVisitor
  """Test copy of _MaxLineVisitor (package-private in pony-lint)."""
  var max_line: USize

  new ref create(seed: USize) => max_line = seed

  fun ref visit(node: ast.AST box): ast.VisitResult =>
    if (node.id() == ast.TokenIds.tk_members())
      and (node.num_children() == 0)
    then
      return ast.Continue
    end
    let l = node.line()
    if l > max_line then max_line = l end
    ast.Continue

class \nodoc\ _TestEntityCollector is ast.ASTVisitor
  """Test copy of entity collection logic from _ASTDispatcher."""
  let _entities: Array[(String val, ast.TokenId, USize, USize)]

  new ref create() =>
    _entities = Array[(String val, ast.TokenId, USize, USize)]

  fun ref visit(node: ast.AST box): ast.VisitResult =>
    let token_id = node.id()
    if ast.TokenIds.is_entity(token_id)
      or (token_id == ast.TokenIds.tk_type())
    then
      try
        let name_node = node(0)?
        match name_node.token_value()
        | let name: String val =>
          let start_line = node.line()
          let mlv = _TestMaxLineVisitor(start_line)
          node.visit(mlv)
          _entities.push((name, token_id, start_line, mlv.max_line))
        end
      end
    end
    ast.Continue

  fun entities(): Array[(String val, ast.TokenId, USize, USize)] val =>
    let result =
      recover iso Array[(String val, ast.TokenId, USize, USize)] end
    for e in _entities.values() do result.push(e) end
    consume result

class \nodoc\ _TestBlankLinesBetweenDocstringEntities is UnitTest
  """Two docstring-only primitives with 1 blank line — full AST pipeline."""
  fun name(): String =>
    "BlankLines: docstring-only entities 1 blank (AST pipeline)"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "primitive Foo\n" +
      "  \"\"\"Foo docs.\"\"\"\n" +
      "\n" +
      "primitive Bar\n" +
      "  \"\"\"Bar docs.\"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let collector = _TestEntityCollector
          mod.ast.visit(collector)
          let entities = collector.entities()
          let diags = lint.BlankLines.check_module(entities, sf)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end
