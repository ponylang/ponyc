#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/lexer.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ast/token.h"
PONY_EXTERN_C_END

#include <gtest/gtest.h>


class LexerCommentTest: public testing::Test
{};


TEST(LexerCommentTest, LineComment)
{
  const char* code = "// Comment\n1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, LineCommentLineNumber)
{
  const char* code = "// Comment\n1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  ASSERT_EQ(2, token_line_number(token));
  ASSERT_EQ(1, token_line_position(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, LineCommentWith3Slashes)
{
  const char* code = "/// Comment\n1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, LineCommentContainingBlockCommentStart)
{
  const char* code = "// Foo /* Bar\n1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, LineCommentEndingWithSlash)
{
  const char* code = "// Comment/\n1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockComment)
{
  const char* code = "/* Comment */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentOverMultipleLines)
{
  const char* code = "/* Foo\nBar */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentStartingWith2Stars)
{
  const char* code = "/** Comment */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentEndingWith2Stars)
{
  const char* code = "/* Comment **/1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNested)
{
  const char* code = "/* Comment /* Inner */ */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedStartingWith2Slashes)
{
  const char* code = "/* Comment //* Inner */ */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedStartingWith2Stars)
{
  const char* code = "/* Comment /** Inner */ */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedEndingWith2Stars)
{
  const char* code = "/* Comment /* Inner **/ */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedEndingWith2Slashes)
{
  const char* code = "/* Comment /* Inner *// */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedInnerEmpty)
{
  const char* code = "/* Comment /**/ */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedInner3Stars)
{
  const char* code = "/* Comment /***/ */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedStartsNoWhitespace)
{
  const char* code = "/*/* Inner */ */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedEndsNoWhitespace)
{
  const char* code = "/* Comment /* Inner */*/1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentContainsLineComment)
{
  const char* code = "/* Comment // */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}


TEST(LexerCommentTest, BlockCommentNestedDeeper)
{
  const char* code = "/* /* /* */ /* */ */ */1";

  source_t* src = source_open_string(code);
  lexer_t* lexer = lexer_open(src);

  token_t* token = lexer_next(lexer);
  ASSERT_NE((void*)NULL, token);
  ASSERT_EQ(TK_INT, token_get_id(token));
  ASSERT_EQ(1, token_int(token));
  token_free(token);

  lexer_close(lexer);
  source_close(src);
}
