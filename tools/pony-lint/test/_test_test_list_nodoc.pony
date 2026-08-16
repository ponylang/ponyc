use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestTestListNodocFlagged is UnitTest
  """Primitive providing TestList without \nodoc\ is flagged."""
  fun name(): String => "TestListNodoc: missing annotation flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "use \"pony_test\"\n" +
      "\n" +
      "primitive _MyTests is TestList\n" +
      "  new make() => None\n" +
      "  fun tag tests(test: PonyTest) => None\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TestListNodoc)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/testlist-nodoc", diags(0)?.rule_id)
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

class \nodoc\ _TestTestListNodocClean is UnitTest
  """Primitive providing TestList with \nodoc\ is clean."""
  fun name(): String => "TestListNodoc: annotated provider is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "use \"pony_test\"\n" +
      "\n" +
      "primitive \\nodoc\\ _MyTests is TestList\n" +
      "  new make() => None\n" +
      "  fun tag tests(test: PonyTest) => None\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TestListNodoc)
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

class \nodoc\ _TestTestListNodocNoTestList is UnitTest
  """Type not providing TestList is not flagged."""
  fun name(): String => "TestListNodoc: non-TestList provider clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "trait Foo\n" +
      "  fun bar(): String\n" +
      "\n" +
      "primitive MyImpl is Foo\n" +
      "  fun bar(): String => \"hello\"\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TestListNodoc)
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

class \nodoc\ _TestTestListNodocNoProvidesClean is UnitTest
  """Type with no provides clause is not flagged."""
  fun name(): String => "TestListNodoc: no provides clause clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "primitive Standalone\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TestListNodoc)
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

class \nodoc\ _TestTestListNodocActorFlagged is UnitTest
  """Actor providing TestList without \nodoc\ is flagged."""
  fun name(): String => "TestListNodoc: actor without annotation flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "use \"pony_test\"\n" +
      "\n" +
      "actor Main is TestList\n" +
      "  new create(env: Env) => PonyTest(env, this)\n" +
      "  new make() => None\n" +
      "  fun tag tests(test: PonyTest) => None\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TestListNodoc)
          h.assert_eq[USize](1, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestTestListNodocIsectFlagged is UnitTest
  """Type providing TestList in an intersection without \nodoc\ is flagged."""
  fun name(): String => "TestListNodoc: intersection provider flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "use \"pony_test\"\n" +
      "\n" +
      "trait MyTrait\n" +
      "  fun extra(): None\n" +
      "\n" +
      "primitive _MyTests is (TestList & MyTrait)\n" +
      "  new make() => None\n" +
      "  fun tag tests(test: PonyTest) => None\n" +
      "  fun extra(): None => None\n"
    try
      (let program, let sf) =
        _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TestListNodoc)
          h.assert_eq[USize](1, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestTestListNodocMetadata is UnitTest
  """Rule metadata is correct."""
  fun name(): String => "TestListNodoc: metadata"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("style/testlist-nodoc", lint.TestListNodoc.id())
    h.assert_eq[String]("style", lint.TestListNodoc.category())
    match lint.TestListNodoc.required_pass()
    | ast.PassParse => None
    else
      h.fail("required_pass should be PassParse")
    end
    match lint.TestListNodoc.default_status()
    | lint.RuleOn => None
    else
      h.fail("default_status should be RuleOn")
    end
