extern "C" {
#include "../../src/libponyc/ast/lexer.h"
#include "../../src/libponyc/ast/source.h"
}
#include <gtest/gtest.h>


class LexerMiscTest: public testing::Test
{};


TEST(LexerMiscTest, Id)
{
  const char* code = "Foo";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_ID, token->id);
  ASSERT_STREQ("Foo", token->string);
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerMiscTest, IdStartingWithKeyword)
{
	const char* code = "classFoo";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_ID, token->id);
	ASSERT_STREQ("classFoo", token->string);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, Keyword)
{
	const char* code = "class";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_CLASS, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, Symbol1Char)
{
	const char* code = "+";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_PLUS, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, Symbol2CharStartingWith1CharSymbol)
{
	const char* code = "->1";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_ARROW, token->id);
	token_free(token);

	token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_INT, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, SymbolNewAtStart)
{
	const char* code = "-";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_MINUS_NEW, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, SymbolNewAfterWhitespace)
{
	const char* code = "    -";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_MINUS_NEW, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, SymbolNewAfterNewline)
{
	const char* code = "1\n-";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_INT, token->id);
	token_free(token);

	token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_MINUS_NEW, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, SymbolNotNewAfterInt)
{
	const char* code = "1-";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_INT, token->id);
	token_free(token);

	token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_MINUS, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, Eof)
{
	const char* code = "1";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_INT, token->id);
	token_free(token);

	token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_EOF, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, EofIfEmpty)
{
	const char* code = "";

	source_t* src = source_open_string(code);
	lexer_t* lexer = lexer_open(src);

	token_t* token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_EOF, token->id);
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}
