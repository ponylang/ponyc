use "files"
use "pony_test"
use "pony_check"
use lint = ".."

class \nodoc\ _TestSourceFileSplitPreservesContent is UnitTest
  """
  Joining lines with newlines reconstructs the original content
  (after CRLF stripping).
  """
  fun name(): String => "SourceFile: splitting preserves content"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 0, max = 200,
        range = ASCIIPrintable) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let stripped = content.clone()
        stripped.remove("\r")
        let expected: String val = consume stripped
        let sf = lint.SourceFile("/tmp/test.pony", content, "/tmp")
        let rejoined: String val = "\n".join(sf.lines.values())
        ph.assert_eq[String](expected, rejoined)
      })?

class \nodoc\ _TestSourceFileNoCR is UnitTest
  """No line in a SourceFile should contain a carriage return."""
  fun name(): String => "SourceFile: no \\r in lines"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 0, max = 200) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let sf = lint.SourceFile("/tmp/test.pony", content, "/tmp")
        for line in sf.lines.values() do
          ph.assert_false(
            line.contains("\r"),
            "line should not contain \\r: " + line)
        end
      })?

class \nodoc\ _TestSourceFileLineCount is UnitTest
  """Line count equals newline count + 1 (after CRLF stripping)."""
  fun name(): String => "SourceFile: line count = newline count + 1"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 0, max = 200,
        range = ASCIIPrintable) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let stripped = content.clone()
        stripped.remove("\r")
        let stripped': String val = consume stripped
        let sf = lint.SourceFile("/tmp/test.pony", content, "/tmp")
        var newline_count: USize = 0
        for byte in stripped'.values() do
          if byte == '\n' then newline_count = newline_count + 1 end
        end
        ph.assert_eq[USize](newline_count + 1, sf.lines.size())
      })?

class \nodoc\ _TestSourceFileEmptyContent is UnitTest
  """An empty file produces a single-element array with an empty string."""
  fun name(): String => "SourceFile: empty content -> one empty line"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/test.pony", "", "/tmp")
    h.assert_eq[USize](1, sf.lines.size())
    try
      h.assert_eq[String]("", sf.lines(0)?)
    else
      h.fail("could not access lines(0)")
    end

class \nodoc\ _TestSourceFileRelPath is UnitTest
  """Relative path is computed from CWD."""
  fun name(): String => "SourceFile: relative path from CWD"

  fun apply(h: TestHelper) =>
    let cwd = Path.cwd()
    let abs_path = Path.join(cwd, Path.join("src", "main.pony"))
    let expected = Path.join("src", "main.pony")
    let sf = lint.SourceFile(abs_path, "actor Main", cwd)
    h.assert_eq[String](expected, sf.rel_path)

class \nodoc\ _TestSourceFileCRLF is UnitTest
  """CRLF line endings are normalized to LF."""
  fun name(): String => "SourceFile: CRLF normalization"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        "/tmp/test.pony",
        "line1\r\nline2\r\nline3",
        "/tmp")
    h.assert_eq[USize](3, sf.lines.size())
    try
      h.assert_eq[String]("line1", sf.lines(0)?)
      h.assert_eq[String]("line2", sf.lines(1)?)
      h.assert_eq[String]("line3", sf.lines(2)?)
    else
      h.fail("could not access lines")
    end

class \nodoc\ _TestSourceFileTrailingNewline is UnitTest
  """A trailing newline produces an extra empty line."""
  fun name(): String => "SourceFile: trailing newline"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/test.pony", "line1\n", "/tmp")
    h.assert_eq[USize](2, sf.lines.size())
    try
      h.assert_eq[String]("line1", sf.lines(0)?)
      h.assert_eq[String]("", sf.lines(1)?)
    else
      h.fail("could not access lines")
    end
