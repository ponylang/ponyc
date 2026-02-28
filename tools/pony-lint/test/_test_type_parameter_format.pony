use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestTypeParamFormatSingleLine is UnitTest
  """Single-line type params produce no diagnostics."""
  fun name(): String => "TypeParameterFormat: single-line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo[A, B]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeParameterFormat)
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

class \nodoc\ _TestTypeParamFormatMultiLineClassClean is UnitTest
  """Multiline type params each on own line (class) produces no diagnostics."""
  fun name(): String => "TypeParameterFormat: multiline class clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo[\n" +
      "  A,\n" +
      "  B]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeParameterFormat)
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

class \nodoc\ _TestTypeParamFormatTraitIsClean is UnitTest
  """Trait with multiline type params and properly aligned 'is'."""
  fun name(): String => "TypeParameterFormat: trait with 'is' clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "interface Hashable\n" +
      "trait Foo[\n" +
      "  A,\n" +
      "  B]\n" +
      "  is Hashable\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeParameterFormat)
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

class \nodoc\ _TestTypeParamFormatMethodClean is UnitTest
  """Method with multiline type params produces no diagnostics."""
  fun name(): String => "TypeParameterFormat: method type params clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun bar[\n" +
      "    A,\n" +
      "    B](x: A)\n" +
      "  =>\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeParameterFormat)
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

class \nodoc\ _TestTypeParamFormatBracketWrongLine is UnitTest
  """'[' on different line than name produces diagnostic."""
  fun name(): String => "TypeParameterFormat: '[' wrong line flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  [A, B]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeParameterFormat)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for '[' on wrong line")
          try
            h.assert_true(
              diags(0)?.message.contains(
                "'[' should be on the same line"),
              "message should mention '[' same line")
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

class \nodoc\ _TestTypeParamFormatSharingLine is UnitTest
  """Two type params sharing a line in multiline declaration."""
  fun name(): String => "TypeParamFormat: params sharing line"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo[\n" +
      "  A, B,\n" +
      "  C]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeParameterFormat)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for type params sharing a line")
          try
            h.assert_true(
              diags(0)?.message.contains(
                "each type parameter should be on its own line"),
              "message should mention type param on own line")
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

class \nodoc\ _TestTypeParamFormatIsMisaligned is UnitTest
  """'is' keyword at wrong column produces diagnostic."""
  fun name(): String => "TypeParameterFormat: 'is' misaligned flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "interface Hashable\n" +
      "class Foo[\n" +
      "  A,\n" +
      "  B]\n" +
      "    is Hashable\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeParameterFormat)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for 'is' misalignment")
          try
            h.assert_true(
              diags(0)?.message.contains(
                "'is' should align at column"),
              "message should mention 'is' alignment")
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

class \nodoc\ _TestTypeParamFormatMultipleViolations is UnitTest
  """Sharing line and misaligned 'is' produce two diagnostics."""
  fun name(): String => "TypeParamFormat: multiple violations"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "interface Hashable\n" +
      "class Foo[\n" +
      "  A, B,\n" +
      "  C]\n" +
      "    is Hashable\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeParameterFormat)
          h.assert_eq[USize](
            2,
            diags.size(),
            "expected 2 diagnostics for sharing + misaligned 'is'")
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end
