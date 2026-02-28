use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestArrayLiteralFormatSingleLine is UnitTest
  """Single-line array literal produces no diagnostics."""
  fun name(): String => "ArrayLiteralFormat: single-line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    let ns = [as USize: 1; 2; 3]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.ArrayLiteralFormat)
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

class \nodoc\ _TestArrayLiteralFormatMultiLineClean is UnitTest
  """
  Correctly formatted multiline array with `]` inline produces no diagnostics.
  """
  fun name(): String => "ArrayLiteralFormat: multiline clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    let ns =\n" +
      "      [ as USize:\n" +
      "        1\n" +
      "        2 ]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.ArrayLiteralFormat)
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

class \nodoc\ _TestArrayLiteralFormatMultiLineCloseOwnLine is UnitTest
  """Multiline array with `]` on its own line produces no diagnostics."""
  fun name(): String => "ArrayLiteralFormat: multiline close on own line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    let ns =\n" +
      "      [ as USize:\n" +
      "        1\n" +
      "        2\n" +
      "      ]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.ArrayLiteralFormat)
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

class \nodoc\ _TestArrayLiteralFormatEmpty is UnitTest
  """Empty array produces no diagnostics."""
  fun name(): String => "ArrayLiteralFormat: empty array clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    let ns = Array[USize]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.ArrayLiteralFormat)
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

class \nodoc\ _TestArrayLiteralFormatHangingIndent is UnitTest
  """Opening `[` not first on line produces a diagnostic."""
  fun name(): String => "ArrayLiteralFormat: hanging indent flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    let ns = [as USize:\n" +
      "      1\n" +
      "      2\n" +
      "    ]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.ArrayLiteralFormat)
          h.assert_eq[USize](1, diags.size(),
            "expected 1 diagnostic for hanging indent")
          try
            h.assert_true(
              diags(0)?.message.contains("first non-whitespace"),
              "message should mention first non-whitespace")
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

class \nodoc\ _TestArrayLiteralFormatNoSpaceAfterOpen is UnitTest
  """Missing space after `[` produces a diagnostic."""
  fun name(): String => "ArrayLiteralFormat: no space after '[' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    let ns =\n" +
      "      [as USize:\n" +
      "        1\n" +
      "        2\n" +
      "      ]\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.ArrayLiteralFormat)
          h.assert_eq[USize](1, diags.size(),
            "expected 1 diagnostic for missing space after '['")
          try
            h.assert_true(
              diags(0)?.message.contains("space required after '['"),
              "message should mention space after '['")
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
