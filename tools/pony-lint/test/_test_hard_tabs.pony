use "pony_test"
use "pony_check"
use lint = ".."

class \nodoc\ _TestHardTabsSingleTab is UnitTest
  """Single tab produces one diagnostic."""
  fun name(): String => "HardTabs: single tab"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "\thello", "/tmp")
    let diags = lint.HardTabs.check(sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](1, diags(0)?.column)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestHardTabsMultipleTabs is UnitTest
  """Multiple tabs produce one diagnostic each."""
  fun name(): String => "HardTabs: multiple tabs"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "\t\thello\tworld", "/tmp")
    let diags = lint.HardTabs.check(sf)
    h.assert_eq[USize](3, diags.size())
    try
      h.assert_eq[USize](1, diags(0)?.column)
      h.assert_eq[USize](2, diags(1)?.column)
      h.assert_eq[USize](8, diags(2)?.column)
    else
      h.fail("could not access diagnostics")
    end

class \nodoc\ _TestHardTabsNoTabs is UnitTest
  """No tabs produces no diagnostics."""
  fun name(): String => "HardTabs: no tabs"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "  hello  world", "/tmp")
    let diags = lint.HardTabs.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestHardTabsEmpty is UnitTest
  """Empty line produces no diagnostics."""
  fun name(): String => "HardTabs: empty line"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "", "/tmp")
    let diags = lint.HardTabs.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestHardTabsProperty is UnitTest
  """
  Property: lines without tabs never produce diagnostics;
  lines with a tab always do.
  """
  fun name(): String =>
    "HardTabs: property - no tabs vs has tabs"

  fun apply(h: TestHelper) ? =>
    // Clean lines (only spaces and letters)
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 0, max = 40,
        range = ASCIILetters) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
        let diags = lint.HardTabs.check(sf)
        ph.assert_eq[USize](0, diags.size())
      })?
    // Lines with a tab always flagged
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 1, max = 20,
        range = ASCIILetters) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let line: String val = content + "\t"
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.HardTabs.check(sf)
        ph.assert_true(diags.size() > 0)
      })?
