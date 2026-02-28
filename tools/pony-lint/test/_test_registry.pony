use "pony_test"
use "collections"
use lint = ".."

primitive \nodoc\ _DummyRule is lint.TextRule
  fun id(): String val => "test/dummy"
  fun category(): String val => "test"
  fun description(): String val => "a dummy rule"
  fun default_status(): lint.RuleStatus => lint.RuleOn

  fun check(source: lint.SourceFile val): Array[lint.Diagnostic val] val =>
    recover val Array[lint.Diagnostic val] end

primitive \nodoc\ _DummyRule2 is lint.TextRule
  fun id(): String val => "test/dummy2"
  fun category(): String val => "test"
  fun description(): String val => "another dummy rule"
  fun default_status(): lint.RuleStatus => lint.RuleOn

  fun check(source: lint.SourceFile val): Array[lint.Diagnostic val] val =>
    recover val Array[lint.Diagnostic val] end

class \nodoc\ _TestRegistryDefaultAllEnabled is UnitTest
  """Default config enables all rules."""
  fun name(): String => "Registry: default config -> all enabled"

  fun apply(h: TestHelper) =>
    let rules: Array[lint.TextRule val] val =
      recover val [as lint.TextRule val: _DummyRule; _DummyRule2] end
    let no_ast = recover val Array[lint.ASTRule val] end
    let registry = lint.RuleRegistry(rules, no_ast, lint.LintConfig.default())
    h.assert_eq[USize](2, registry.enabled_text_rules().size())
    h.assert_eq[USize](2, registry.all_text_rules().size())

class \nodoc\ _TestRegistryDisableRule is UnitTest
  """Disabling a rule excludes it from enabled list."""
  fun name(): String => "Registry: disable rule"

  fun apply(h: TestHelper) =>
    let rules: Array[lint.TextRule val] val =
      recover val [as lint.TextRule val: _DummyRule; _DummyRule2] end
    let no_ast = recover val Array[lint.ASTRule val] end
    let disabled =
      recover val Set[String] .> set("test/dummy") end
    let config =
      lint.LintConfig(
        disabled,
        recover val Map[String, lint.RuleStatus] end)
    let registry = lint.RuleRegistry(rules, no_ast, config)
    h.assert_eq[USize](1, registry.enabled_text_rules().size())
    h.assert_eq[USize](2, registry.all_text_rules().size())

class \nodoc\ _TestRegistryDisableCategory is UnitTest
  """Disabling a category excludes all rules in that category."""
  fun name(): String => "Registry: disable category"

  fun apply(h: TestHelper) =>
    let rules: Array[lint.TextRule val] val =
      recover val [as lint.TextRule val: _DummyRule; _DummyRule2] end
    let no_ast = recover val Array[lint.ASTRule val] end
    let disabled =
      recover val Set[String] .> set("test") end
    let config =
      lint.LintConfig(
        disabled,
        recover val Map[String, lint.RuleStatus] end)
    let registry = lint.RuleRegistry(rules, no_ast, config)
    h.assert_eq[USize](0, registry.enabled_text_rules().size())
    h.assert_eq[USize](2, registry.all_text_rules().size())

class \nodoc\ _TestRegistryAllAlwaysComplete is UnitTest
  """all_text_rules always returns all rules regardless of config."""
  fun name(): String => "Registry: all_text_rules always complete"

  fun apply(h: TestHelper) =>
    let rules: Array[lint.TextRule val] val =
      recover val [as lint.TextRule val: _DummyRule; _DummyRule2] end
    let no_ast = recover val Array[lint.ASTRule val] end
    let disabled =
      recover val
        Set[String]
          .> set("test/dummy")
          .> set("test/dummy2")
      end
    let config =
      lint.LintConfig(
        disabled,
        recover val Map[String, lint.RuleStatus] end)
    let registry = lint.RuleRegistry(rules, no_ast, config)
    h.assert_eq[USize](0, registry.enabled_text_rules().size())
    h.assert_eq[USize](2, registry.all_text_rules().size())

class \nodoc\ _TestRegistryASTRules is UnitTest
  """AST rules are filtered by configuration like text rules."""
  fun name(): String => "Registry: AST rules filtered by config"

  fun apply(h: TestHelper) =>
    let no_text = recover val Array[lint.TextRule val] end
    let ast_rules: Array[lint.ASTRule val] val =
      recover val
        [as lint.ASTRule val: lint.TypeNaming; lint.MemberNaming]
      end
    let no_ast_disabled = lint.LintConfig.default()
    let registry = lint.RuleRegistry(no_text, ast_rules, no_ast_disabled)
    h.assert_eq[USize](2, registry.enabled_ast_rules().size())
    h.assert_eq[USize](2, registry.all_ast_rules().size())

    // Disable one rule
    let disabled =
      recover val Set[String] .> set("style/type-naming") end
    let config =
      lint.LintConfig(
        disabled,
        recover val Map[String, lint.RuleStatus] end)
    let registry2 = lint.RuleRegistry(no_text, ast_rules, config)
    h.assert_eq[USize](1, registry2.enabled_ast_rules().size())
    h.assert_eq[USize](2, registry2.all_ast_rules().size())
