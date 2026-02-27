use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestPartialCallSpacingClean is UnitTest
  """Partial call with ? immediately after ) produces no diagnostics."""
  fun name(): String => "PartialCallSpacing: attached '?' is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply() ? =>\n" +
      "    let a = Array[U32](1)\n" +
      "    a.push(0)\n" +
      "    a(0)?\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PartialCallSpacing)
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

class \nodoc\ _TestPartialCallSpacingViolation is UnitTest
  """Partial call with space before ? produces 1 diagnostic."""
  fun name(): String => "PartialCallSpacing: space before '?' flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply() ? =>\n" +
      "    let a = Array[U32](1)\n" +
      "    a.push(0)\n" +
      "    a(0) ?\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PartialCallSpacing)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/partial-call-spacing", diags(0)?.rule_id)
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

class \nodoc\ _TestPartialCallSpacingNonPartialClean is UnitTest
  """Non-partial call produces no diagnostics."""
  fun name(): String => "PartialCallSpacing: non-partial call is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let a = Array[U32](1)\n" +
      "    a.push(0)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PartialCallSpacing)
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
