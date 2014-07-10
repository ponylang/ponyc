extern "C" {
#include "../../src/libponyc/ast/lexer.h"
#include "../../src/libponyc/ast/source.h"
}
#include <gtest/gtest.h>


class LexerNumberTest: public testing::Test
{};


TEST(LexerNumberTest, Int0)
{
  const char* code = "0";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token->id);
  ASSERT_EQ(0, token->integer);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, Int0Finishes)
{
  const char* code = "0-";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token->id);
  ASSERT_EQ(0, token->integer);
  token_free(token);

  token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_MINUS, token->id);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntDecimal)
{
  const char* code = "12345";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token->id);
  ASSERT_EQ(12345, token->integer);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntDecimalDot)
{
  const char* code = "12345.foo";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token->id);
  ASSERT_EQ(12345, token->integer);
  token_free(token);

  token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_DOT, token->id);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntDecimalBadChar)
{
  const char* code = "123A";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token->id);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntBinary)
{
  const char* code = "0b10100";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token->id);
  ASSERT_EQ(20, token->integer);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntBinaryIncomplete)
{
  const char* code = "0b";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token->id);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntBinaryBadChar)
{
  const char* code = "0b10134";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token->id);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntHex)
{
  const char* code = "0xFFFE";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token->id);
  ASSERT_EQ(65534, token->integer);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntHexIncomplete)
{
  const char* code = "0x";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token->id);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntHexBadChar)
{
  const char* code = "0x123EFGH";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token->id);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, IntNoOctal)
{
  const char* code = "0100";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token->id);
  ASSERT_EQ(100, token->integer);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, FloatDotOnly)
{
  const char* code = "1.234";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_FLOAT, token->id);
  ASSERT_EQ(1.234, token->real);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, FloatEOnly)
{
  const char* code = "1e3";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_FLOAT, token->id);
  ASSERT_EQ(1000.0, token->real);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, FloatNegativeE)
{
  const char* code = "1e-2";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_FLOAT, token->id);
  ASSERT_EQ(0.01, token->real);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, FloatPositiveE)
{
  const char* code = "1e+2";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_FLOAT, token->id);
  ASSERT_EQ(100.0, token->real);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, FloatDotAndE)
{
  const char* code = "1.234e2";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_FLOAT, token->id);
  ASSERT_EQ(123.4, token->real);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, FloatLeading0)
{
  const char* code = "01.234e2";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_FLOAT, token->id);
  ASSERT_EQ(123.4, token->real);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerNumberTest, FloatExpLeading0)
{
  const char* code = "1.234e02";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_FLOAT, token->id);
  ASSERT_EQ(123.4, token->real);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}
