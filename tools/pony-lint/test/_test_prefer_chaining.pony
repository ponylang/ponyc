use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestPreferChainingViolation is UnitTest
  """Basic let-push-return pattern with 2+ calls is flagged."""
  fun name(): String => "PreferChaining: basic pattern flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1))\n" +
      "    x.qux(U32(2))\n" +
      "    x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String]("style/prefer-chaining",
              diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("x"))
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

class \nodoc\ _TestPreferChainingVarViolation is UnitTest
  """Var binding with let-push-return pattern is also flagged."""
  fun name(): String => "PreferChaining: var pattern flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    var x = Bar\n" +
      "    x.baz(U32(1))\n" +
      "    x.qux(U32(2))\n" +
      "    x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String]("style/prefer-chaining",
              diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("x"))
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

class \nodoc\ _TestPreferChainingAlreadyChaining is UnitTest
  """Code already using .> chaining produces no diagnostics."""
  fun name(): String => "PreferChaining: already chaining is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    Bar .> baz(U32(1)) .> qux(U32(2))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
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

class \nodoc\ _TestPreferChainingSingleCallTrailingRef is UnitTest
  """
  Single call with trailing reference is flagged — .> eliminates the
  intermediate variable.
  """
  fun name(): String => "PreferChaining: single call trailing ref"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1))\n" +
      "    x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String]("style/prefer-chaining",
              diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("x"))
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

class \nodoc\ _TestPreferChainingSingleCallRefNotLast is UnitTest
  """
  Single call with trailing reference followed by another sibling is not
  flagged — the variable is used beyond just building and returning.
  """
  fun name(): String => "PreferChaining: single call ref not last"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1))\n" +
      "    x\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
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

class \nodoc\ _TestPreferChainingNoTrailingRef is UnitTest
  """Calls without trailing variable reference are flagged."""
  fun name(): String => "PreferChaining: no trailing ref flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1))\n" +
      "    x.qux(U32(2))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String]("style/prefer-chaining",
              diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("x"))
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

class \nodoc\ _TestPreferChainingUsedAfterCalls is UnitTest
  """Calls followed by an unrelated statement are not flagged."""
  fun name(): String => "PreferChaining: used after calls is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1))\n" +
      "    x.qux(U32(2))\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
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

class \nodoc\ _TestPreferChainingInterspersed is UnitTest
  """Non-consecutive calls on the variable produces no diagnostics."""
  fun name(): String => "PreferChaining: interspersed statements is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1))\n" +
      "    None\n" +
      "    x.qux(U32(2))\n" +
      "    x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
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

class \nodoc\ _TestPreferChainingNormalLet is UnitTest
  """Normal let usage with a single call produces no diagnostics."""
  fun name(): String => "PreferChaining: normal let usage is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz()\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.PreferChaining)
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
