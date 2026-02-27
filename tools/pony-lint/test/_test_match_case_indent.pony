use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestMatchCaseIndentClean is UnitTest
  """Cases aligned with match keyword produce no diagnostics."""
  fun name(): String => "MatchCaseIndent: aligned cases are clean"
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
          let diags = _CollectRuleDiags(mod, sf, lint.MatchCaseIndent)
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

class \nodoc\ _TestMatchCaseIndentViolation is UnitTest
  """Cases indented relative to match keyword produce diagnostics."""
  fun name(): String => "MatchCaseIndent: indented cases flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): String =>\n" +
      "    match x\n" +
      "      | 1 => \"one\"\n" +
      "      | 2 => \"two\"\n" +
      "    else \"other\"\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MatchCaseIndent)
          h.assert_eq[USize](2, diags.size())
          try
            h.assert_eq[String](
              "style/match-case-indent", diags(0)?.rule_id)
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

class \nodoc\ _TestMatchCaseIndentNestedClean is UnitTest
  """Match inside indented method body — alignment is relative to match."""
  fun name(): String =>
    "MatchCaseIndent: nested match with aligned cases is clean"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): String =>\n" +
      "    let _y = U32(0)\n" +
      "    match x\n" +
      "    | 1 => \"one\"\n" +
      "    else \"other\"\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MatchCaseIndent)
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
