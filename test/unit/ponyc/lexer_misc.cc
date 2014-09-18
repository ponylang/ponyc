#include <gtest/gtest.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN
#include <ast/lexer.h>
#include <ast/source.h>
#include <ast/token.h>
PONY_EXTERN_C_END

class LexerMiscTest: public testing::Test
{};


TEST(LexerMiscTest, Id)
{
  const char* code = "Foo";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_ID, token_get_id(token));
  ASSERT_STREQ("Foo", token_string(token));
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
	ASSERT_EQ(TK_ID, token_get_id(token));
	ASSERT_STREQ("classFoo", token_string(token));
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
	ASSERT_EQ(TK_CLASS, token_get_id(token));
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
	ASSERT_EQ(TK_PLUS, token_get_id(token));
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
	ASSERT_EQ(TK_ARROW, token_get_id(token));
	token_free(token);

	token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_INT, token_get_id(token));
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
	ASSERT_EQ(TK_MINUS_NEW, token_get_id(token));
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
	ASSERT_EQ(TK_MINUS_NEW, token_get_id(token));
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
	ASSERT_EQ(TK_INT, token_get_id(token));
	token_free(token);

	token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_MINUS_NEW, token_get_id(token));
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
	ASSERT_EQ(TK_INT, token_get_id(token));
	token_free(token);

	token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_MINUS, token_get_id(token));
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
	ASSERT_EQ(TK_INT, token_get_id(token));
	token_free(token);

	token = lexer_next(lexer);
	ASSERT_NE((void*)NULL, token);
	ASSERT_EQ(TK_EOF, token_get_id(token));
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
	ASSERT_EQ(TK_EOF, token_get_id(token));
	token_free(token);

	lexer_close(lexer);
	source_close(src);
}


TEST(LexerMiscTest, BadChar)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);
  free_errors();

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_LEX_ERROR, token_get_id(token));
  ASSERT_EQ(1, get_error_count());
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerMiscTest, IsAbstractKeyword)
{
  ASSERT_EQ(TK_PROGRAM, lexer_is_abstract_keyword("program"));
  ASSERT_EQ(TK_CASE, lexer_is_abstract_keyword("case"));
  ASSERT_EQ(TK_LEX_ERROR, lexer_is_abstract_keyword("foo"));
  ASSERT_EQ(TK_LEX_ERROR, lexer_is_abstract_keyword("+"));
}


TEST(LexerMiscTest, Print)
{
  ASSERT_STREQ("match", lexer_print(TK_MATCH));
  ASSERT_STREQ("program", lexer_print(TK_PROGRAM));
  ASSERT_STREQ("+", lexer_print(TK_PLUS));
  ASSERT_STREQ(">=", lexer_print(TK_GE));
  ASSERT_EQ((void*)NULL, lexer_print(TK_ID));
  ASSERT_EQ((void*)NULL, lexer_print(TK_INT));
}
