extern "C" {
#include "../../src/libponyc/ast/lexer.h"
#include "../../src/libponyc/ast/source.h"
}
#include <gtest/gtest.h>



class LexerTest: public testing::Test
{};


TEST(LexerTest, TripleString)
{
  const char* code = "\"\"\"Foo\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringEnds)
{
  const char* code = "\"\"\"Foo\"\"\"1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo", token->string);
  token_free(token);

  token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token->id);
  ASSERT_EQ(1, token->integer);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringContainingDoubleQuote)
{
  const char* code = "\"\"\"Foo\"bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo\"bar", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringContaining2DoubleQuotes)
{
  const char* code = "\"\"\"Foo\"\"bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo\"\"bar", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringMultipleLines)
{
  const char* code = "\"\"\"Foo\nbar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo\nbar", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringContainingEscape)
{
  const char* code = "\"\"\"Foo\\nbar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo\\nbar", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringStripsEqualLeadingWhitespace)
{
  const char* code = "\"\"\"   Foo\n   bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo\nbar", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringStripsIncreasingLeadingWhitespace)
{
  const char* code = "\"\"\"   Foo\n     bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo\n  bar", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringStripsDecreasingLeadingWhitespace)
{
  const char* code = "\"\"\"   Foo\n  bar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ(" Foo\nbar", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringStripsVariableLeadingWhitespace)
{
  const char* code = "\"\"\"   Foo\n     bar\n    wom\n   bat\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo\n  bar\n wom\nbat", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerTest, TripleStringWithLeadingEmptyLine)
{
  const char* code = "\"\"\"\nFoo\nbar\"\"\"";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_STRING, token->id);
  ASSERT_STREQ("Foo\nbar", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}
