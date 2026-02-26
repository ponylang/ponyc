use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestPublicDocstringTypeClean is UnitTest
  """Public class with docstring produces no diagnostics."""
  fun name(): String => "PublicDocstring: documented type is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringTypeViolation is UnitTest
  """Public class without docstring produces 1 diagnostic."""
  fun name(): String => "PublicDocstring: undocumented type flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source = "class Foo\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/public-docstring", diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("Foo"))
            h.assert_true(diags(0)?.message.contains("type"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringMethodClean is UnitTest
  """Public method with docstring produces no diagnostics."""
  fun name(): String => "PublicDocstring: documented method is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  fun apply(): None =>\n" +
      "    \"\"\"Method docstring.\"\"\"\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringMethodViolation is UnitTest
  """Public method without docstring (4 expressions, non-exempt) is flagged."""
  fun name(): String => "PublicDocstring: undocumented method flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  fun apply(): None =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    let _d = U32(4)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/public-docstring", diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("apply"))
            h.assert_true(diags(0)?.message.contains("method"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringPrivateSkipped is UnitTest
  """Private type and private method without docstrings produce 0."""
  fun name(): String => "PublicDocstring: private names skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class _Foo\n" +
      "  fun _bar(): None => None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringExemptMethods is UnitTest
  """All commonly-known method names without docstrings produce 0."""
  fun name(): String => "PublicDocstring: exempt method names skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    // Each exempt method has 4 top-level expressions so gate 2 (simple
    // body ≤ 3) does NOT apply — only gate 1 (exempt name) saves them.
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  new create() =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    let _d = U32(4)\n" +
      "  fun string(): String =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    \"\"\n" +
      "  fun eq(other: Foo box): Bool =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    true\n" +
      "  fun ne(other: Foo box): Bool =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    true\n" +
      "  fun hash(): USize =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    USize(0)\n" +
      "  fun hash64(): U64 =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    U64(0)\n" +
      "  fun compare(other: Foo box): I32 =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    I32(0)\n" +
      "  fun lt(other: Foo box): Bool =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    true\n" +
      "  fun le(other: Foo box): Bool =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    true\n" +
      "  fun ge(other: Foo box): Bool =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    true\n" +
      "  fun gt(other: Foo box): Bool =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    true\n" +
      "  fun dispose() =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    let _d = U32(4)\n" +
      "  fun has_next(): Bool =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    true\n" +
      "  fun next(): None =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    None\n" +
      "  fun values(): None =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    None\n" +
      "  fun size(): USize =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    USize(0)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringAllEntityTypes is UnitTest
  """All 6 entity types without docstrings produce 6 diagnostics."""
  fun name(): String => "PublicDocstring: all 6 entity types flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "actor Bar\n" +
      "primitive Baz\n" +
      "struct Qux\n" +
      "trait Quux\n" +
      "interface Corge\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](6, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringBehavior is UnitTest
  """Public behavior without docstring (4+ expressions) is flagged."""
  fun name(): String => "PublicDocstring: undocumented behavior flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  be apply() =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    let _d = U32(4)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_true(diags(0)?.message.contains("apply"))
            h.assert_true(diags(0)?.message.contains("method"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringTypeAlias is UnitTest
  """Type alias without docstring produces 0 (not checked)."""
  fun name(): String => "PublicDocstring: type alias not checked"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "\n" +
      "type Bar is Foo\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringSimpleBody is UnitTest
  """Method with exactly 3 expressions and no docstring is exempt."""
  fun name(): String => "PublicDocstring: simple body (3 exprs) exempt"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  fun apply(): None =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringAbstractMethod is UnitTest
  """Abstract method in trait without docstring is flagged."""
  fun name(): String => "PublicDocstring: abstract method flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "trait Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  fun apply(): None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_true(diags(0)?.message.contains("apply"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringNonExemptConstructor is UnitTest
  """Non-`create` constructor without docstring (4+ expressions) is flagged."""
  fun name(): String =>
    "PublicDocstring: non-exempt constructor flagged"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  new from_string(s: String) =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    let _d = U32(4)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_true(
              diags(0)?.message.contains("from_string"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringNodocTypeSkipped is UnitTest
  """Type with \\nodoc\\ annotation and no docstring produces 0."""
  fun name(): String => "PublicDocstring: \\nodoc\\ type skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class \\nodoc\\ Foo\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringNodocMethodSkipped is UnitTest
  """Method with \\nodoc\\ annotation and no docstring produces 0."""
  fun name(): String => "PublicDocstring: \\nodoc\\ method skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  fun \\nodoc\\ apply(): None =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    let _d = U32(4)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringNodocEntityMethodsSkipped is UnitTest
  """Methods inside a \\nodoc\\ entity produce 0 even without docstrings."""
  fun name(): String =>
    "PublicDocstring: methods in \\nodoc\\ entity skipped"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class \\nodoc\\ Foo\n" +
      "  fun apply(): None =>\n" +
      "    let _a = U32(1)\n" +
      "    let _b = U32(2)\n" +
      "    let _c = U32(3)\n" +
      "    let _d = U32(4)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestPublicDocstringMainActorSkipped is UnitTest
  """Actor named Main without docstring produces 0."""
  fun name(): String => "PublicDocstring: Main actor skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) => None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PublicDocstring)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end
