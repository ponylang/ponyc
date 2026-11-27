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
      match program.module()
      | let pkg: ast.Module val =>
        match pkg.file()
        | let mod: ast.SourceFile val =>
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
        h.fail("no module")
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
      match program.module()
      | let pkg: ast.Module val =>
        match pkg.file()
        | let mod: ast.SourceFile val =>
          let diags = _CollectRuleDiags(mod, sf, lint.ExhaustiveMatch)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no module")
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
      match program.module()
      | let pkg: ast.Module val =>
        match pkg.file()
        | let mod: ast.SourceFile val =>
          let diags = _CollectRuleDiags(mod, sf, lint.ExhaustiveMatch)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no module")
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
      match program.module()
      | let pkg: ast.Module val =>
        match pkg.file()
        | let mod: ast.SourceFile val =>
          let diags = _CollectRuleDiags(mod, sf, lint.ExhaustiveMatch)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no module")
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

primitive \nodoc\ _ExhaustiveMatchDiags
  """
  Helper: compile `source` to the expr pass and collect the exhaustive-match
  diagnostics for its file, or fail the test.
  """
  fun apply(h: TestHelper, source: String val)
    : (Array[lint.Diagnostic val] val | None)
  =>
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source where pass = ast.PassExpr)?
      match program.module()
      | let pkg: ast.Module val =>
        match pkg.file()
        | let mod: ast.SourceFile val =>
          return _CollectRuleDiags(mod, sf, lint.ExhaustiveMatch)
        end
      end
      h.fail("no module")
    else
      h.fail("compilation failed")
    end
    None

class \nodoc\ _TestExhaustiveMatchInheritedCopy is UnitTest
  """
  A match in a trait's method that is exhaustive only in a class's copy of
  the method, where `v()` is the class's U8, is not reported: the copy is not
  the text the author wrote.
  """
  fun name(): String => "ExhaustiveMatch: inherited copy is not checked"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "trait T\n" +
      "  fun v(): (U8 | String)\n" +
      "\n" +
      "  fun f(): None =>\n" +
      "    match v()\n" +
      "    | let u: U8 => None\n" +
      "    end\n" +
      "\n" +
      "class C is T\n" +
      "  new create() => None\n" +
      "  fun v(): U8 => 1\n"
    match _ExhaustiveMatchDiags(h, source)
    | let diags: Array[lint.Diagnostic val] val =>
      h.assert_eq[USize](0, diags.size())
    end

class \nodoc\ _TestExhaustiveMatchInheritedLiteral is UnitTest
  """
  A match in an object literal in a trait's method, exhaustive only in the
  class a host's copy of the method makes from the literal, is not reported.
  """
  fun name(): String => "ExhaustiveMatch: literal in inherited copy"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "interface Runner\n" +
      "  fun apply(): None\n" +
      "\n" +
      "trait T\n" +
      "  fun v(): (U8 | String)\n" +
      "\n" +
      "  fun mk(): Runner val =>\n" +
      "    let x = v()\n" +
      "    object val is Runner\n" +
      "      fun apply(): None =>\n" +
      "        match x\n" +
      "        | let u: U8 => None\n" +
      "        end\n" +
      "    end\n" +
      "\n" +
      "class C is T\n" +
      "  new create() => None\n" +
      "  fun v(): U8 => 1\n"
    match _ExhaustiveMatchDiags(h, source)
    | let diags: Array[lint.Diagnostic val] val =>
      h.assert_eq[USize](0, diags.size())
    end

class \nodoc\ _TestExhaustiveMatchOwnMethodBesideTrait is UnitTest
  """
  A class's own exhaustive, unannotated match is still reported when the class
  also inherits a method from a trait in the same file.
  """
  fun name(): String => "ExhaustiveMatch: own method beside inherited copy"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "trait T\n" +
      "  fun v(): (U8 | String)\n" +
      "\n" +
      "  fun f(): None =>\n" +
      "    match v()\n" +
      "    | let u: U8 => None\n" +
      "    end\n" +
      "\n" +
      "class C is T\n" +
      "  new create() => None\n" +
      "  fun v(): U8 => 1\n" +
      "  fun g(): None =>\n" +
      "    match v()\n" +
      "    | let u: U8 => None\n" +
      "    end\n"
    match _ExhaustiveMatchDiags(h, source)
    | let diags: Array[lint.Diagnostic val] val =>
      h.assert_eq[USize](1, diags.size())
      try
        h.assert_eq[USize](13, diags(0)?.line)
      else
        h.fail("could not access diagnostic")
      end
    end
