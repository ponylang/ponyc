#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/lexer.h"
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


static bool _reached_end;
static bool _opt_at_end;
static token_id _next_token_at_end;


// Use the actually precedence table for testing
static int precedence(token_id id)
{
  switch(id)
  {
    // type operators
    case TK_ARROW:
      return 40;

    case TK_ISECTTYPE:
      return 30;

    case TK_UNIONTYPE:
      return 20;

    case TK_TUPLETYPE:
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
  TOKEN(NULL, 
    TK_AND, TK_OR, TK_XOR,
    TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD,
    TK_LSHIFT, TK_RSHIFT,
    TK_IS, TK_ISNT, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GE, TK_GT,
    TK_ASSIGN
    );
  TOKEN(NULL, TK_ID);
  DONE();

DEF(infix_test);
  TOKEN(NULL, TK_ID);
  BINDOP("test", binop);
  _reached_end = true;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();

DEF(infix_opt_test);
  TOKEN(NULL, TK_ID);
  OPT BINDOP("test", binop);
  _reached_end = true;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


class ParserApiBindopTest: public testing::Test
{};


// BINDOP

TEST(ParserApiBindopTest, LexError)
{
  const char* code = "A$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiBindopTest, Missing)
{
  const char* code = "A;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiBindopTest, SingleOp)
{
  const char* code = "A+B;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  DO(check_tree("(+ (id A) (id B))", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiBindopTest, TwoOpsInOrder)
{
  const char* code = "A * B + C;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  DO(check_tree("(+ (* (id A) (id B)) (id C))", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiBindopTest, TwoOpsOutOfOrder)
{
  const char* code = "A + B * C;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  DO(check_tree("(+ (id A) (* (id B) (id C)))", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiBindopTest, RepeatedOp)
{
  const char* code = "A + B + C + D;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  DO(check_tree("(+ (+ (+ (id A) (id B)) (id C)) (id D))", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiBindopTest, EqualPrecedence)
{
  const char* code = "A + B - C + D;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  DO(check_tree("(+ (- (+ (id A) (id B)) (id C)) (id D))", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiBindopTest, PrecedenceInBetweenExisting)
{
  const char* code = "A < B * C + D;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  DO(check_tree("(< (id A) (+ (* (id B) (id C)) (id D)))", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiBindopTest, RepeatedRightAssociative)
{
  const char* code = "A = B = C = D;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_test, "test");
  DO(check_tree("(= (id A) (= (id B) (= (id C) (id D))))", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


// OPT BINDOP

TEST(ParserApiBindopTest, OptLexError)
{
  const char* code = "A$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_opt_test, "test");
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiBindopTest, OptMissing)
{
  const char* code = "A;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_opt_test, "test");
  DO(check_tree("(id A)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiBindopTest, OptSingleOp)
{
  const char* code = "A+B;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, infix_opt_test, "test");
  DO(check_tree("(+ (id A) (id B))", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}
