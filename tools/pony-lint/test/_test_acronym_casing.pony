use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestAcronymCasingClean is UnitTest
  """Properly uppercased acronyms produce no diagnostics."""
  fun name(): String => "AcronymCasing: uppercase acronyms are clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source = "class JSONDoc\nclass HTTPClient\nclass MyClass\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AcronymCasing)
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

class \nodoc\ _TestAcronymCasingViolation is UnitTest
  """Lowered acronyms in type names produce diagnostics."""
  fun name(): String => "AcronymCasing: lowered acronym flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source = "class JsonDoc\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AcronymCasing)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String]("style/acronym-casing", diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("JSON"))
            h.assert_true(diags(0)?.message.contains("JsonDoc"))
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

class \nodoc\ _TestAcronymCasingHTTP is UnitTest
  """Http -> HTTP violation detected."""
  fun name(): String => "AcronymCasing: Http -> HTTP violation"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source = "class HttpClient\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AcronymCasing)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_true(diags(0)?.message.contains("HTTP"))
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
