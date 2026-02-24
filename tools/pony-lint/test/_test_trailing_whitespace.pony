use "pony_test"
use "pony_check"
use lint = ".."

class \nodoc\ _TestTrailingWhitespaceSpace is UnitTest
  """Trailing space is flagged."""
  fun name(): String => "TrailingWhitespace: trailing space"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "hello ", "/tmp")
    let diags = lint.TrailingWhitespace.check(sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](6, diags(0)?.column)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestTrailingWhitespaceTab is UnitTest
  """Trailing tab is flagged."""
  fun name(): String => "TrailingWhitespace: trailing tab"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "hello\t", "/tmp")
    let diags = lint.TrailingWhitespace.check(sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](6, diags(0)?.column)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestTrailingWhitespaceClean is UnitTest
  """No trailing whitespace produces no diagnostics."""
  fun name(): String => "TrailingWhitespace: clean line"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "hello", "/tmp")
    let diags = lint.TrailingWhitespace.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestTrailingWhitespaceEmptyLine is UnitTest
  """Empty lines don't trigger."""
  fun name(): String => "TrailingWhitespace: empty line"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "", "/tmp")
    let diags = lint.TrailingWhitespace.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestTrailingWhitespaceMultiple is UnitTest
  """Multiple trailing spaces: column is position of first."""
  fun name(): String =>
    "TrailingWhitespace: multiple trailing spaces"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "hi   ", "/tmp")
    let diags = lint.TrailingWhitespace.check(sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](3, diags(0)?.column)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestTrailingWhitespaceProperty is UnitTest
  """
  Property: lines without trailing whitespace never produce
  diagnostics; lines with trailing spaces always do.
  """
  fun name(): String =>
    "TrailingWhitespace: property - clean vs trailing"

  fun apply(h: TestHelper) ? =>
    // Clean lines: no trailing whitespace
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 0, max = 40,
        range = ASCIILetters) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let line = content.clone()
        line.rstrip(" \t")
        let clean: String val = consume line
        let sf = lint.SourceFile("/tmp/t.pony", clean, "/tmp")
        let diags = lint.TrailingWhitespace.check(sf)
        ph.assert_eq[USize](0, diags.size())
      })?
    // Lines with trailing space always flagged
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 1, max = 20,
        range = ASCIILetters) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let line: String val = content + " "
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.TrailingWhitespace.check(sf)
        ph.assert_eq[USize](1, diags.size())
      })?
