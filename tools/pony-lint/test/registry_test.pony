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
    let rules: Array[lint.TextRule val] val = recover val
      [as lint.TextRule val: _DummyRule; _DummyRule2]
    end
    let registry = lint.RuleRegistry(rules, lint.LintConfig.default())
    h.assert_eq[USize](2, registry.enabled_text_rules().size())
    h.assert_eq[USize](2, registry.all_text_rules().size())

class \nodoc\ _TestRegistryDisableRule is UnitTest
  """Disabling a rule excludes it from enabled list."""
  fun name(): String => "Registry: disable rule"

  fun apply(h: TestHelper) =>
    let rules: Array[lint.TextRule val] val = recover val
      [as lint.TextRule val: _DummyRule; _DummyRule2]
    end
    let disabled = recover val
      let s = Set[String]
      s.set("test/dummy")
      s
    end
    let config = lint.LintConfig(disabled,
      recover val Map[String, lint.RuleStatus] end)
    let registry = lint.RuleRegistry(rules, config)
    h.assert_eq[USize](1, registry.enabled_text_rules().size())
    h.assert_eq[USize](2, registry.all_text_rules().size())

class \nodoc\ _TestRegistryDisableCategory is UnitTest
  """Disabling a category excludes all rules in that category."""
  fun name(): String => "Registry: disable category"

  fun apply(h: TestHelper) =>
    let rules: Array[lint.TextRule val] val = recover val
      [as lint.TextRule val: _DummyRule; _DummyRule2]
    end
    let disabled = recover val
      let s = Set[String]
      s.set("test")
      s
    end
    let config = lint.LintConfig(disabled,
      recover val Map[String, lint.RuleStatus] end)
    let registry = lint.RuleRegistry(rules, config)
    h.assert_eq[USize](0, registry.enabled_text_rules().size())
    h.assert_eq[USize](2, registry.all_text_rules().size())

class \nodoc\ _TestRegistryAllAlwaysComplete is UnitTest
  """all_text_rules always returns all rules regardless of config."""
  fun name(): String => "Registry: all_text_rules always complete"

  fun apply(h: TestHelper) =>
    let rules: Array[lint.TextRule val] val = recover val
      [as lint.TextRule val: _DummyRule; _DummyRule2]
    end
    let disabled = recover val
      let s = Set[String]
      s.set("test/dummy")
      s.set("test/dummy2")
      s
    end
    let config = lint.LintConfig(disabled,
      recover val Map[String, lint.RuleStatus] end)
    let registry = lint.RuleRegistry(rules, config)
    h.assert_eq[USize](0, registry.enabled_text_rules().size())
    h.assert_eq[USize](2, registry.all_text_rules().size())
