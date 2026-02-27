use "pony_test"
use "pony_check"
use lint = ".."

class \nodoc\ _TestCommentSpacingGood is UnitTest
  """Properly spaced comment produces no diagnostic."""
  fun name(): String => "CommentSpacing: good spacing"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "// hello", "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestCommentSpacingEmpty is UnitTest
  """Empty comment `//` at end of line is OK."""
  fun name(): String => "CommentSpacing: empty comment"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "//", "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestCommentSpacingNoSpace is UnitTest
  """Comment with no space after // is flagged."""
  fun name(): String => "CommentSpacing: no space"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "//hello", "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](1, diags(0)?.column)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestCommentSpacingTwoSpaces is UnitTest
  """Comment with two spaces after // is flagged."""
  fun name(): String => "CommentSpacing: two spaces"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "//  hello", "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestCommentSpacingInsideString is UnitTest
  """// inside a string literal is not flagged."""
  fun name(): String => "CommentSpacing: // inside string"

  fun apply(h: TestHelper) =>
    // pony-lint: off style/comment-spacing
    let sf =
      lint.SourceFile(
        "/tmp/t.pony",
        """let x = "http://example.com" """.clone() .> strip(),
        "/tmp")
    // pony-lint: on style/comment-spacing
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestCommentSpacingDocstringURL is UnitTest
  """// in a URL inside a triple-quoted docstring is not flagged."""
  fun name(): String => "CommentSpacing: // in docstring URL"

  fun apply(h: TestHelper) =>
    let src = "\"\"\"\nSee https://example.com/foo for details.\n\"\"\""
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestCommentSpacingAfterDocstring is UnitTest
  """A // comment after a docstring is still checked."""
  fun name(): String => "CommentSpacing: comment after docstring"

  fun apply(h: TestHelper) =>
    let src = "\"\"\"\nSome docstring.\n\"\"\"\n//bad"
    let sf = lint.SourceFile("/tmp/t.pony", src, "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestCommentSpacingAfterCode is UnitTest
  """Comment after code with proper spacing is OK."""
  fun name(): String => "CommentSpacing: after code with proper spacing"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        "/tmp/t.pony", "let x = 1 // a number", "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestCommentSpacingAfterCodeBadSpacing is UnitTest
  """Comment after code with bad spacing is flagged."""
  fun name(): String => "CommentSpacing: after code bad spacing"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        "/tmp/t.pony", "let x = 1 //a number", "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](11, diags(0)?.column)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestCommentSpacingNoComment is UnitTest
  """Line with no comment produces no diagnostic."""
  fun name(): String => "CommentSpacing: no comment"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile("/tmp/t.pony", "let x = 1 / 2", "/tmp")
    let diags = lint.CommentSpacing.check(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestCommentSpacingProperty is UnitTest
  """
  Property: properly spaced comments never produce diagnostics;
  badly spaced comments always do.
  """
  fun name(): String =>
    "CommentSpacing: property - good vs bad spacing"

  fun apply(h: TestHelper) ? =>
    // Good comments: "// " followed by text
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 1, max = 30,
        range = ASCIILetters) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let line: String val = "// " + content
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.CommentSpacing.check(sf)
        ph.assert_eq[USize](0, diags.size())
      })?
    // Bad comments: "//" followed by text with no space
    PonyCheck.for_all[String](
      recover val Generators.ascii(where min = 1, max = 30,
        range = ASCIILetters) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let line: String val = "//" + content
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.CommentSpacing.check(sf)
        ph.assert_eq[USize](1, diags.size())
      })?
