use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestDotSpacingClean is UnitTest
  """Normal dot access with no spaces produces no diagnostics."""
  fun name(): String => "DotSpacing: foo.bar() is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  var x: U32 = 0\n" +
      "\n" +
      "class Bar\n" +
      "  fun apply(): U32 =>\n" +
      "    let f = Foo\n" +
      "    f.x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DotSpacing)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestDotSpacingSpaceBeforeViolation is UnitTest
  """Space before dot produces a diagnostic."""
  fun name(): String => "DotSpacing: 'foo .bar()' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  var x: U32 = 0\n" +
      "\n" +
      "class Bar\n" +
      "  fun apply(): U32 =>\n" +
      "    let f = Foo\n" +
      "    f .x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DotSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_eq[String](
              "style/dot-spacing", diags(0)?.rule_id)
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestDotSpacingChainNoSpaceViolation is UnitTest
  """Chain operator .> without spaces produces a diagnostic."""
  fun name(): String => "DotSpacing: 'foo.>bar()' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  var x: U32 = 0\n" +
      "  fun ref set_x(v: U32): Foo =>\n" +
      "    x = v\n" +
      "    this\n" +
      "\n" +
      "class Bar\n" +
      "  fun apply(): None =>\n" +
      "    Foo.>set_x(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DotSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_eq[String](
              "style/dot-spacing", diags(0)?.rule_id)
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestDotSpacingChainSpacedClean is UnitTest
  """Chain operator .> with spaces produces no diagnostics."""
  fun name(): String => "DotSpacing: 'foo .> bar()' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  var x: U32 = 0\n" +
      "  fun ref set_x(v: U32): Foo =>\n" +
      "    x = v\n" +
      "    this\n" +
      "\n" +
      "class Bar\n" +
      "  fun apply(): None =>\n" +
      "    Foo .> set_x(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DotSpacing)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestDotSpacingContinuationLineClean is UnitTest
  """Dot at start of continuation line is clean (no before check)."""
  fun name(): String => "DotSpacing: continuation line dot is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun bar(): Foo => Foo\n" +
      "\n" +
      "class Bar\n" +
      "  fun apply(): Foo =>\n" +
      "    Foo\n" +
      "      .bar()\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DotSpacing)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestDotSpacingChainContinuationClean is UnitTest
  """.> at start of continuation line is clean (no before check)."""
  fun name(): String => "DotSpacing: continuation line .> is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  var x: U32 = 0\n" +
      "  fun ref set_x(v: U32): Foo =>\n" +
      "    x = v\n" +
      "    this\n" +
      "\n" +
      "class Bar\n" +
      "  fun apply(): None =>\n" +
      "    Foo\n" +
      "      .> set_x(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DotSpacing)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end
