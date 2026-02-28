use "pony_test"
use "collections"
use lint = ".."

class \nodoc\ _TestConfigDefaultAllEnabled is UnitTest
  """Default config returns default status for all rules."""
  fun name(): String => "Config: default returns rule defaults"

  fun apply(h: TestHelper) =>
    let config = lint.LintConfig.default()
    match config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOn => h.assert_true(true)
    | lint.RuleOff => h.fail("expected RuleOn")
    end

class \nodoc\ _TestConfigCLIDisableRule is UnitTest
  """CLI disable always produces RuleOff."""
  fun name(): String => "Config: CLI disable overrides everything"

  fun apply(h: TestHelper) =>
    let disabled =
      recover val Set[String] .> set("style/line-length") end
    let file_rules = recover val Map[String, lint.RuleStatus] end
    let config = lint.LintConfig(disabled, file_rules)
    match config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigCLIDisableCategory is UnitTest
  """CLI disable by category disables all rules in that category."""
  fun name(): String => "Config: CLI disable category"

  fun apply(h: TestHelper) =>
    let disabled =
      recover val Set[String] .> set("style") end
    let file_rules = recover val Map[String, lint.RuleStatus] end
    let config = lint.LintConfig(disabled, file_rules)
    match config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigFileRuleOverride is UnitTest
  """File config rule-specific override takes effect."""
  fun name(): String => "Config: file rule-specific override"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOff
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    match config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigFileCategoryDefault is UnitTest
  """File config category setting applies to rules in that category."""
  fun name(): String => "Config: file category default"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style") = lint.RuleOff
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    match config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigCLIOverridesFile is UnitTest
  """CLI disable overrides file config that enables a rule."""
  fun name(): String => "Config: CLI overrides file config"

  fun apply(h: TestHelper) =>
    let disabled =
      recover val Set[String] .> set("style/line-length") end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOn
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    match config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigParseValidJSON is UnitTest
  """Valid JSON config parses correctly."""
  fun name(): String => "Config: parse valid JSON"

  fun apply(h: TestHelper) =>
    match lint.ConfigLoader.parse(
      """{"rules": {"style/line-length": "off", "style": "on"}}""")
    | let rules: Map[String, lint.RuleStatus] val =>
      h.assert_eq[USize](2, rules.size())
    | let err: lint.ConfigError =>
      h.fail("unexpected error: " + err.message)
    end

class \nodoc\ _TestConfigParseMalformedJSON is UnitTest
  """Malformed JSON produces ConfigError."""
  fun name(): String => "Config: malformed JSON -> error"

  fun apply(h: TestHelper) =>
    match lint.ConfigLoader.parse("{not valid json}")
    | let rules: Map[String, lint.RuleStatus] val =>
      h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("malformed JSON"))
    end

class \nodoc\ _TestConfigParseInvalidStatus is UnitTest
  """Invalid rule status value produces ConfigError."""
  fun name(): String => "Config: invalid status value -> error"

  fun apply(h: TestHelper) =>
    match lint.ConfigLoader.parse(
      """{"rules": {"style/line-length": "maybe"}}""")
    | let rules: Map[String, lint.RuleStatus] val =>
      h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("invalid rule status"))
    end

class \nodoc\ _TestConfigParseNoRules is UnitTest
  """JSON with no rules key returns empty map."""
  fun name(): String => "Config: no rules key -> empty"

  fun apply(h: TestHelper) =>
    match lint.ConfigLoader.parse("{}")
    | let rules: Map[String, lint.RuleStatus] val =>
      h.assert_eq[USize](0, rules.size())
    | let err: lint.ConfigError =>
      h.fail("unexpected error: " + err.message)
    end
