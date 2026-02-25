use "pony_test"
use ast = "ast"
use lint = ".."

class \nodoc\ _TestTypeNamingClean is UnitTest
  """CamelCase type names produce no diagnostics."""
  fun name(): String => "TypeNaming: CamelCase types are clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source = "class Foo\nprimitive Bar\nactor Baz\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TypeNaming)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestTypeNamingViolation is UnitTest
  """Non-CamelCase type names produce diagnostics."""
  fun name(): String => "TypeNaming: non-CamelCase type flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source = "class foo_bar\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TypeNaming)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String]("style/type-naming", diags(0)?.rule_id)
            h.assert_true(diags(0)?.message.contains("foo_bar"))
          else h.fail("could not access diagnostic")
          end
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestTypeNamingPrivate is UnitTest
  """Private CamelCase type names (with leading _) are clean."""
  fun name(): String => "TypeNaming: private _CamelCase is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source = "primitive _PrivateType\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TypeNaming)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestTypeNamingAllEntityTypes is UnitTest
  """All entity types are checked for CamelCase."""
  fun name(): String => "TypeNaming: all 7 entity types checked"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class GoodClass\n" +
      "actor GoodActor\n" +
      "primitive GoodPrimitive\n" +
      "struct GoodStruct\n" +
      "trait GoodTrait\n" +
      "interface GoodInterface\n" +
      "type GoodType is GoodClass\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.TypeNaming)
          h.assert_eq[USize](0, diags.size())
        else h.fail("no module")
        end
      else h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

primitive \nodoc\ _CollectRuleDiags
  """Helper: walk a module's AST and collect diagnostics from a single rule."""
  fun apply(mod: ast.Module val, sf: lint.SourceFile val,
    rule: lint.ASTRule val): Array[lint.Diagnostic val] val
  =>
    let collector = _RuleCollector(rule, sf)
    mod.ast.visit(collector)
    collector.diagnostics()

class \nodoc\ _RuleCollector is ast.ASTVisitor
  """Walks an AST and collects diagnostics from a single rule."""
  let _rule: lint.ASTRule val
  let _sf: lint.SourceFile val
  let _filter: Array[ast.TokenId] val
  let _diags: Array[lint.Diagnostic val]

  new create(rule: lint.ASTRule val, sf: lint.SourceFile val) =>
    _rule = rule
    _sf = sf
    _filter = rule.node_filter()
    _diags = Array[lint.Diagnostic val]

  fun ref visit(node: ast.AST box): ast.VisitResult =>
    for tk in _filter.values() do
      if node.id() == tk then
        let d = _rule.check(node, _sf)
        for diag in d.values() do
          _diags.push(diag)
        end
        break
      end
    end
    ast.Continue

  fun ref leave(node: ast.AST box): ast.VisitResult =>
    ast.Continue

  fun diagnostics(): Array[lint.Diagnostic val] val =>
    let result = recover iso Array[lint.Diagnostic val] end
    for d in _diags.values() do
      result.push(d)
    end
    consume result
