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
  void expect(size_t line, size_t pos, token_id id, const char* print)
  {
    char buffer[80];

    snprintf(buffer, sizeof(buffer), "%d @ " __zu "." __zu "\"",
      id, line, pos);
    _expected += string(buffer) + print + "\"\n";
  }

  // Check that the given source produces the expected tokens.
  // Errors are checked with ASSERTs, call in ASSERT_NO_FATAL_FAILURE.
  void test(const char* src)
  {
    errors_t* errors = errors_alloc();
    source_t* source = source_open_string(src);
    lexer_t* lexer = lexer_open(source, errors);
    token_id id = TK_LEX_ERROR;
    string actual;

    // Get tokens from lexer
    while(id != TK_EOF)
    {
      token_t* token = lexer_next(lexer);
      id = token_get_id(token);

      char buffer[80];
      snprintf(buffer, sizeof(buffer), "%d @ " __zu "." __zu "\"",
        token_get_id(token),
        token_line_number(token), token_line_position(token));

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
    errors_free(errors);

    ASSERT_STREQ(_expected.c_str(), actual.c_str());
  }
};


TEST_F(LexerTest, Id)
{
  const char* src = "Foo";

  expect(1, 1, TK_ID, "Foo");
  expect(1, 4, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IdStartingWithKeyword)
{
  const char* src = "classFoo";

  expect(1, 1, TK_ID, "classFoo");
  expect(1, 9, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Keyword)
{
  const char* src = "class";

  expect(1, 1, TK_CLASS, "class");
  expect(1, 6, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Symbol1Char)
{
  const char* src = "+";

  expect(1, 1, TK_PLUS, "+");
  expect(1, 2, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Symbol2CharStartingWith1CharSymbol)
{
  const char* src = "->+";

  expect(1, 1, TK_ARROW, "->");
  expect(1, 3, TK_PLUS, "+");
  expect(1, 4, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Symbol3CharStartingWith1CharSymbol)
{
  const char* src = "...+";

  expect(1, 1, TK_ELLIPSIS, "...");
  expect(1, 4, TK_PLUS, "+");
  expect(1, 5, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, SymbolNewAtStart)
{
  const char* src = "-";

  expect(1, 1, TK_MINUS_NEW, "-");
  expect(1, 2, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, SymbolNewAfterWhitespace)
{
  const char* src = "    -";

  expect(1, 5, TK_MINUS_NEW, "-");
  expect(1, 6, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, SymbolNewAfterNewline)
{
  const char* src = "+\n-";

  expect(1, 1, TK_PLUS, "+");
  expect(2, 1, TK_MINUS_NEW, "-");
  expect(2, 2, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, SymbolNotNewAfterOther)
{
  const char* src = "+-";

  expect(1, 1, TK_PLUS, "+");
  expect(1, 2, TK_MINUS, "-");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, EofIfEmpty)
{
  const char* src = "";

  expect(1, 1, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BadChar)
{
  const char* src = "$";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 2, TK_EOF, "EOF");
  DO(test(src));
}


// First on line symbols

TEST_F(LexerTest, Lparen)
{
  const char* src = "+(";

  expect(1, 1, TK_PLUS, "+");
  expect(1, 2, TK_LPAREN, "(");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LparenNew)
{
  const char* src = "(+";

  expect(1, 1, TK_LPAREN_NEW, "(");
  expect(1, 2, TK_PLUS, "+");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LparenAfterBlockComment)
{
  const char* src = "/* comment */(+";

  expect(1, 14, TK_LPAREN, "(");
  expect(1, 15, TK_PLUS, "+");
  expect(1, 16, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Lsquare)
{
  const char* src = "+[";

  expect(1, 1, TK_PLUS, "+");
  expect(1, 2, TK_LSQUARE, "[");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LsquareNew)
{
  const char* src = "[+";

  expect(1, 1, TK_LSQUARE_NEW, "[");
  expect(1, 2, TK_PLUS, "+");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LsquareAfterBlockComment)
{
  const char* src = "/* comment */[+";

  expect(1, 14, TK_LSQUARE, "[");
  expect(1, 15, TK_PLUS, "+");
  expect(1, 16, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Minus)
{
  const char* src = "+-";

  expect(1, 1, TK_PLUS, "+");
  expect(1, 2, TK_MINUS, "-");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, MinusNew)
{
  const char* src = "-+";

  expect(1, 1, TK_MINUS_NEW, "-");
  expect(1, 2, TK_PLUS, "+");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, MinusAfterBlockComment)
{
  const char* src = "/* comment */-+";

  expect(1, 14, TK_MINUS, "-");
  expect(1, 15, TK_PLUS, "+");
  expect(1, 16, TK_EOF, "EOF");
  DO(test(src));
}


// Strings

TEST_F(LexerTest, String)
{
  const char* src = "\"Foo\"";

  expect(1, 1, TK_STRING, "Foo");
  expect(1, 6, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringEnds)
{
  const char* src = "\"Foo\"+";

  expect(1, 1, TK_STRING, "Foo");
  expect(1, 6, TK_PLUS, "+");
  expect(1, 7, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringEscapedDoubleQuote)
{
  const char* src = "\"Foo\\\"Bar\"";

  expect(1, 1, TK_STRING, "Foo\"Bar");
  expect(1, 11, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringEscapedSlashAtEnd)
{
  const char* src = "\"Foo\\\\\"";

  expect(1, 1, TK_STRING, "Foo\\");
  expect(1, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringHexEscape)
{
  const char* src = "\"Foo\\x413\"";

  expect(1, 1, TK_STRING, "FooA3");
  expect(1, 11, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringUnicode4Escape)
{
  const char* src = "\"Foo\\u00413\"";

  expect(1, 1, TK_STRING, "FooA3");
  expect(1, 13, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, StringUnicode6Escape)
{
  const char* src = "\"Foo\\U0000413\"";

  expect(1, 1, TK_STRING, "FooA3");
  expect(1, 15, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleString)
{
  const char* src = "\"\"\"Foo\"\"\"";

  expect(1, 1, TK_STRING, "Foo");
  expect(1, 10, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringEnds)
{
  const char* src = "\"\"\"Foo\"\"\"+";

  expect(1, 1, TK_STRING, "Foo");
  expect(1, 10, TK_PLUS, "+");
  expect(1, 11, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringContainingDoubleQuote)
{
  const char* src = "\"\"\"Foo\"bar\"\"\"";

  expect(1, 1, TK_STRING, "Foo\"bar");
  expect(1, 14, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringContaining2DoubleQuotes)
{
  const char* src = "\"\"\"Foo\"\"bar\"\"\"";

  expect(1, 1, TK_STRING, "Foo\"\"bar");
  expect(1, 15, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringEndingWithExtraDoubleQuotes)
{
  const char* src = "\"\"\"Foobar\"\"\"\"";

  expect(1, 1, TK_STRING, "Foobar\"");
  expect(1, 14, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringEndingWith3ExtraDoubleQuotes)
{
  const char* src = "\"\"\"Foobar\"\"\"\"\"\"";

  expect(1, 1, TK_STRING, "Foobar\"\"\"");
  expect(1, 16, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringEndingWith4ExtraDoubleQuotes)
{
  const char* src = "\"\"\"Foobar\"\"\"\"\"\"\"";

  expect(1, 1, TK_STRING, "Foobar\"\"\"\"");
  expect(1, 17, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringMultipleLines)
{
  const char* src = "\"\"\"Foo\nbar\"\"\"";

  expect(1, 1, TK_STRING, "Foo\nbar");
  expect(2, 7, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringMultipleLinesBlocksNewline)
{
  const char* src = "\"\"\"Foo\nbar\"\"\"-";

  expect(1, 1, TK_STRING, "Foo\nbar");
  expect(2, 7, TK_MINUS, "-");  // Not TK_MINUS_NEW
  expect(2, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringEmpty)
{
  const char* src = "\"\"\"\"\"\"";

  expect(1, 1, TK_STRING, "");
  expect(1, 7, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringContainingEscape)
{
  const char* src = "\"\"\"Foo\\nbar\"\"\"";

  expect(1, 1, TK_STRING, "Foo\\nbar");
  expect(1, 15, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsEqualLeadingWhitespace)
{
  const char* src = "\"\"\"   Foo\n   bar\"\"\"";

  expect(1, 1, TK_STRING, "Foo\nbar");
  expect(2, 10, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsIncreasingLeadingWhitespace)
{
  const char* src = "\"\"\"   Foo\n     bar\"\"\"";

  expect(1, 1, TK_STRING, "Foo\n  bar");
  expect(2, 12, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsDecreasingLeadingWhitespace)
{
  const char* src = "\"\"\"   Foo\n  bar\"\"\"";

  expect(1, 1, TK_STRING, " Foo\nbar");
  expect(2, 9, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsVariableLeadingWhitespace)
{
  const char* src = "\"\"\"   Foo\n     bar\n    wom\n   bat\"\"\"";

  expect(1, 1, TK_STRING, "Foo\n  bar\n wom\nbat");
  expect(4, 10, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringStripsVariableLeadingWhitespaceWithBlankLines)
{
  const char* src = "\"\"\"   Foo\n     bar\n    \n   bat\n \"\"\"";

  expect(1, 1, TK_STRING, "Foo\n  bar\n \nbat\n");
  expect(5, 5, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringWithLeadingEmptyLine)
{
  const char* src = "\"\"\"\nFoo\nbar\"\"\"";

  expect(1, 1, TK_STRING, "Foo\nbar");
  expect(3, 7, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringWithNonLeadingEmptyLine)
{
  const char* src = "\"\"\"Foo\n\nbar\"\"\"";

  expect(1, 1, TK_STRING, "Foo\n\nbar");
  expect(3, 7, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringUnterminated)
{
  const char* src = "\"\"\"\nFoo\nbar";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(3, 4, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringUnterminatedEndWithDoubleQuote)
{
  const char* src = "\"\"\"\nFoo\nbar\"";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(3, 5, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, TripleStringUnterminatedEndWith2DoubleQuotes)
{
  const char* src = "\"\"\"\nFoo\nbar\"\"";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(3, 6, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, EmptyStringAtEndOfSource)
{
  const char* src = "\"\"";

  expect(1, 1, TK_STRING, "");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


// Symbols after errors

TEST_F(LexerTest, KeywordAfterError)
{
  const char* src = "$ class";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, TK_CLASS, "class");
  expect(1, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IdAfterError)
{
  const char* src = "$ foo";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, TK_ID, "foo");
  expect(1, 6, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, OperatorAfterError)
{
  const char* src = "$ ->";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, TK_ARROW, "->");
  expect(1, 5, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntegerAfterError)
{
  const char* src = "$ 1234";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, TK_INT, "1234");
  expect(1, 7, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, MultipleAfterError)
{
  const char* src = "$ while foo 1234";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, TK_WHILE, "while");
  expect(1, 9, TK_ID, "foo");
  expect(1, 13, TK_INT, "1234");
  expect(1, 17, TK_EOF, "EOF");
  DO(test(src));
}


// Numbers

TEST_F(LexerTest, Int0)
{
  const char* src = "0";

  expect(1, 1, TK_INT, "0");
  expect(1, 2, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, Int0Finishes)
{
  const char* src = "0-";

  expect(1, 1, TK_INT, "0");
  expect(1, 2, TK_MINUS, "-");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntDecimal)
{
  const char* src = "12345";

  expect(1, 1, TK_INT, "12345");
  expect(1, 6, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntDecimalWithSeparators)
{
  const char* src = "12_3_45";

  expect(1, 1, TK_INT, "12345");
  expect(1, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntDecimalWithTrailingSeparator)
{
  const char* src = "12_3_45_";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 9, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntDecimalDot)
{
  const char* src = "12345.foo";

  expect(1, 1, TK_INT, "12345");
  expect(1, 6, TK_DOT, ".");
  expect(1, 7, TK_ID, "foo");
  expect(1, 10, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntDecimalBadChar)
{
  const char* src = "123A";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 4, TK_ID, "A");
  expect(1, 5, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntBinary)
{
  const char* src = "0b10100";

  expect(1, 1, TK_INT, "20");
  expect(1, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntBinaryWithSeparators)
{
  const char* src = "0b_101_00";

  expect(1, 1, TK_INT, "20");
  expect(1, 10, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntBinaryIncomplete)
{
  const char* src = "0b";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntBinaryBadChar)
{
  const char* src = "0b10134";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 6, TK_INT, "34");
  expect(1, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHex)
{
  const char* src = "0xFFFE";

  expect(1, 1, TK_INT, "65534");
  expect(1, 7, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexWithSeparator)
{
  const char* src = "0xFF_FE";

  expect(1, 1, TK_INT, "65534");
  expect(1, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexIncomplete)
{
  const char* src = "0x";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 3, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexBadChar)
{
  const char* src = "0x123EFGH";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 8, TK_ID, "GH");
  expect(1, 10, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexNoOverflow)
{
  const char* src = "0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF";

  // TODO: This is wrong, that's 2^64 - 1, should be 2^128 - 1
  expect(1, 1, TK_INT, "18446744073709551615");
  expect(1, 42, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexOverflow)
{
  const char* src = "0x1_0000_0000_0000_0000_0000_0000_0000_0000";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 43, TK_INT, "0");
  expect(1, 44, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntHexDigitOverflow)
{
  const char* src = "0x1_1111_1111_1111_1111_1111_1111_1111_1112";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 43, TK_INT, "2");
  expect(1, 44, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, IntNoOctal)
{
  const char* src = "0100";

  expect(1, 1, TK_INT, "100");
  expect(1, 5, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatDotOnly)
{
  const char* src = "1.234";

  expect(1, 1, TK_FLOAT, "1.234");
  expect(1, 6, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatEOnly)
{
  const char* src = "1e3";

  expect(1, 1, TK_FLOAT, "1000.0");
  expect(1, 4, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatNegativeE)
{
  const char* src = "1e-2";

  expect(1, 1, TK_FLOAT, "0.01");
  expect(1, 5, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatPositiveE)
{
  const char* src = "1e+2";

  expect(1, 1, TK_FLOAT, "100.0");
  expect(1, 5, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatDotAndE)
{
  const char* src = "1.234e2";

  expect(1, 1, TK_FLOAT, "123.4");
  expect(1, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatDotAndNegativeE)
{
  const char* src = "1.234e-2";

  expect(1, 1, TK_FLOAT, "0.01234");
  expect(1, 9, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatLeading0)
{
  const char* src = "01.234e2";

  expect(1, 1, TK_FLOAT, "123.4");
  expect(1, 9, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatExpLeading0)
{
  const char* src = "1.234e02";

  expect(1, 1, TK_FLOAT, "123.4");
  expect(1, 9, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, FloatIllegalExp)
{
  const char* src = "1.234efg";

  expect(1, 1, TK_LEX_ERROR, "LEX_ERROR");
  expect(1, 7, TK_ID, "fg");
  expect(1, 9, TK_EOF, "EOF");
  DO(test(src));
}


// Comments

TEST_F(LexerTest, LineComment)
{
  const char* src = "// Comment\n+";

  expect(2, 1, TK_PLUS, "+");
  expect(2, 2, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LineCommentWith3Slashes)
{
  const char* src = "/// Comment\n+";

  expect(2, 1, TK_PLUS, "+");
  expect(2, 2, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LineCommentContainingBlockCommentStart)
{
  const char* src = "// Foo /* Bar\n+";

  expect(2, 1, TK_PLUS, "+");
  expect(2, 2, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, LineCommentEndingWithSlash)
{
  const char* src = "// Comment/\n+";

  expect(2, 1, TK_PLUS, "+");
  expect(2, 2, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockComment)
{
  const char* src = "/* Comment */+";

  expect(1, 14, TK_PLUS, "+");
  expect(1, 15, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentOverMultipleLines)
{
  const char* src = "/* Foo\nBar */+";

  expect(2, 7, TK_PLUS, "+");
  expect(2, 8, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentStartingWith2Stars)
{
  const char* src = "/** Comment */+";

  expect(1, 15, TK_PLUS, "+");
  expect(1, 16, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentEndingWith2Stars)
{
  const char* src = "/* Comment **/+";

  expect(1, 15, TK_PLUS, "+");
  expect(1, 16, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNested)
{
  const char* src = "/* Comment /* Inner */ */+";

  expect(1, 26, TK_PLUS, "+");
  expect(1, 27, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedStartingWith2Slashes)
{
  const char* src = "/* Comment //* Inner */ */+";

  expect(1, 27, TK_PLUS, "+");
  expect(1, 28, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedStartingWith2Stars)
{
  const char* src = "/* Comment /** Inner */ */+";

  expect(1, 27, TK_PLUS, "+");
  expect(1, 28, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedEndingWith2Stars)
{
  const char* src = "/* Comment /* Inner **/ */+";

  expect(1, 27, TK_PLUS, "+");
  expect(1, 28, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedEndingWith2Slashes)
{
  const char* src = "/* Comment /* Inner *// */+";

  expect(1, 27, TK_PLUS, "+");
  expect(1, 28, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedInnerEmpty)
{
  const char* src = "/* Comment /**/ */+";

  expect(1, 19, TK_PLUS, "+");
  expect(1, 20, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedInner3Stars)
{
  const char* src = "/* Comment /***/ */+";

  expect(1, 20, TK_PLUS, "+");
  expect(1, 21, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedStartsNoWhitespace)
{
  const char* src = "/*/* Inner */ */+";

  expect(1, 17, TK_PLUS, "+");
  expect(1, 18, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedEndsNoWhitespace)
{
  const char* src = "/* Comment /* Inner */*/+";

  expect(1, 25, TK_PLUS, "+");
  expect(1, 26, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentContainsLineComment)
{
  const char* src = "/* Comment // */+";

  expect(1, 17, TK_PLUS, "+");
  expect(1, 18, TK_EOF, "EOF");
  DO(test(src));
}


TEST_F(LexerTest, BlockCommentNestedDeeper)
{
  const char* src = "/* /* /* */ /* */ */ */+";

  expect(1, 24, TK_PLUS, "+");
  expect(1, 25, TK_EOF, "EOF");
  DO(test(src));
}


// Non-token tests

TEST_F(LexerTest, Print)
{
  ASSERT_STREQ("match", lexer_print(TK_MATCH));
  ASSERT_STREQ("program", lexer_print(TK_PROGRAM));
  ASSERT_STREQ("+", lexer_print(TK_PLUS));
  ASSERT_STREQ(">=", lexer_print(TK_GE));
  ASSERT_EQ((void*)NULL, lexer_print(TK_ID));
  ASSERT_EQ((void*)NULL, lexer_print(TK_INT));
}
