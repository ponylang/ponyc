use "pony_test"
use lint = ".."

class \nodoc\ _TestExplainFormatEnabled is UnitTest
  """Verify format output for an enabled-by-default rule."""
  fun name(): String => "ExplainHelpers: format enabled rule"

  fun apply(h: TestHelper) =>
    let output = lint.ExplainHelpers.format(
      "style/dot-spacing",
      "no spaces around '.'; '.>' spaced as infix operator",
      lint.RuleOn)

    h.assert_true(output.contains("style/dot-spacing"))
    h.assert_true(output.contains("enabled by default"))
    h.assert_true(
      output.contains(
        "no spaces around '.'; '.>' spaced as infix operator"))
    h.assert_true(
      output.contains(
        "https://www.ponylang.io/use/linting/rule-reference/"
          + "#styledot-spacing"))

class \nodoc\ _TestExplainFormatDisabled is UnitTest
  """Verify format output for a disabled-by-default rule."""
  fun name(): String => "ExplainHelpers: format disabled rule"

  fun apply(h: TestHelper) =>
    let output = lint.ExplainHelpers.format(
      "style/some-rule",
      "checks something",
      lint.RuleOff)

    h.assert_true(output.contains("style/some-rule"))
    h.assert_true(output.contains("disabled by default"))
    h.assert_true(output.contains("checks something"))

class \nodoc\ _TestExplainURLConstruction is UnitTest
  """Verify rule_url produces correct URLs for various rule IDs."""
  fun name(): String => "ExplainHelpers: rule_url construction"

  fun apply(h: TestHelper) =>
    let base = "https://www.ponylang.io/use/linting/rule-reference/#"

    h.assert_eq[String val](
      base + "styledot-spacing",
      lint.ExplainHelpers.rule_url("style/dot-spacing"))

    h.assert_eq[String val](
      base + "styleline-length",
      lint.ExplainHelpers.rule_url("style/line-length"))

    h.assert_eq[String val](
      base + "namingtype-naming",
      lint.ExplainHelpers.rule_url("naming/type-naming"))

    h.assert_eq[String val](
      base + "structuralmatch-single-line",
      lint.ExplainHelpers.rule_url("structural/match-single-line"))
