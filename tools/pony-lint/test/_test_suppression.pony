use "pony_test"
use lint = ".."

class \nodoc\ _TestSuppressionOffOn is UnitTest
  """Lines within an off/on region are suppressed."""
  fun name(): String => "Suppression: off/on region"

  fun apply(h: TestHelper) =>
    let content: String val =
      "line 1\n"
      + "// pony-lint: off style/line-length\n"
      + "line 3\n"
      + "// pony-lint: on style/line-length\n"
      + "line 5\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_false(sup.is_suppressed(1, "style/line-length"))
    h.assert_true(sup.is_suppressed(3, "style/line-length"))
    h.assert_false(sup.is_suppressed(5, "style/line-length"))
    h.assert_eq[USize](0, sup.errors().size())

class \nodoc\ _TestSuppressionOffOnAll is UnitTest
  """Off/on without argument suppresses all rules."""
  fun name(): String => "Suppression: off/on all rules"

  fun apply(h: TestHelper) =>
    let content: String val =
      "line 1\n"
      + "// pony-lint: off\n"
      + "line 3\n"
      + "// pony-lint: on\n"
      + "line 5\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_false(sup.is_suppressed(1, "style/line-length"))
    h.assert_true(sup.is_suppressed(3, "style/line-length"))
    h.assert_true(sup.is_suppressed(3, "style/hard-tabs"))
    h.assert_false(sup.is_suppressed(5, "style/line-length"))
    h.assert_eq[USize](0, sup.errors().size())

class \nodoc\ _TestSuppressionAllow is UnitTest
  """Allow suppresses only the next line."""
  fun name(): String => "Suppression: allow next line"

  fun apply(h: TestHelper) =>
    let content: String val =
      "line 1\n"
      + "// pony-lint: allow style/line-length\n"
      + "line 3\n"
      + "line 4\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_false(sup.is_suppressed(1, "style/line-length"))
    h.assert_true(sup.is_suppressed(3, "style/line-length"))
    h.assert_false(sup.is_suppressed(4, "style/line-length"))
    h.assert_eq[USize](0, sup.errors().size())

class \nodoc\ _TestSuppressionCategory is UnitTest
  """Category-level suppression covers all rules in that category."""
  fun name(): String => "Suppression: category suppression"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off style\n"
      + "line 2\n"
      + "// pony-lint: on style\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_true(sup.is_suppressed(2, "style/line-length"))
    h.assert_true(sup.is_suppressed(2, "style/hard-tabs"))
    h.assert_false(sup.is_suppressed(2, "other/some-rule"))
    h.assert_eq[USize](0, sup.errors().size())

class \nodoc\ _TestSuppressionRuleOverridesCategory is UnitTest
  """Rule-specific on overrides category-level off."""
  fun name(): String => "Suppression: rule on overrides category off"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off style\n"
      + "// pony-lint: on style/line-length\n"
      + "line 3\n"
      + "// pony-lint: on style\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_false(sup.is_suppressed(3, "style/line-length"))
    h.assert_true(sup.is_suppressed(3, "style/hard-tabs"))
    h.assert_eq[USize](0, sup.errors().size())

class \nodoc\ _TestSuppressionUnclosed is UnitTest
  """Unclosed off at EOF produces lint/unclosed-suppression error."""
  fun name(): String => "Suppression: unclosed off error"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off style/line-length\n"
      + "line 2\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_eq[USize](1, sup.errors().size())
    try
      let err = sup.errors()(0)?
      h.assert_eq[String]("lint/unclosed-suppression", err.rule_id)
    else
      h.fail("could not access error")
    end

class \nodoc\ _TestSuppressionDuplicateOff is UnitTest
  """Duplicate off for same target produces lint/malformed-suppression."""
  fun name(): String => "Suppression: duplicate off error"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off style/line-length\n"
      + "// pony-lint: off style/line-length\n"
      + "// pony-lint: on style/line-length\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    var found_malformed = false
    for err in sup.errors().values() do
      if err.rule_id == "lint/malformed-suppression" then
        found_malformed = true
        break
      end
    end
    h.assert_true(found_malformed)

class \nodoc\ _TestSuppressionMalformed is UnitTest
  """Unknown directive produces lint/malformed-suppression."""
  fun name(): String => "Suppression: malformed directive error"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony",
      "// pony-lint: bogus style/line-length\n", "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_eq[USize](1, sup.errors().size())
    try
      let err = sup.errors()(0)?
      h.assert_eq[String]("lint/malformed-suppression", err.rule_id)
    else
      h.fail("could not access error")
    end

class \nodoc\ _TestSuppressionMagicCommentLines is UnitTest
  """Magic comment lines are tracked."""
  fun name(): String => "Suppression: magic comment line tracking"

  fun apply(h: TestHelper) =>
    let content: String val =
      "line 1\n"
      + "// pony-lint: off style\n"
      + "line 3\n"
      + "// pony-lint: on style\n"
      + "// pony-lint: allow style/hard-tabs\n"
      + "line 6\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_true(sup.magic_comment_lines().contains(2))
    h.assert_true(sup.magic_comment_lines().contains(4))
    h.assert_true(sup.magic_comment_lines().contains(5))
    h.assert_false(sup.magic_comment_lines().contains(1))
    h.assert_false(sup.magic_comment_lines().contains(3))
    h.assert_false(sup.magic_comment_lines().contains(6))

class \nodoc\ _TestSuppressionLintNotSuppressible is UnitTest
  """lint/* rules cannot be suppressed."""
  fun name(): String => "Suppression: lint/* not suppressible"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off\n"
      + "line 2\n"
      + "// pony-lint: on\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_false(sup.is_suppressed(2, "lint/unclosed-suppression"))
    h.assert_false(sup.is_suppressed(2, "lint/malformed-suppression"))
    // But regular rules are suppressed
    h.assert_true(sup.is_suppressed(2, "style/line-length"))

class \nodoc\ _TestSuppressionEmptyDirective is UnitTest
  """Empty directive (just 'pony-lint:') produces error."""
  fun name(): String => "Suppression: empty directive error"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony",
      "// pony-lint:\n", "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_eq[USize](1, sup.errors().size())
    try
      h.assert_eq[String]("lint/malformed-suppression",
        sup.errors()(0)?.rule_id)
    else
      h.fail("could not access error")
    end

class \nodoc\ _TestSuppressionOverrideThenReSuppress is UnitTest
  """
  A rule-specific on within a category off, followed by a later
  rule-specific off, correctly re-suppresses the rule.
  """
  fun name(): String =>
    "Suppression: override then re-suppress"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off style\n"       // line 1
        + "// pony-lint: on style/ll\n"  // line 2: override
        + "code A\n"                     // line 3: ll NOT suppressed
        + "// pony-lint: on style\n"     // line 4: end category off
        + "// pony-lint: off style/ll\n" // line 5: explicit off
        + "code B\n"                     // line 6: ll suppressed
        + "// pony-lint: on style/ll\n"  // line 7: end
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    // Line 3: style/ll overridden, not suppressed
    h.assert_false(sup.is_suppressed(3, "style/ll"))
    // Line 3: style/other still suppressed by category
    h.assert_true(sup.is_suppressed(3, "style/other"))
    // Line 6: style/ll re-suppressed by explicit off
    h.assert_true(sup.is_suppressed(6, "style/ll"))
    h.assert_eq[USize](0, sup.errors().size())

class \nodoc\ _TestSuppressionCategoryOnOverridesWildcardOff is UnitTest
  """Category-level on within wildcard off re-enables that category."""
  fun name(): String =>
    "Suppression: category on overrides wildcard off"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off\n"               // line 1: suppress all
        + "// pony-lint: on style\n"       // line 2: re-enable style
        + "code\n"                         // line 3
        + "// pony-lint: on\n"             // line 4: end
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    // style rules re-enabled by category on
    h.assert_false(sup.is_suppressed(3, "style/line-length"))
    h.assert_false(sup.is_suppressed(3, "style/hard-tabs"))
    // non-style rules still suppressed
    h.assert_true(sup.is_suppressed(3, "other/some-rule"))
    h.assert_eq[USize](0, sup.errors().size())

class \nodoc\ _TestSuppressionWildcardOnOverridesCategoryOff is UnitTest
  """Wildcard on within category off re-enables everything."""
  fun name(): String =>
    "Suppression: wildcard on overrides category off"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off style\n"          // line 1: suppress style
        + "// pony-lint: on\n"             // line 2: re-enable all
        + "code\n"                         // line 3
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_false(sup.is_suppressed(3, "style/line-length"))
    h.assert_false(sup.is_suppressed(3, "style/hard-tabs"))
    h.assert_eq[USize](0, sup.errors().size())

class \nodoc\ _TestSuppressionNoSpaceAfterSlashes is UnitTest
  """//pony-lint: (no space after //) produces malformed error."""
  fun name(): String =>
    "Suppression: no space after // is malformed"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony",
      "//pony-lint: off style\n", "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_eq[USize](1, sup.errors().size())
    try
      let err = sup.errors()(0)?
      h.assert_eq[String]("lint/malformed-suppression", err.rule_id)
      h.assert_true(err.message.contains("space"))
    else
      h.fail("could not access error")
    end
    // The directive should NOT be processed as a suppression
    h.assert_false(sup.is_suppressed(2, "style/line-length"))

class \nodoc\ _TestSuppressionDoubleSpaceInDirective is UnitTest
  """Extra spaces between action and target are handled correctly."""
  fun name(): String =>
    "Suppression: double space in directive"

  fun apply(h: TestHelper) =>
    let content: String val =
      "// pony-lint: off  style/line-length\n"
        + "line 2\n"
        + "// pony-lint: on style/line-length\n"
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let sup = lint.Suppressions(sf)
    h.assert_true(sup.is_suppressed(2, "style/line-length"))
    h.assert_eq[USize](0, sup.errors().size())
