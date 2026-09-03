use "pony_test"
use "pony_check"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestLineLengthExactly80 is UnitTest
  """Line of exactly 80 characters does not trigger."""
  fun name(): String => "LineLength: exactly 80 chars -> no diagnostic"

  fun apply(h: TestHelper) =>
    let line = recover val String .> append("a".mul(80)) end
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthOver80 is UnitTest
  """Breakable line of 81+ characters is flagged at column 81."""
  fun name(): String => "LineLength: over 80 chars -> diagnostic"

  fun apply(h: TestHelper) =>
    // Space at col 80 so neither word crosses the boundary.
    let line: String val =
      recover val
        String
          .> append("a".mul(79))
          .> append(" ")
          .> append("a".mul(15))
      end
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
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
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthEmptyLine is UnitTest
  """Empty lines don't trigger."""
  fun name(): String => "LineLength: empty line -> no diagnostic"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile("/tmp/t.pony", "", "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthProperty is UnitTest
  """
  Property: lines <= 80 codepoints never produce diagnostics;
  breakable lines > 80 codepoints (where no single word crosses
  column 80) always do.
  """
  fun name(): String =>
    "LineLength: property - short lines OK, long lines flagged"

  fun apply(h: TestHelper) ? =>
    // Lines up to 80 chars never produce diagnostics
    PonyCheck.for_all[String](
      recover val Generators.ascii(where from = 0, to = 80,
        range = ASCIIPrintable) end, h)(
      {(content: String, ph: PropertyHelper) =>
        let line = content.clone()
        line.remove("\n")
        let safe_line: String val = consume line
        if safe_line.codepoints() <= 80 then
          let sf = lint.SourceFile("/tmp/t.pony", safe_line, "/tmp")
          let diags = lint.LineLength.check_text(sf)
          ph.assert_eq[USize](0, diags.size())
        end
      })?
    // Space at column 80 ensures neither word crosses the boundary.
    PonyCheck.for_all[USize](
      recover val Generators.usize(where from = 81, to = 120) end, h)(
      {(n: USize, ph: PropertyHelper) =>
        let line: String val =
          recover val
            String
              .> append("a".mul(79))
              .> append(" ")
              .> append("b".mul(n - 80))
          end
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.LineLength.check_text(sf)
        ph.assert_eq[USize](1, diags.size())
      })?

class \nodoc\ _TestLineLengthStringNoSpacesFlagged is UnitTest
  """
  No-space string crossing column 80 is flagged when not among the
  first two words — the line can be broken before the string.
  """
  fun name(): String =>
    "LineLength: no-space string crossing col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // `    let x = "` = 13 chars prefix, then 70 'a's + `"`
    // The string word `"aaa..."` is at word position 4 (let, x, =, "aaa...").
    let line: String val =
      recover val
        String
          .> append("    let x = \"")
          .> append("a".mul(70))
          .> append("\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringNotExemptSpaces is UnitTest
  """
  String with spaces crossing column 80 is flagged — no word among
  the first two crosses the boundary.
  """
  fun name(): String =>
    "LineLength: string with spaces crossing col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // Prefix is 13 chars. The string is at word position 4 (let, x,
    // =, "aaa..."). Not among first two words.
    let line: String val =
      recover val
        String
          .> append("    let x = \"")
          .> append("a".mul(67))
          .> append(" ")
          .> append("a".mul(10))
          .> append("\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringBeforeCol80 is UnitTest
  """String ends before col 80; line is long from trailing content."""
  fun name(): String =>
    "LineLength: string before col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // Short string then breakable trailing content. Prefix
    // `    let x = "short" + ` is 22 chars. 57 a's fill cols 23-79,
    // space at col 80, 10 a's at cols 81-90. No word crosses col 80.
    let line: String val =
      recover val
        String
          .> append("    let x = \"short\" + ")
          .> append("a".mul(57))
          .> append(" ")
          .> append("a".mul(10))
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringAfterCol80 is UnitTest
  """String starts after col 80 (doesn't cross the boundary)."""
  fun name(): String =>
    "LineLength: string starts after col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // Breakable code then a short string after col 80. 79 a's fill
    // cols 1-79, space at col 80, `aa"url"` at cols 81-87.
    let line: String val =
      recover val
        String
          .> append("a".mul(79))
          .> append(" aa\"url\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
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
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthDocstringNoExempt is UnitTest
  """
  Long line inside a triple-quoted block with a no-space string is
  flagged when checked via text-only check_text (no AST context).
  The string is at word position 4 (let, x, =, string), so the
  first-two-words exemption does not apply.
  """
  fun name(): String =>
    "LineLength: triple-quote content with no-space string -> flagged"

  fun apply(h: TestHelper) =>
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
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthMultipleStringsFlagged is UnitTest
  """
  Two strings on one line; the long one is at word position 3+, so
  the line is flagged.
  """
  fun name(): String =>
    "LineLength: long string at word 3+ -> flagged"

  fun apply(h: TestHelper) =>
    // `    f("a` is word 1, `b",` is word 2, `"xxx...")` is word 3.
    let line: String val =
      recover val
        String
          .> append("    f(\"a b\", \"")
          .> append("x".mul(70))
          .> append("\")")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthEscapedQuotesFlagged is UnitTest
  """
  String with escaped quotes inside, crossing col 80 — flagged
  because the string is at word position 4.
  """
  fun name(): String =>
    "LineLength: escaped quotes in string -> flagged"

  fun apply(h: TestHelper) =>
    // String at word position 4 (let, x, =, "abc\"def...").
    let line: String val =
      recover val
        String
          .> append("    let x = \"abc\\\"def")
          .> append("g".mul(60))
          .> append("\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthTripleQuoteLineNoExempt is UnitTest
  """Long line containing triple-quote delimiter is not eligible."""
  fun name(): String =>
    "LineLength: triple-quote delimiter line -> flagged"

  fun apply(h: TestHelper) =>
    // `  """` is 5 chars. 74 a's fill cols 6-79, space at col 80,
    // 5 a's at cols 81-85. No word crosses col 80.
    let line: String val =
      recover val
        String
          .> append("  \"\"\"")
          .> append("a".mul(74))
          .> append(" ")
          .> append("a".mul(5))
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringUTF8Flagged is UnitTest
  """Multi-byte UTF-8 in string at word position 4 is flagged."""
  fun name(): String =>
    "LineLength: UTF-8 string crossing col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // String at word position 4 (let, x, =, "éé..."). 13 prefix +
    // 35 é + 35 a + 1 closing quote = 84 codepoints.
    let line: String val =
      recover val
        String
          .> append("    let x = \"")
          .> append("é".mul(35))
          .> append("a".mul(35))
          .> append("\"")
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthStringFlaggedWhenDeepProperty is UnitTest
  """
  Property: a no-space string at word position 4 is always flagged
  regardless of length. The string crosses column 80 but is not among
  the first two words (let, x, =, string).
  """
  fun name(): String =>
    "LineLength: property - deep no-space strings always flagged"

  fun apply(h: TestHelper) ? =>
    let gen =
      recover val Generators.usize(where from = 67, to = 200) end
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
        let diags = lint.LineLength.check_text(sf)
        ph.assert_eq[USize](1, diags.size())
      })?

class \nodoc\ _TestLineLengthStringFlaggedProperty is UnitTest
  """
  Property: a space-containing string crossing column 80 is always
  flagged — no word among the first two crosses the boundary.
  """
  fun name(): String =>
    "LineLength: property - breakable strings always flagged"

  fun apply(h: TestHelper) ? =>
    let gen =
      recover val Generators.usize(where from = 1, to = 134) end
    PonyCheck.for_all[USize](gen, h)(
      {(n2: USize, ph: PropertyHelper) =>
        // Prefix `    let x = "` = 13 chars. The string is at word
        // position 4 (let, x, =, "aaa...").
        let line: String val =
          recover val
            String
              .> append("    let x = \"")
              .> append("a".mul(67))
              .> append(" ")
              .> append("a".mul(n2))
              .> append("\"")
          end
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.LineLength.check_text(sf)
        ph.assert_eq[USize](1, diags.size())
      })?

class \nodoc\ _TestLineLengthASTStringLiteralExempt is UnitTest
  """
  Lines inside a triple-quoted string literal (not a docstring) are
  exempt from the 80-column check via the AST-based check_module.
  """
  fun name(): String =>
    "LineLength: AST string literal lines exempt"

  fun apply(h: TestHelper) ? =>
    let long_content = recover val String .> append("a".mul(100)) end
    let source: String val =
      recover val
        String
          .> append("primitive Foo\n")
          .> append("  fun apply(): String =>\n")
          .> append("    let x: String =\n")
          .> append("      \"\"\"\n")
          .> append("      " + long_content + "\n")
          .> append("      \"\"\"\n")
          .> append("    x")
      end
    (let program, let sf) = _ASTTestHelper.compile(h, source)?
    let mod_ast =
      (program.package() as ast.Package).module()
        as ast.Module
    let diags = lint.LineLength.check_module(mod_ast.ast, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthASTDocstringNotExempt is UnitTest
  """
  Lines inside a docstring are NOT exempt from the 80-column check
  when the overflow is caused by breakable prose (no single word
  crosses column 80).
  """
  fun name(): String =>
    "LineLength: AST docstring lines not exempt"

  fun apply(h: TestHelper) ? =>
    // 77 a's + space + 23 a's = 101 chars. With 2-space indent the
    // space falls at col 80, so neither word crosses the boundary.
    let long_content =
      recover val
        String
          .> append("a".mul(77))
          .> append(" ")
          .> append("a".mul(23))
      end
    let source: String val =
      recover val
        String
          .> append("primitive Foo\n")
          .> append("  \"\"\"\n")
          .> append("  " + long_content + "\n")
          .> append("  \"\"\"\n")
          .> append("  fun apply(): None => None")
      end
    (let program, let sf) = _ASTTestHelper.compile(h, source)?
    let mod_ast =
      (program.package() as ast.Package).module()
        as ast.Module
    let diags = lint.LineLength.check_module(mod_ast.ast, sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](3, diags(0)?.line)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestLineLengthASTMethodDocstringNotExempt is UnitTest
  """
  Lines inside a method-body docstring are NOT exempt when the
  overflow is breakable prose. The AST identifies child 0 of the body
  TK_SEQ under a method as a docstring.
  """
  fun name(): String =>
    "LineLength: AST method-body docstring lines not exempt"

  fun apply(h: TestHelper) ? =>
    // 75 a's + space + 25 a's = 101 chars. With 4-space indent the
    // space falls at col 80, so neither word crosses the boundary.
    let long_content =
      recover val
        String
          .> append("a".mul(75))
          .> append(" ")
          .> append("a".mul(25))
      end
    let source: String val =
      recover val
        String
          .> append("primitive Foo\n")
          .> append("  fun apply(): None =>\n")
          .> append("    \"\"\"\n")
          .> append("    " + long_content + "\n")
          .> append("    \"\"\"\n")
          .> append("    None")
      end
    (let program, let sf) = _ASTTestHelper.compile(h, source)?
    let mod_ast =
      (program.package() as ast.Package).module()
        as ast.Module
    let diags = lint.LineLength.check_module(mod_ast.ast, sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](4, diags(0)?.line)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestLineLengthASTModuleDocstringNotExempt is UnitTest
  """
  Lines inside a module-level docstring are NOT exempt when the
  overflow is breakable prose. The AST identifies child 0 of the
  module as the package docstring.
  """
  fun name(): String =>
    "LineLength: AST module-level docstring lines not exempt"

  fun apply(h: TestHelper) ? =>
    // 79 a's + space + 21 a's = 101 chars. No indent, so the space
    // falls at col 80 and neither word crosses the boundary.
    let long_content =
      recover val
        String
          .> append("a".mul(79))
          .> append(" ")
          .> append("a".mul(21))
      end
    let source: String val =
      recover val
        String
          .> append("\"\"\"\n")
          .> append(long_content + "\n")
          .> append("\"\"\"\n")
          .> append("\n")
          .> append("primitive Foo")
      end
    (let program, let sf) = _ASTTestHelper.compile(h, source)?
    let mod_ast =
      (program.package() as ast.Package).module()
        as ast.Module
    let diags = lint.LineLength.check_module(mod_ast.ast, sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](2, diags(0)?.line)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestLineLengthASTDocstringQuotedIdExempt is UnitTest
  """
  A docstring line with a quoted identifier (no spaces) crossing column
  80 is exempt. The quoted identifier is an unbreakable word.
  """
  fun name(): String =>
    "LineLength: AST docstring quoted identifier -> exempt"

  fun apply(h: TestHelper) ? =>
    let long_name =
      recover val String .> append("a".mul(75)) end
    let source: String val =
      recover val
        String
          .> append("primitive Foo\n")
          .> append("  \"\"\"\n")
          .> append("  Wraps \"" + long_name + "\" type.\n")
          .> append("  \"\"\"\n")
          .> append("  fun apply(): None => None")
      end
    (let program, let sf) = _ASTTestHelper.compile(h, source)?
    let mod_ast =
      (program.package() as ast.Package).module()
        as ast.Module
    let diags = lint.LineLength.check_module(mod_ast.ast, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthASTDocstringURLExempt is UnitTest
  """
  A docstring line with a URL crossing column 80 is exempt. The URL
  is an unbreakable word, even though docstring prose is normally held
  to the 80-column limit.
  """
  fun name(): String =>
    "LineLength: AST docstring URL -> exempt"

  fun apply(h: TestHelper) ? =>
    // The markdown link token `[RFC-0038](...md)` is 78 chars and
    // starts at col 7 (after `  See `), ending at col 84 — it crosses
    // col 80, so the word exemption applies.
    let source: String val =
      recover val
        String
          .> append("primitive Foo\n")
          .> append("  \"\"\"\n")
          .> append("  See [RFC-0038](https://github.com/")
          .> append("ponylang/rfcs/blob/main/text/")
          .> append("0038-cli-format.md)")
          .> append(" for more background.\n")
          .> append("  \"\"\"\n")
          .> append("  fun apply(): None => None")
      end
    (let program, let sf) = _ASTTestHelper.compile(h, source)?
    let mod_ast =
      (program.package() as ast.Package).module()
        as ast.Module
    let diags = lint.LineLength.check_module(mod_ast.ast, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthASTMixedDocstringAndLiteral is UnitTest
  """
  A file with both a docstring and a string literal, each containing
  long lines. Only the docstring's long line should be flagged (the
  string literal is exempt via the AST). The docstring content uses
  breakable prose so the word exemption does not apply.
  """
  fun name(): String =>
    "LineLength: AST mixed docstring and literal"

  fun apply(h: TestHelper) ? =>
    // Docstring at 2-space indent: 77 a's + space + 23 a's = 101 chars.
    // Space at col 80 so neither word crosses the boundary.
    let ds_content =
      recover val
        String
          .> append("a".mul(77))
          .> append(" ")
          .> append("a".mul(23))
      end
    // String literal content can be anything — it's exempt via AST.
    let lit_content =
      recover val String .> append("a".mul(100)) end
    let source: String val =
      recover val
        String
          .> append("primitive Foo\n")
          .> append("  \"\"\"\n")
          .> append("  " + ds_content + "\n")
          .> append("  \"\"\"\n")
          .> append("  fun apply(): String =>\n")
          .> append("    let x: String =\n")
          .> append("      \"\"\"\n")
          .> append("      " + lit_content + "\n")
          .> append("      \"\"\"\n")
          .> append("    x")
      end
    (let program, let sf) = _ASTTestHelper.compile(h, source)?
    let mod_ast =
      (program.package() as ast.Package).module()
        as ast.Module
    let diags = lint.LineLength.check_module(mod_ast.ast, sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[USize](3, diags(0)?.line)
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestLineLengthWordExemptCommentURL is UnitTest
  """
  A URL as the only content in a comment is exempt. The URL is word 2,
  so the first-two-words exemption applies.
  """
  fun name(): String =>
    "LineLength: URL alone in comment -> exempt"

  fun apply(h: TestHelper) =>
    // URL is word 2 (after //), 78 chars, starts at col 4, ends at
    // col 81. Crosses col 80. Total line: 81 codepoints.
    let line: String val =
      recover val
        String
          .> append("// https://docs.microsoft.com")
          .> append("/de-de/windows/desktop/Debug/")
          .> append("system-error-codes-list")
      end
    h.assert_eq[USize](81, line.codepoints())
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthWordFlaggedCommentURL is UnitTest
  """
  A URL in a comment with text before it (word 3+) is flagged.
  The text and URL can go on separate lines.
  """
  fun name(): String =>
    "LineLength: URL after text in comment -> flagged"

  fun apply(h: TestHelper) =>
    // URL is word 3 (after // and see:). Total: 86 codepoints.
    let line: String val =
      recover val
        String
          .> append("// see: https://docs.microsoft.com")
          .> append("/de-de/windows/desktop/Debug/")
          .> append("system-error-codes-list")
      end
    h.assert_eq[USize](86, line.codepoints())
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthWordThirdWordCrossingFlagged is UnitTest
  """
  Word 3 crosses column 80 but the exemption only covers the first
  two words, so the line is flagged.
  """
  fun name(): String =>
    "LineLength: word 3 crosses col 80 -> flagged"

  fun apply(h: TestHelper) =>
    // "ab cd " (6 chars) + 77 e's = 83 codepoints.
    // Word 1 = "ab" (cols 1-2), word 2 = "cd" (cols 4-5),
    // word 3 = 77 e's (cols 7-83). Word 3 crosses col 80,
    // but word_count > 2 so it returns false. Flagged.
    let line: String val =
      recover val
        String
          .> append("ab cd ")
          .> append("e".mul(77))
      end
    h.assert_eq[USize](83, line.codepoints())
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthWordExemptLongWord is UnitTest
  """
  A long word as the second word in a comment crossing column 80 is
  exempt.
  """
  fun name(): String =>
    "LineLength: long word as word 2 crossing col 80 -> exempt"

  fun apply(h: TestHelper) =>
    let line: String val =
      recover val
        String
          .> append("// ")
          .> append("a".mul(80))
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthWordNotExemptShortWords is UnitTest
  """
  A line over 80 columns where every word ends at or before column
  80 is flagged. No single word crosses the boundary.
  """
  fun name(): String =>
    "LineLength: short words totaling > 80 -> flagged"

  fun apply(h: TestHelper) =>
    let line: String val =
      recover val
        String
          .> append("a".mul(79))
          .> append(" ")
          .> append("b".mul(10))
      end
    h.assert_eq[USize](90, line.codepoints())
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthWordAtCol80Exempt is UnitTest
  """
  A word starting at exactly column 80 and extending past it is exempt.
  """
  fun name(): String =>
    "LineLength: word starting at col 80 -> exempt"

  fun apply(h: TestHelper) =>
    // 79 spaces + "ab": word starts at col 80, ends at col 81.
    let line: String val =
      recover val
        String
          .> append(" ".mul(79))
          .> append("ab")
      end
    h.assert_eq[USize](81, line.codepoints())
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestLineLengthWordAtCol81NotExempt is UnitTest
  """
  A word starting at column 81 does not cross the boundary; the line
  is flagged.
  """
  fun name(): String =>
    "LineLength: word starting at col 81 -> flagged"

  fun apply(h: TestHelper) =>
    // 80 spaces + "ab": word starts at col 81, does not cross col 80.
    let line: String val =
      recover val
        String
          .> append(" ".mul(80))
          .> append("ab")
      end
    h.assert_eq[USize](82, line.codepoints())
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](1, diags.size())

class \nodoc\ _TestLineLengthWordExemptProperty is UnitTest
  """
  Property: a line with a single word crossing column 80 is always
  exempt, regardless of the word's length.
  """
  fun name(): String =>
    "LineLength: property - single word crossing col 80 exempt"

  fun apply(h: TestHelper) ? =>
    let gen =
      recover val Generators.usize(where from = 81, to = 200) end
    PonyCheck.for_all[USize](gen, h)(
      {(n: USize, ph: PropertyHelper) =>
        let line: String val =
          recover val String .> append("a".mul(n)) end
        ph.assert_true(line.codepoints() > 80)
        let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
        let diags = lint.LineLength.check_text(sf)
        ph.assert_eq[USize](0, diags.size())
      })?

class \nodoc\ _TestLineLengthWordExemptUTF8 is UnitTest
  """
  A comment containing a long multi-byte UTF-8 word crossing column 80
  is exempt.
  """
  fun name(): String =>
    "LineLength: UTF-8 word in comment crossing col 80 -> exempt"

  fun apply(h: TestHelper) =>
    // Each char is 3 bytes (U+3042 hiragana "a"), 1 codepoint.
    // "// " is 3 chars; 78 hiragana chars puts last char at col 81.
    let hiragana = String.from_utf32(0x3042)
    let line: String val =
      recover val
        String
          .> append("// ")
          .> append(hiragana.mul(78))
      end
    h.assert_true(line.codepoints() > 80)
    let sf = lint.SourceFile("/tmp/t.pony", line, "/tmp")
    let diags = lint.LineLength.check_text(sf)
    h.assert_eq[USize](0, diags.size())
