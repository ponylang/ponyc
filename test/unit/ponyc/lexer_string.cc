#include <platform/platform.h>

PONY_EXTERN_C_BEGIN
#include <ast/lexer.h>
#include <ast/source.h>
#include <ast/token.h>
PONY_EXTERN_C_END

#include <gtest/gtest.h>


class LexerStringTest: public testing::Test
{};


TEST(LexerStringTest, String)
{
  const char* code = "\"Foo\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, StringEnds)
{
  const char* code = "\"Foo\"1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo", token_string(token));
  token_free(token);

  token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(token_int(token), 1);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, StringEscapedDoubleQuote)
{
  const char* code = "\"Foo\\\"Bar\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\"Bar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, StringEscapedSlashAtEnd)
{
  const char* code = "\"Foo\\\\\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\\", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, StringHexEscape)
{
  const char* code = "\"Foo\\x413\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("FooA3", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, StringUnicode4Escape)
{
  const char* code = "\"Foo\\u00413\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("FooA3", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, StringUnicode6Escape)
{
  const char* code = "\"Foo\\U0000413\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("FooA3", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleString)
{
  const char* code = "\"\"\"Foo\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringEnds)
{
  const char* code = "\"\"\"Foo\"\"\"1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo", token_string(token));
  token_free(token);

  token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(token_int(token), 1);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringContainingDoubleQuote)
{
  const char* code = "\"\"\"Foo\"bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\"bar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringContaining2DoubleQuotes)
{
  const char* code = "\"\"\"Foo\"\"bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\"\"bar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringMultipleLines)
{
  const char* code = "\"\"\"Foo\nbar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\nbar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringEmpty)
{
  const char* code = "\"\"\"\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringContainingEscape)
{
  const char* code = "\"\"\"Foo\\nbar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\\nbar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringStripsEqualLeadingWhitespace)
{
  const char* code = "\"\"\"   Foo\n   bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\nbar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringStripsIncreasingLeadingWhitespace)
{
  const char* code = "\"\"\"   Foo\n     bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\n  bar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringStripsDecreasingLeadingWhitespace)
{
  const char* code = "\"\"\"   Foo\n  bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ(" Foo\nbar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringStripsVariableLeadingWhitespace)
{
  const char* code = "\"\"\"   Foo\n     bar\n    wom\n   bat\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\n  bar\n wom\nbat", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringWithLeadingEmptyLine)
{
  const char* code = "\"\"\"\nFoo\nbar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\nbar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringWithNonLeadingEmptyLine)
{
  const char* code = "\"\"\"Foo\n\nbar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("Foo\n\nbar", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringUnterminated)
{
  const char* code = "\"\"\"\nFoo\nbar";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token_get_id(token));

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringUnterminatedEndWithDoubleQuote)
{
  const char* code = "\"\"\"\nFoo\nbar\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token_get_id(token));

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, TripleStringUnterminatedEndWith2DoubleQuotes)
{
  const char* code = "\"\"\"\nFoo\nbar\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token_get_id(token));

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerStringTest, EmptyStringAtEndOfSource)
{
  const char* code = "\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token_get_id(token));
  ASSERT_STREQ("", token_string(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}
