use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestMatchSingleLineClean is UnitTest
  """Multi-line match produces no diagnostics."""
  fun name(): String => "MatchSingleLine: multi-line match is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): String =>\n" +
      "    match x\n" +
      "    | 1 => \"one\"\n" +
      "    | 2 => \"two\"\n" +
      "    else \"other\"\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MatchSingleLine)
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

class \nodoc\ _TestMatchSingleLineViolation is UnitTest
  """Single-line match produces 1 diagnostic."""
  fun name(): String => "MatchSingleLine: single-line match flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): String =>\n" +
      "    match x | 1 => \"one\" else \"other\" end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MatchSingleLine)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/match-no-single-line", diags(0)?.rule_id)
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

class \nodoc\ _TestMatchSingleLineMultiLineBody is UnitTest
  """Match with multi-line case body is clean."""
  fun name(): String => "MatchSingleLine: multi-line case body is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): String =>\n" +
      "    match x\n" +
      "    | 1 =>\n" +
      "      \"one\"\n" +
      "    else\n" +
      "      \"other\"\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MatchSingleLine)
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

class \nodoc\ _TestMatchSingleLineElseSeparateLine is UnitTest
  """Match with else on separate line is clean."""
  fun name(): String => "MatchSingleLine: else on separate line is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): String =>\n" +
      "    match x\n" +
      "    | 1 => \"one\"\n" +
      "    else\n" +
      "      \"other\"\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MatchSingleLine)
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
