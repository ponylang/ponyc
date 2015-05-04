#include <gtest/gtest.h>
#include <platform.h>
#include <string>

#include <ast/lexer.h>
#include <ast/source.h>
#include <ast/token.h>

#include "util.h"

using std::string;

class LexerTest : public testing::Test
{
protected:
  string _expected;

  virtual void SetUp()
  {
    _expected = "";
  }

  // Add an expected token to our list
  void expect(size_t line, size_t pos, bool first_on_line, token_id id,
    const char* print)
  {
    char buffer[80];

    snprintf(buffer, sizeof(buffer), "%d @ %zu.%zu %s\"", id, line, pos,
      first_on_line ? "first " : "");

    _expected += string(buffer) + print + "\"\n";
  }

  // Check that the given source produces the expected tokens.
  // Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
  void test(const char* src)
  {
    source_t* source = source_open_string(src);
    lexer_t* lexer = lexer_open(source);
    token_id id = TK_LEX_ERROR;
    string actual;

    // Get tokens from lexer
    while(id != TK_EOF)
    {
      token_t* token = lexer_next(lexer);
      id = token_get_id(token);

      char buffer[80];
      snprintf(buffer, sizeof(buffer), "%d @ %zu.%zu %s\"",
        token_get_id(token),
        token_line_number(token), token_line_position(token),
        token_is_first_on_line(token) ? "first " : "");

      actual += string(buffer) + token_print(token) + "\"\n";
    }

    // Compare got to expected
    if(actual.compare(_expected) != 0)
    {
      // Fail
      printf("Expected:\n%s", _expected.c_str());
      printf("Got:\n%s", actual.c_str());
    }

    lexer_close(lexer);
    source_close(source);

    ASSERT_STREQ(_expected.c_str(), actual.c_str());
  }
};


TEST_F(LexerTest, Id)
{
  const char* src = "Foo";

  expect(1, 1, true, TK_ID, "Foo");
  expect(1, 4, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IdStartingWithKeyword)
{
  const char* src = "classFoo";

  expect(1, 1, true, TK_ID, "classFoo");
  expect(1, 9, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Keyword)
{
  const char* src = "class";

  expect(1, 1, true, TK_CLASS, "class");
  expect(1, 6, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Symbol1Char)
{
  const char* src = "+";

  expect(1, 1, true, TK_PLUS, "+");
  expect(1, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Symbol2CharStartingWith1CharSymbol)
{
  const char* src = "->+";

  expect(1, 1, true, TK_ARROW, "->");
  expect(1, 3, false, TK_PLUS, "+");
  expect(1, 4, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Symbol3CharStartingWith1CharSymbol)
{
  const char* src = "...+";

  expect(1, 1, true, TK_ELLIPSIS, "...");
  expect(1, 4, false, TK_PLUS, "+");
  expect(1, 5, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, SymbolNewAtStart)
{
  const char* src = "-";

  expect(1, 1, true, TK_MINUS_NEW, "-");
  expect(1, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, SymbolNewAfterWhitespace)
{
  const char* src = "    -";

  expect(1, 5, true, TK_MINUS_NEW, "-");
  expect(1, 6, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, SymbolNewAfterNewline)
{
  const char* src = "+\n-";

  expect(1, 1, true, TK_PLUS, "+");
  expect(2, 1, true, TK_MINUS_NEW, "-");
  expect(2, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, SymbolNotNewAfterOther)
{
  const char* src = "+-";

  expect(1, 1, true, TK_PLUS, "+");
  expect(1, 2, false, TK_MINUS, "-");
  expect(1, 3, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, EofIfEmpty)
{
  const char* src = "";

  expect(1, 1, true, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BadChar)
{
  const char* src = "$";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


// Strings

TEST_F(LexerTest, String)
{
  const char* src = "\"Foo\"";

  expect(1, 1, true, TK_STRING, "Foo");
  expect(1, 6, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringEnds)
{
  const char* src = "\"Foo\"+";

  expect(1, 1, true, TK_STRING, "Foo");
  expect(1, 6, false, TK_PLUS, "+");
  expect(1, 7, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringEscapedDoubleQuote)
{
  const char* src = "\"Foo\\\"Bar\"";

  expect(1, 1, true, TK_STRING, "Foo\"Bar");
  expect(1, 11, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringEscapedSlashAtEnd)
{
  const char* src = "\"Foo\\\\\"";

  expect(1, 1, true, TK_STRING, "Foo\\");
  expect(1, 8, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringHexEscape)
{
  const char* src = "\"Foo\\x413\"";

  expect(1, 1, true, TK_STRING, "FooA3");
  expect(1, 11, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringUnicode4Escape)
{
  const char* src = "\"Foo\\u00413\"";

  expect(1, 1, true, TK_STRING, "FooA3");
  expect(1, 13, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringUnicode6Escape)
{
  const char* src = "\"Foo\\U0000413\"";

  expect(1, 1, true, TK_STRING, "FooA3");
  expect(1, 15, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleString)
{
  const char* src = "\"\"\"Foo\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo");
  expect(1, 10, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringEnds)
{
  const char* src = "\"\"\"Foo\"\"\"+";

  expect(1, 1, true, TK_STRING, "Foo");
  expect(1, 10, false, TK_PLUS, "+");
  expect(1, 11, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringContainingDoubleQuote)
{
  const char* src = "\"\"\"Foo\"bar\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\"bar");
  expect(1, 14, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringContaining2DoubleQuotes)
{
  const char* src = "\"\"\"Foo\"\"bar\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\"\"bar");
  expect(1, 15, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringMultipleLines)
{
  const char* src = "\"\"\"Foo\nbar\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\nbar");
  expect(2, 7, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringMultipleLinesBlocksNewline)
{
  const char* src = "\"\"\"Foo\nbar\"\"\"-";

  expect(1, 1, true, TK_STRING, "Foo\nbar");
  expect(2, 7, false, TK_MINUS, "-");  // Not TK_MINUS_NEW
  expect(2, 8, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringEmpty)
{
  const char* src = "\"\"\"\"\"\"";

  expect(1, 1, true, TK_STRING, "");
  expect(1, 7, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringContainingEscape)
{
  const char* src = "\"\"\"Foo\\nbar\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\\nbar");
  expect(1, 15, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsEqualLeadingWhitespace)
{
  const char* src = "\"\"\"   Foo\n   bar\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\nbar");
  expect(2, 10, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsIncreasingLeadingWhitespace)
{
  const char* src = "\"\"\"   Foo\n     bar\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\n  bar");
  expect(2, 12, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsDecreasingLeadingWhitespace)
{
  const char* src = "\"\"\"   Foo\n  bar\"\"\"";

  expect(1, 1, true, TK_STRING, " Foo\nbar");
  expect(2, 9, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsVariableLeadingWhitespace)
{
  const char* src = "\"\"\"   Foo\n     bar\n    wom\n   bat\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\n  bar\n wom\nbat");
  expect(4, 10, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringWithLeadingEmptyLine)
{
  const char* src = "\"\"\"\nFoo\nbar\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\nbar");
  expect(3, 7, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringWithNonLeadingEmptyLine)
{
  const char* src = "\"\"\"Foo\n\nbar\"\"\"";

  expect(1, 1, true, TK_STRING, "Foo\n\nbar");
  expect(3, 7, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringUnterminated)
{
  const char* src = "\"\"\"\nFoo\nbar";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(3, 4, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringUnterminatedEndWithDoubleQuote)
{
  const char* src = "\"\"\"\nFoo\nbar\"";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(3, 5, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringUnterminatedEndWith2DoubleQuotes)
{
  const char* src = "\"\"\"\nFoo\nbar\"\"";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(3, 6, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, EmptyStringAtEndOfSource)
{
  const char* src = "\"\"";

  expect(1, 1, true, TK_STRING, "");
  expect(1, 3, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, KeywordAfterError)
{
  const char* src = "$ class";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, false, TK_CLASS, "class");
  expect(1, 8, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IdAfterError)
{
  const char* src = "$ foo";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, false, TK_ID, "foo");
  expect(1, 6, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, OperatorAfterError)
{
  const char* src = "$ ->";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, false, TK_ARROW, "->");
  expect(1, 5, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntegerAfterError)
{
  const char* src = "$ 1234";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, false, TK_INT, "1234");
  expect(1, 7, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, MultipleAfterError)
{
  const char* src = "$ while foo 1234";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, false, TK_WHILE, "while");
  expect(1, 9, false, TK_ID, "foo");
  expect(1, 13, false, TK_INT, "1234");
  expect(1, 17, false, TK_EOF, "EOF");
  DO(test(src));
}


// Numbers


TEST_F(LexerTest, Int0)
{
  const char* src = "0";

  expect(1, 1, true, TK_INT, "0");
  expect(1, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Int0Finishes)
{
  const char* src = "0-";

  expect(1, 1, true, TK_INT, "0");
  expect(1, 2, false, TK_MINUS, "-");
  expect(1, 3, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntDecimal)
{
  const char* src = "12345";

  expect(1, 1, true, TK_INT, "12345");
  expect(1, 6, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntDecimalDot)
{
  const char* src = "12345.foo";

  expect(1, 1, true, TK_INT, "12345");
  expect(1, 6, false, TK_DOT, ".");
  expect(1, 7, false, TK_ID, "foo");
  expect(1, 10, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntDecimalBadChar)
{
  const char* src = "123A";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 4, false, TK_ID, "A");
  expect(1, 5, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntBinary)
{
  const char* src = "0b10100";

  expect(1, 1, true, TK_INT, "20");
  expect(1, 8, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntBinaryIncomplete)
{
  const char* src = "0b";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntBinaryBadChar)
{
  const char* src = "0b10134";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 6, false, TK_INT, "34");
  expect(1, 8, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHex)
{
  const char* src = "0xFFFE";

  expect(1, 1, true, TK_INT, "65534");
  expect(1, 7, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexIncomplete)
{
  const char* src = "0x";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexBadChar)
{
  const char* src = "0x123EFGH";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 8, false, TK_ID, "GH");
  expect(1, 10, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexNoOverflow)
{
  const char* src = "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

  // TODO: This is wrong, that's 2^64 - 1, should be 2^128 - 1
  expect(1, 1, true, TK_INT, "18446744073709551615");
  expect(1, 35, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexOverflow)
{
  const char* src = "0x100000000000000000000000000000000";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 35, false, TK_INT, "0");
  expect(1, 36, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexDigitOverflow)
{
  const char* src = "0x111111111111111111111111111111112";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 35, false, TK_INT, "2");
  expect(1, 36, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntNoOctal)
{
  const char* src = "0100";

  expect(1, 1, true, TK_INT, "100");
  expect(1, 5, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatDotOnly)
{
  const char* src = "1.234";

  expect(1, 1, true, TK_FLOAT, "1.234");
  expect(1, 6, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatEOnly)
{
  const char* src = "1e3";

  expect(1, 1, true, TK_FLOAT, "1000.0");
  expect(1, 4, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatNegativeE)
{
  const char* src = "1e-2";

  expect(1, 1, true, TK_FLOAT, "0.01");
  expect(1, 5, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatPositiveE)
{
  const char* src = "1e+2";

  expect(1, 1, true, TK_FLOAT, "100.0");
  expect(1, 5, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatDotAndE)
{
  const char* src = "1.234e2";

  expect(1, 1, true, TK_FLOAT, "123.4");
  expect(1, 8, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatLeading0)
{
  const char* src = "01.234e2";

  expect(1, 1, true, TK_FLOAT, "123.4");
  expect(1, 9, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatExpLeading0)
{
  const char* src = "1.234e02";

  expect(1, 1, true, TK_FLOAT, "123.4");
  expect(1, 9, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatIllegalExp)
{
  const char* src = "1.234efg";

  expect(1, 1, true, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 7, false, TK_ID, "fg");
  expect(1, 9, false, TK_EOF, "EOF");
  DO(test(src));
}


// Comments

TEST_F(LexerTest, LineComment)
{
  const char* src = "// Comment\n+";

  expect(2, 1, true, TK_PLUS, "+");
  expect(2, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LineCommentWith3Slashes)
{
  const char* src = "/// Comment\n+";

  expect(2, 1, true, TK_PLUS, "+");
  expect(2, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LineCommentContainingBlockCommentStart)
{
  const char* src = "// Foo /* Bar\n+";

  expect(2, 1, true, TK_PLUS, "+");
  expect(2, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LineCommentEndingWithSlash)
{
  const char* src = "// Comment/\n+";

  expect(2, 1, true, TK_PLUS, "+");
  expect(2, 2, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockComment)
{
  const char* src = "/* Comment */+";

  expect(1, 14, true, TK_PLUS, "+");
  expect(1, 15, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentOverMultipleLines)
{
  const char* src = "/* Foo\nBar */+";

  expect(2, 7, true, TK_PLUS, "+");
  expect(2, 8, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentStartingWith2Stars)
{
  const char* src = "/** Comment */+";

  expect(1, 15, true, TK_PLUS, "+");
  expect(1, 16, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentEndingWith2Stars)
{
  const char* src = "/* Comment **/+";

  expect(1, 15, true, TK_PLUS, "+");
  expect(1, 16, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNested)
{
  const char* src = "/* Comment /* Inner */ */+";

  expect(1, 26, true, TK_PLUS, "+");
  expect(1, 27, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedStartingWith2Slashes)
{
  const char* src = "/* Comment //* Inner */ */+";

  expect(1, 27, true, TK_PLUS, "+");
  expect(1, 28, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedStartingWith2Stars)
{
  const char* src = "/* Comment /** Inner */ */+";

  expect(1, 27, true, TK_PLUS, "+");
  expect(1, 28, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedEndingWith2Stars)
{
  const char* src = "/* Comment /* Inner **/ */+";

  expect(1, 27, true, TK_PLUS, "+");
  expect(1, 28, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedEndingWith2Slashes)
{
  const char* src = "/* Comment /* Inner *// */+";

  expect(1, 27, true, TK_PLUS, "+");
  expect(1, 28, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedInnerEmpty)
{
  const char* src = "/* Comment /**/ */+";

  expect(1, 19, true, TK_PLUS, "+");
  expect(1, 20, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedInner3Stars)
{
  const char* src = "/* Comment /***/ */+";

  expect(1, 20, true, TK_PLUS, "+");
  expect(1, 21, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedStartsNoWhitespace)
{
  const char* src = "/*/* Inner */ */+";

  expect(1, 17, true, TK_PLUS, "+");
  expect(1, 18, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedEndsNoWhitespace)
{
  const char* src = "/* Comment /* Inner */*/+";

  expect(1, 25, true, TK_PLUS, "+");
  expect(1, 26, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentContainsLineComment)
{
  const char* src = "/* Comment // */+";

  expect(1, 17, true, TK_PLUS, "+");
  expect(1, 18, false, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedDeeper)
{
  const char* src = "/* /* /* */ /* */ */ */+";

  expect(1, 24, true, TK_PLUS, "+");
  expect(1, 25, false, TK_EOF, "EOF");
  DO(test(src));
}


// Non-token tests

TEST_F(LexerTest, IsAbstractKeyword)
{
  ASSERT_EQ(TK_PROGRAM, lexer_is_abstract_keyword("program"));
  ASSERT_EQ(TK_CASE, lexer_is_abstract_keyword("case"));
  ASSERT_EQ(TK_LEX_ERROR, lexer_is_abstract_keyword("foo"));
  ASSERT_EQ(TK_LEX_ERROR, lexer_is_abstract_keyword("+"));
}


TEST_F(LexerTest, Print)
{
  ASSERT_STREQ("match", lexer_print(TK_MATCH));
  ASSERT_STREQ("program", lexer_print(TK_PROGRAM));
  ASSERT_STREQ("+", lexer_print(TK_PLUS));
  ASSERT_STREQ(">=", lexer_print(TK_GE));
  ASSERT_EQ((void*)NULL, lexer_print(TK_ID));
  ASSERT_EQ((void*)NULL, lexer_print(TK_INT));
}
