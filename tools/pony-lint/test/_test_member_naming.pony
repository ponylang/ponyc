use "pony_test"
use ast = "ast"
use lint = ".."

class \nodoc\ _TestMemberNamingClean is UnitTest
  """snake_case member names produce no diagnostics."""
  fun name(): String => "MemberNaming: snake_case members are clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  let my_field: U32 = 0\n" +
      "  fun my_fun(): None => None\n" +
      "  new create() => None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MemberNaming)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMemberNamingMethodViolation is UnitTest
  """Non-snake_case method names produce diagnostics."""
  fun name(): String => "MemberNaming: non-snake_case method flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun MyFun(): None => None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MemberNaming)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String]("style/member-naming", diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("MyFun"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMemberNamingFieldViolation is UnitTest
  """Non-snake_case field names produce diagnostics."""
  fun name(): String => "MemberNaming: non-snake_case field flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  let CamelField: U32 = 0\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MemberNaming)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_true(diags(0)?.message.contains("CamelField"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMemberNamingDontcareSkipped is UnitTest
  """Dontcare locals (_) are skipped."""
  fun name(): String => "MemberNaming: dontcare _ is skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let _ = U32(1)\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MemberNaming)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMemberNamingParamViolation is UnitTest
  """Non-snake_case parameter names produce diagnostics."""
  fun name(): String => "MemberNaming: non-snake_case param flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(BadParam: U32): None => None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.MemberNaming)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_true(diags(0)?.message.contains("BadParam"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end
