use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestExhaustiveMatchFlagged is UnitTest
  """Exhaustive match without \exhaustive\ annotation is flagged."""
  fun name(): String => "ExhaustiveMatch: missing annotation flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Color is (Red | Green | Blue)\n" +
      "primitive Red\n" +
      "primitive Green\n" +
      "primitive Blue\n" +
      "\n" +
      "primitive Foo\n" +
      "  fun apply(c: Color): String =>\n" +
      "    match c\n" +
      "    | Red => \"red\"\n" +
      "    | Green => \"green\"\n" +
      "    | Blue => \"blue\"\n" +
      "    end\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source where pass = ast.PassExpr)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.ExhaustiveMatch)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "safety/exhaustive-match", diags(0)?.rule_id)
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

class \nodoc\ _TestExhaustiveMatchAnnotated is UnitTest
  """Exhaustive match with \exhaustive\ annotation is clean."""
  fun name(): String => "ExhaustiveMatch: annotated match is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Color is (Red | Green | Blue)\n" +
      "primitive Red\n" +
      "primitive Green\n" +
      "primitive Blue\n" +
      "\n" +
      "primitive Foo\n" +
      "  fun apply(c: Color): String =>\n" +
      "    match \\exhaustive\\ c\n" +
      "    | Red => \"red\"\n" +
      "    | Green => \"green\"\n" +
      "    | Blue => \"blue\"\n" +
      "    end\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source where pass = ast.PassExpr)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.ExhaustiveMatch)
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

class \nodoc\ _TestExhaustiveMatchNonExhaustive is UnitTest
  """Non-exhaustive match without annotation is not flagged."""
  fun name(): String => "ExhaustiveMatch: non-exhaustive match clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "type Color is (Red | Green | Blue)\n" +
      "primitive Red\n" +
      "primitive Green\n" +
      "primitive Blue\n" +
      "\n" +
      "primitive Foo\n" +
      "  fun apply(c: Color): (String | None) =>\n" +
      "    match c\n" +
      "    | Red => \"red\"\n" +
      "    | Green => \"green\"\n" +
      "    end\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source where pass = ast.PassExpr)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.ExhaustiveMatch)
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

class \nodoc\ _TestExhaustiveMatchExplicitElse is UnitTest
  """Match with explicit else clause is not flagged."""
  fun name(): String => "ExhaustiveMatch: explicit else clause clean"
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
      (let program, let sf) =
        _ASTTestHelper.compile(h, source where pass = ast.PassExpr)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.ExhaustiveMatch)
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

class \nodoc\ _TestExhaustiveMatchMetadata is UnitTest
  """Rule metadata is correct for suppression and registry integration."""
  fun name(): String => "ExhaustiveMatch: metadata"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("safety/exhaustive-match", lint.ExhaustiveMatch.id())
    h.assert_eq[String]("safety", lint.ExhaustiveMatch.category())
    match lint.ExhaustiveMatch.required_pass()
    | ast.PassExpr => None
    else
      h.fail("required_pass should be PassExpr")
    end
    match lint.ExhaustiveMatch.default_status()
    | lint.RuleOn => None
    else
      h.fail("default_status should be RuleOn")
    end
