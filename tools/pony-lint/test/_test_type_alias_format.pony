use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestTypeAliasFormatSingleLineUnion is UnitTest
  """Single-line union type alias produces no diagnostics."""
  fun name(): String => "TypeAliasFormat: single-line union clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Signed is (I8 | I16 | I32)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
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

class \nodoc\ _TestTypeAliasFormatMultiLineClean is UnitTest
  """Correctly formatted multiline union produces no diagnostics."""
  fun name(): String => "TypeAliasFormat: multiline union clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Signed is\n" +
      "  ( I8\n" +
      "  | I16\n" +
      "  | I32 )\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
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

class \nodoc\ _TestTypeAliasFormatMultiLineCloseOwnLine is UnitTest
  """Multiline union with `)` on its own line produces no diagnostics."""
  fun name(): String => "TypeAliasFormat: multiline close on own line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Signed is\n" +
      "  ( I8\n" +
      "  | I16\n" +
      "  | I32\n" +
      "  )\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
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

class \nodoc\ _TestTypeAliasFormatSimpleAlias is UnitTest
  """Simple type alias (no union) produces no diagnostics."""
  fun name(): String => "TypeAliasFormat: simple alias clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Foo is String\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
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

class \nodoc\ _TestTypeAliasFormatMultiLineIsectClean is UnitTest
  """Correctly formatted multiline intersection produces no diagnostics."""
  fun name(): String => "TypeAliasFormat: multiline intersection clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "interface Hashable\n" +
      "interface Stringable\n" +
      "type Both is\n" +
      "  ( Hashable\n" +
      "  & Stringable )\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
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

class \nodoc\ _TestTypeAliasFormatTypeParamsClean is UnitTest
  """Type alias with type parameters (single-line) produces no diagnostics."""
  fun name(): String => "TypeAliasFormat: type params single-line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Bar[A]\n" +
      "class Baz[A]\n" +
      "type Foo[A] is (Bar[A] | Baz[A])\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
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

class \nodoc\ _TestTypeAliasFormatHangingIndent is UnitTest
  """Opening `(` on the `type ... is` line produces a diagnostic."""
  fun name(): String => "TypeAliasFormat: hanging indent flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Signed is (I8\n" +
      "  | I16\n" +
      "  | I32 )\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
          h.assert_eq[USize](1, diags.size(),
            "expected 1 diagnostic for hanging indent")
          try
            h.assert_true(
              diags(0)?.message.contains("must not be on the"),
              "message should mention hanging indent")
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

class \nodoc\ _TestTypeAliasFormatNoSpaceAfterOpen is UnitTest
  """Missing space after `(` produces a diagnostic."""
  fun name(): String => "TypeAliasFormat: no space after '(' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Signed is\n" +
      "  (I8\n" +
      "  | I16\n" +
      "  | I32 )\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
          h.assert_eq[USize](1, diags.size(),
            "expected 1 diagnostic for missing space after '('")
          try
            h.assert_true(
              diags(0)?.message.contains("space required after '('"),
              "message should mention space after '('")
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

class \nodoc\ _TestTypeAliasFormatNoSpaceAfterPipe is UnitTest
  """Missing space after `|` produces diagnostics."""
  fun name(): String => "TypeAliasFormat: no space after '|' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Signed is\n" +
      "  ( I8\n" +
      "  |I16\n" +
      "  | I32 )\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
          h.assert_eq[USize](1, diags.size(),
            "expected 1 diagnostic for missing space after '|'")
          try
            h.assert_true(
              diags(0)?.message.contains("space required after '|'"),
              "message should mention space after '|'")
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

class \nodoc\ _TestTypeAliasFormatNoSpaceBeforeClose is UnitTest
  """Missing space before `)` produces a diagnostic."""
  fun name(): String => "TypeAliasFormat: no space before ')' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Signed is\n" +
      "  ( I8\n" +
      "  | I16\n" +
      "  | I32)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.TypeAliasFormat)
          h.assert_eq[USize](1, diags.size(),
            "expected 1 diagnostic for missing space before ')'")
          try
            h.assert_true(
              diags(0)?.message.contains("space required before ')'"),
              "message should mention space before ')'")
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
