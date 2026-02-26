use "pony_test"
use "pony_check"
use lint = ".."

class \nodoc\ _TestLineLengthExactly80 is UnitTest
  """Line of exactly 80 characters does not trigger."""
  fun name(): String => "LineLength: exactly 80 chars -> no diagnostic"

  fun apply(h: TestHelper) =>
    let line = recover val String .> append("a".mul(80)) end
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthOver80 is UnitTest
  """Line of 81+ characters triggers with column 81."""
  fun name(): String => "LineLength: over 80 chars -> diagnostic"

  fun apply(h: TestHelper) =>
    let line = recover val String .> append("a".mul(95)) end
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](81, diags(0)?.column)
      h.assert_true(diags(0)?.message.contains("95"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestLineLengthMultiByteUTF8 is UnitTest
  """Multi-byte UTF-8 counts as one codepoint."""
  fun name(): String => "LineLength: multi-byte UTF-8 counts as one codepoint"

  fun apply(h: TestHelper) =>
    // 80 characters where some are multi-byte (e.g., é = 2 bytes)
    // "é" is 2 bytes but 1 codepoint
    let line: String val = recover val
      String .> append("a".mul(79)) .> append("é")
    end
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthEmptyLine is UnitTest
  """Empty lines don't trigger."""
  fun name(): String => "LineLength: empty line -> no diagnostic"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "", "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthProperty is UnitTest
  """
  Property: lines <= 80 codepoints never produce diagnostics;
  lines > 80 codepoints always do.
  """
  fun name(): String =>
    "LineLength: property - short lines OK, long lines flagged"

  fun apply(h: TestHelper) ? =>
    // Lines up to 80 chars never produce diagnostics
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 0, max = 80,
        range = ASCIIPrintable) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let line = content.clone()
        line.remove("\n")
        let safe_line: String val = consume line
        if safe_line.codepoints() <= 80 then
          let sf = lint.SourceFile("/tmp/t.pony", safe_line, "/tmp")
          let diags = lint.LineLength.check(sf)
          ph.assert_eq[USize](0, diags.size())
        end
      })?
    // Lines over 80 chars always produce diagnostics
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 81, max = 120,
        range = ASCIILetters) end, h)(
      {(content: String, ph: PropertyHelper) =>
        // ASCIILetters has no whitespace, so no newlines or \r
        let sf = lint.SourceFile(
          "/tmp/t.pony", content, "/tmp")
        let diags = lint.LineLength.check(sf)
        ph.assert_eq[USize](1, diags.size())
      })?
