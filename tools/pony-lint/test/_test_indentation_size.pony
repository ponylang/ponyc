use "pony_test"
use lint = ".."

class \nodoc\ _TestIndentationSizeClean is UnitTest
  """Lines indented at multiples of 2 produce no diagnostics."""
  fun name(): String => "IndentationSize: clean even indentation"

  fun apply(h: TestHelper) =>
    let src = "class Foo\n  fun apply() =>\n    None\n      let x = 1\n"
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.IndentationSize.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestIndentationSizeOddViolation is UnitTest
  """Line with 3-space indentation produces 1 diagnostic."""
  fun name(): String => "IndentationSize: 3-space indentation flagged"

  fun apply(h: TestHelper) =>
    let src = "class Foo\n   fun apply() => None\n"
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.IndentationSize.check(sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[String](
        "style/indentation-size", diags(0)?.rule_id)
      h.assert_eq[USize](2, diags(0)?.line)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestIndentationSizeOneSpace is UnitTest
  """Line with 1-space indentation produces 1 diagnostic."""
  fun name(): String => "IndentationSize: 1-space indentation flagged"

  fun apply(h: TestHelper) =>
    let src = " let x = 1\n"
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.IndentationSize.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestIndentationSizeDocstringSkipped is UnitTest
  """Odd indentation inside triple-quoted string is not flagged."""
  fun name(): String => "IndentationSize: inside docstring skipped"

  fun apply(h: TestHelper) =>
    let src = "  \"\"\"\n   odd indentation inside\n  \"\"\"\n"
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.IndentationSize.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestIndentationSizeBlankSkipped is UnitTest
  """Blank lines produce no diagnostics."""
  fun name(): String => "IndentationSize: blank lines skipped"

  fun apply(h: TestHelper) =>
    let src = "\n\n  let x = 1\n\n"
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.IndentationSize.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestIndentationSizeZeroIndent is UnitTest
  """Lines at column 0 are not flagged."""
  fun name(): String => "IndentationSize: zero indentation clean"

  fun apply(h: TestHelper) =>
    let src = "class Foo\nfun apply() => None\n"
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.IndentationSize.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestIndentationSizeTabSkipped is UnitTest
  """Tab-indented line is not flagged (tabs are style/hard-tabs concern)."""
  fun name(): String => "IndentationSize: tab indentation not flagged"

  fun apply(h: TestHelper) =>
    let src = "\tlet x = 1\n"
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.IndentationSize.check(sf)
    h.assert_eq[USize](0, diags.size())
