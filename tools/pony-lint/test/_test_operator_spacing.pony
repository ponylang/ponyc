use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestOperatorSpacingCleanBinary is UnitTest
  """Binary operator with spaces produces no diagnostics."""
  fun name(): String => "OperatorSpacing: 'x + y' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): U32 =>\n" +
      "    let x: U32 = 3\n" +
      "    x + 1\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingNoSpaces is UnitTest
  """Binary operator with no spaces produces a diagnostic."""
  fun name(): String => "OperatorSpacing: 'x+y' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "primitive Foo\n" +
      "  fun apply(x: U32, y: U32): U32 =>\n" +
      "    x+y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_eq[String](
              "style/operator-spacing", diags(0)?.rule_id)
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

class \nodoc\ _TestOperatorSpacingSpaceOnlyBefore is UnitTest
  """Space only before operator flags missing space after."""
  fun name(): String => "OperatorSpacing: 'x +y' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "primitive Foo\n" +
      "  fun apply(x: U32, y: U32): U32 =>\n" +
      "    x +y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(diags(0)?.message.contains("after"))
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

class \nodoc\ _TestOperatorSpacingSpaceOnlyAfter is UnitTest
  """Space only after operator flags missing space before."""
  fun name(): String => "OperatorSpacing: 'x+ y' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "primitive Foo\n" +
      "  fun apply(x: U32, y: U32): U32 =>\n" +
      "    x+ y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(diags(0)?.message.contains("before"))
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

class \nodoc\ _TestOperatorSpacingUnaryMinusClean is UnitTest
  """Unary minus with no space after is clean."""
  fun name(): String => "OperatorSpacing: '-a' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): I32 =>\n" +
      "    let a: I32 = 5\n" +
      "    -a\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingUnaryMinusSpaceViolation is UnitTest
  """Unary minus with space after is flagged."""
  fun name(): String => "OperatorSpacing: '- a' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): I32 =>\n" +
      "    let a: I32 = 5\n" +
      "    - a\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("unary"))
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

class \nodoc\ _TestOperatorSpacingNotClean is UnitTest
  """`not` with proper spacing produces no diagnostics."""
  fun name(): String => "OperatorSpacing: 'not x' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): Bool =>\n" +
      "    let x: Bool = true\n" +
      "    not x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingNotAfterParenClean is UnitTest
  """`not` preceded by `(` is clean (non-alphanumeric before is OK)."""
  fun name(): String => "OperatorSpacing: '(not x)' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): Bool =>\n" +
      "    let x: Bool = true\n" +
      "    (not x) and true\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingNotNoSpaceAfter is UnitTest
  """`not` without space after is flagged."""
  fun name(): String => "OperatorSpacing: 'not(x)' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    // Using `not true` form since `not(x)` needs to compile;
    // the lexer sees `not` as keyword followed by `(` which is valid syntax.
    // But `not` still needs space after per style guide.
    let source: String val =
      "class Foo\n" +
      "  fun apply(): Bool =>\n" +
      "    not(true and false)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("after"))
            h.assert_true(
              diags(0)?.message.contains("not"))
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

class \nodoc\ _TestOperatorSpacingAssignClean is UnitTest
  """Assignment with spaces is clean."""
  fun name(): String => "OperatorSpacing: 'x = 3' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  var x: U32 = 0\n" +
      "  fun ref apply(): None =>\n" +
      "    x = 3\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingAssignViolation is UnitTest
  """Assignment without spaces is flagged."""
  fun name(): String => "OperatorSpacing: 'x=3' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  var x: U32 = 0\n" +
      "  fun ref apply(): None =>\n" +
      "    x=3\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_eq[String](
              "style/operator-spacing", diags(0)?.rule_id)
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

class \nodoc\ _TestOperatorSpacingMultipleViolations is UnitTest
  """Multiple operators without spaces produce multiple diagnostics."""
  fun name(): String => "OperatorSpacing: 'x+y*z' produces two diagnostics"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "primitive Foo\n" +
      "  fun apply(x: U32, y: U32, z: U32): U32 =>\n" +
      "    x+y*z\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
          h.assert_true(diags.size() >= 2)
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestOperatorSpacingKeywordBinaryClean is UnitTest
  """Keyword binary operator with spaces is clean."""
  fun name(): String => "OperatorSpacing: 'x and y' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): Bool =>\n" +
      "    true and false\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingIdentityClean is UnitTest
  """Identity operator `is` with spaces is clean."""
  fun name(): String => "OperatorSpacing: 'x is None' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): Bool =>\n" +
      "    let x: (U32 | None) = None\n" +
      "    x is None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingSaturatingClean is UnitTest
  """Saturating operator with spaces is clean."""
  fun name(): String => "OperatorSpacing: 'x +~ y' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "primitive Foo\n" +
      "  fun apply(x: U32, y: U32): U32 =>\n" +
      "    x +~ y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingStyleGuideExample is UnitTest
  """Style guide example: `if not x then -a end` is clean."""
  fun name(): String =>
    "OperatorSpacing: 'if not x then -a end' is clean"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): I32 =>\n" +
      "    let x: Bool = false\n" +
      "    let a: I32 = 5\n" +
      "    if not x then -a else a end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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

class \nodoc\ _TestOperatorSpacingContinuationLineClean is UnitTest
  """Operator at start of continuation line is clean (before-check exempt)."""
  fun name(): String =>
    "OperatorSpacing: continuation line is clean"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "primitive Foo\n" +
      "  fun apply(x: U32, y: U32): U32 =>\n" +
      "    x\n" +
      "      + y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.OperatorSpacing)
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
