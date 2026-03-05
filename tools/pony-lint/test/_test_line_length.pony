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
    let line: String val =
      recover val
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
  lines > 80 codepoints always do (when the line contains no string
  literals — the ASCIILetters generator never produces `"`).
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
        let sf =
          lint.SourceFile("/tmp/t.pony", content, "/tmp")
        let diags = lint.LineLength.check(sf)
        ph.assert_eq[USize](1, diags.size())
      })?

class \nodoc\ _TestLineLengthStringExemptNoSpaces is UnitTest
  """No-space string crossing column 80 is exempt."""
  fun name(): String =>
    "LineLength: no-space string crossing col 80 -> exempt"

  fun apply(h: TestHelper) =>
    // 4 spaces + `let x = "` = 13 chars prefix, then 70 'a's + `"`
    // String starts at col 14, ends at col 84. No spaces -> exempt.
    let line: String val =
      recover val
        String
          .> append("    let x = \"")
          .> append("a".mul(70))
          .> append("\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthStringNotExemptSpaces is UnitTest
  """String with spaces crossing column 80 is not exempt."""
  fun name(): String =>
    "LineLength: string with spaces crossing col 80 -> flagged"

  fun apply(h: TestHelper) =>
    let line: String val =
      recover val
        String
          .> append("    let x = \"")
          .> append("a".mul(30))
          .> append(" ")
          .> append("a".mul(39))
          .> append("\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringBeforeCol80 is UnitTest
  """String ends before col 80; line is long from trailing content."""
  fun name(): String =>
    "LineLength: string before col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // Short string at beginning, then lots of trailing code
    let line: String val =
      recover val
        String
          .> append("    let x = \"short\"")
          .> append(" + ")
          .> append("a".mul(70))
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringAfterCol80 is UnitTest
  """String starts after col 80 (doesn't cross the boundary)."""
  fun name(): String =>
    "LineLength: string starts after col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // 82 chars of code, then a short string
    let line: String val =
      recover val
        String
          .> append("a".mul(82))
          .> append("\"url\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringEndsAtCol80 is UnitTest
  """No-space string ending exactly at col 80 on an over-80 line."""
  fun name(): String =>
    "LineLength: string ends at col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // 13 chars prefix + 66 'a's + closing quote at col 80 + trailing
    // String covers cols 14..80. Does not cross col 80 -> not exempt.
    let line: String val =
      recover val
        String
          .> append("    let x = \"")
          .> append("a".mul(66))
          .> append("\" + more_stuff_here")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthDocstringNoExempt is UnitTest
  """Long line inside a docstring is not eligible for exemption."""
  fun name(): String =>
    "LineLength: long line inside docstring -> flagged"

  fun apply(h: TestHelper) =>
    // Three lines: opening """, long content with a no-space "string",
    // closing """
    let content: String val =
      recover val
        String
          .> append("  \"\"\"\n")
          .> append("  let x = \"")
          .> append("a".mul(75))
          .> append("\"\n")
          .> append("  \"\"\"")
      end
    let sf = lint.SourceFile("/tmp/t.pony", content, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthMultipleStringsOneExempt is UnitTest
  """Two strings on one line; second one qualifies for exemption."""
  fun name(): String =>
    "LineLength: second string qualifies -> exempt"

  fun apply(h: TestHelper) =>
    // Short string with spaces, then a long no-space string crossing 80
    let line: String val =
      recover val
        String
          .> append("    f(\"a b\", \"")
          .> append("x".mul(70))
          .> append("\")")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthEscapedQuotes is UnitTest
  """String with escaped quotes inside, crossing col 80."""
  fun name(): String =>
    "LineLength: escaped quotes in string -> exempt"

  fun apply(h: TestHelper) =>
    // String contains \" inside but no spaces, crosses col 80
    let line: String val =
      recover val
        String
          .> append("    let x = \"abc\\\"def")
          .> append("g".mul(60))
          .> append("\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthTripleQuoteLineNoExempt is UnitTest
  """Long line containing triple-quote delimiter is not eligible."""
  fun name(): String =>
    "LineLength: triple-quote delimiter line -> flagged"

  fun apply(h: TestHelper) =>
    let line: String val =
      recover val
        String
          .> append("  \"\"\"")
          .> append("a".mul(80))
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringExemptUTF8 is UnitTest
  """Multi-byte UTF-8 in string; codepoint counting is correct."""
  fun name(): String =>
    "LineLength: UTF-8 string crossing col 80 -> exempt"

  fun apply(h: TestHelper) =>
    // 13 prefix + string with multi-byte chars crossing col 80
    // "é" is 2 bytes but 1 codepoint
    let line: String val =
      recover val
        String
          .> append("    let x = \"")
          .> append("é".mul(35))
          .> append("a".mul(35))
          .> append("\"")
      end
    // 13 + 35 + 35 + 1 = 84 codepoints
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthStringExemptProperty is UnitTest
  """
  Property: a line with a no-space string crossing column 80 is always
  exempt. String length ranges from 67 to 200, ensuring the string
  always crosses column 80 (prefix is 14 codepoints including quotes).
  """
  fun name(): String =>
    "LineLength: property - no-space strings always exempt"

  fun apply(h: TestHelper) ? =>
    let gen =
      recover val Generators.usize(where min = 67, max = 200) end
    PonyCheck.for_all[USize](gen, h)(
      {(str_len: USize, ph: PropertyHelper) =>
        let line: String val =
          recover val
            String
              .> append("    let x = \"")
              .> append("a".mul(str_len))
              .> append("\"")
          end
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.LineLength.check(sf)
        ph.assert_eq[USize](0, diags.size())
      })?

class \nodoc\ _TestLineLengthStringFlaggedProperty is UnitTest
  """
  Property: a line with a space-containing string crossing column 80 is
  always flagged. Same construction as the exempt property but one
  character is replaced with a space.
  """
  fun name(): String =>
    "LineLength: property - space strings always flagged"

  fun apply(h: TestHelper) ? =>
    let gen =
      recover val Generators.usize(where min = 67, max = 200) end
    PonyCheck.for_all[USize](gen, h)(
      {(str_len: USize, ph: PropertyHelper) =>
        // Put a space in the middle of the string
        let half = str_len / 2
        let line: String val =
          recover val
            String
              .> append("    let x = \"")
              .> append("a".mul(half))
              .> append(" ")
              .> append("a".mul(str_len - half - 1))
              .> append("\"")
          end
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.LineLength.check(sf)
        ph.assert_eq[USize](1, diags.size())
      })?
