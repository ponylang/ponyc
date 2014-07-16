extern "C" {
#include "../../src/libponyc/ast/lexer.h"
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
}
#include <gtest/gtest.h>


static char* flatten_ast_internal(ast_t* ast, char* dst)
{
  if(ast_child(ast) != NULL)
  {
    *dst = '(';
    dst++;
    dst = flatten_ast_internal(ast_child(ast), dst);
  }

  *dst = ast_get_print(ast)[0];
  dst++;

  if(ast_childidx(ast, 1) != NULL)
    dst = flatten_ast_internal(ast_childidx(ast, 1), dst);

  if(ast_child(ast) != NULL)
  {
    *dst = ')';
    dst++;
  }

  return dst;
}

static const char* flatten_ast(ast_t* ast)
{
  static char buf[100];
  char* end = flatten_ast_internal(ast, buf);
  *end = '\0';
  return buf;
}


// Use the actually precedence table for testing
static int precedence(token_id id)
{
  switch(id)
  {
    // type operators
  case TK_ISECTTYPE:
    return 40;

  case TK_UNIONTYPE:
    return 30;

  case TK_TUPLETYPE:
    return 20;

  case TK_ARROW:
    return 10;

    // postfix operators
  case TK_DOT:
  case TK_BANG:
  case TK_QUALIFY:
  case TK_CALL:
    return 100;

    // infix operators
  case TK_MULTIPLY:
  case TK_DIVIDE:
  case TK_MOD:
    return 90;

  case TK_PLUS:
  case TK_MINUS:
    return 80;

  case TK_LSHIFT:
  case TK_RSHIFT:
    return 70;

  case TK_LT:
  case TK_LE:
  case TK_GE:
  case TK_GT:
    return 60;

  case TK_IS:
  case TK_ISNT:
  case TK_EQ:
  case TK_NE:
    return 50;

  case TK_AND: return 40;
  case TK_XOR: return 30;
  case TK_OR: return 20;
  case TK_ASSIGN: return 10;

  default: return INT_MAX;
  }
}

static bool associativity(token_id id)
{
  switch(id)
  {
  case TK_UNIONTYPE:
  case TK_ISECTTYPE:
  case TK_TUPLETYPE:
  case TK_ASSIGN:
  case TK_ARROW:
    return true;

  default:
    return false;
  }
}


DEF(binop);
  TOKEN(
    TK_AND, TK_OR, TK_XOR,
    TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD,
    TK_LSHIFT, TK_RSHIFT,
    TK_IS, TK_ISNT, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GE, TK_GT,
    TK_ASSIGN
    );
  TOKEN(TK_ID);
  DONE();

DEF(infix);
  TOKEN(TK_ID);
  OPT BINDOP(binop);
  DONE();


class ParserApiBindopTest: public testing::Test
{};


TEST(ParserApiBindopTest, SingleOp)
{
  const char* code = "A+B";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, infix);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_STREQ("(A+B)", flatten_ast(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiBindopTest, TwoOpsInOrder)
{
  const char* code = "A * B + C";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, infix);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_STREQ("((A*B)+C)", flatten_ast(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiBindopTest, TwoOpsOutOfOrder)
{
  const char* code = "A + B * C";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, infix);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_STREQ("(A+(B*C))", flatten_ast(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiBindopTest, RepeatedOp)
{
  const char* code = "A + B + C + D";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, infix);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_STREQ("(((A+B)+C)+D)", flatten_ast(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiBindopTest, EqualPrecedence)
{
  const char* code = "A + B - C + D";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, infix);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_STREQ("(((A+B)-C)+D)", flatten_ast(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiBindopTest, PrecedenceInBetweenExisting)
{
  const char* code = "A < B * C + D";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, infix);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_STREQ("(A<((B*C)+D))", flatten_ast(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiBindopTest, RepeatedRightAssociative)
{
  const char* code = "A = B = C = D";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, infix);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_STREQ("(A=(B=(C=D)))", flatten_ast(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiBindopTest, LexError)
{
  const char* code = "A = B = $ = D";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, infix);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiBindopTest, ParseError)
{
  const char* code = "A = B = C = +";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, infix);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}
