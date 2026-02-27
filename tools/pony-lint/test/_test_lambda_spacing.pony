use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestLambdaSpacingSingleLineClean is UnitTest
  """Single-line lambda with space before `}` is clean."""
  fun name(): String => "LambdaSpacing: single-line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): USize =>\n" +
      "    let f = {(a: USize): USize => a + 1 }\n" +
      "    f(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
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

class \nodoc\ _TestLambdaSpacingSingleLineMissingSpaceBefore is UnitTest
  """Single-line lambda without space before `}` is flagged."""
  fun name(): String =>
    "LambdaSpacing: single-line missing space before '}'"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): USize =>\n" +
      "    let f = {(a: USize): USize => a + 1}\n" +
      "    f(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("space required"))
            h.assert_true(
              diags(0)?.message.contains("'}'"))
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

class \nodoc\ _TestLambdaSpacingSpaceAfterOpen is UnitTest
  """Space after `{` in lambda is flagged."""
  fun name(): String => "LambdaSpacing: space after '{' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): USize =>\n" +
      "    let f = { (a: USize): USize => a + 1 }\n" +
      "    f(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("no space after"))
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

class \nodoc\ _TestLambdaSpacingMultiLineClean is UnitTest
  """Multi-line lambda with `}` on its own line is clean."""
  fun name(): String => "LambdaSpacing: multi-line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): USize =>\n" +
      "    let f = {(a: USize): USize =>\n" +
      "      a + 1\n" +
      "    }\n" +
      "    f(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
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

class \nodoc\ _TestLambdaSpacingMultiLineSpaceBeforeClose is UnitTest
  """Multi-line lambda with space before `}` on content line is flagged."""
  fun name(): String =>
    "LambdaSpacing: multi-line space before '}' flagged"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): USize =>\n" +
      "    let f = {(a: USize): USize =>\n" +
      "      a + 1 }\n" +
      "    f(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("no space before"))
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

class \nodoc\ _TestLambdaSpacingTypeClean is UnitTest
  """Lambda type with no extra spaces is clean."""
  fun name(): String => "LambdaSpacing: lambda type clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Fn is {(USize): USize}\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
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

class \nodoc\ _TestLambdaSpacingTypeSpaceAfterOpen is UnitTest
  """Lambda type with space after `{` is flagged."""
  fun name(): String => "LambdaSpacing: lambda type space after '{'"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Fn is { (USize): USize}\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("no space after"))
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

class \nodoc\ _TestLambdaSpacingTypeSpaceBeforeClose is UnitTest
  """Lambda type with space before `}` is flagged."""
  fun name(): String => "LambdaSpacing: lambda type space before '}'"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Fn is {(USize): USize }\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("no space before"))
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

class \nodoc\ _TestLambdaSpacingBareLambdaClean is UnitTest
  """Bare lambda `@{` with correct spacing is clean."""
  fun name(): String => "LambdaSpacing: bare lambda clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): USize =>\n" +
      "    let f = @{(a: USize): USize => a + 1 }\n" +
      "    f(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
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

class \nodoc\ _TestLambdaSpacingBareLambdaSpaceAfterOpen is UnitTest
  """Bare lambda `@{` with space after `{` is flagged."""
  fun name(): String => "LambdaSpacing: bare lambda space after '{'"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): USize =>\n" +
      "    let f = @{ (a: USize): USize => a + 1 }\n" +
      "    f(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
          h.assert_true(diags.size() > 0)
          try
            h.assert_true(
              diags(0)?.message.contains("no space after"))
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

class \nodoc\ _TestLambdaSpacingBareLambdaTypeClean is UnitTest
  """Bare lambda type `@{(USize): USize}` with no extra spaces is clean."""
  fun name(): String => "LambdaSpacing: bare lambda type clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Fn is @{(USize): USize}\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.LambdaSpacing)
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
