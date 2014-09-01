extern "C" {
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/lexer.h"
#include "../../src/libponyc/pass/expr.h"
#include "../../src/libponyc/ds/stringtab.h"
#include "../../src/libponyc/pass/pass.h"
#include "../../src/libponyc/pkg/package.h"
}
#include "util.h"
#include <gtest/gtest.h>
#include <stdio.h>

/*
#define DO(...) ASSERT_NO_FATAL_FAILURE(__VA_ARGS__)

const char* builtin =
" data None"
" data True"
" data False"
" type Bool is (True | False)"
" data I8"
" data I16"
" data I32"
" data I64"
" data I128"
" data U8"
" data U16"
" data U32"
" data U64"
" data U128"
" data F16"
" data F32"
" data F64"
" data SIntLiteral"
" data UIntLiteral"
" data FloatLiteral"
" class String val"
" type SInt is (SIntLiteral | I8 | I16 | I32 | I64 | I128)"
" type UInt is (UIntLiteral | U8 | U16 | U32 | U64 | U128)"
" type Integer is (SInt | UInt)"
" type Float is (FloatLiteral | F16 | F32 | F64)"
" type Arithmetic is (Integer | Float)";


static void type_check_and_compare_ast(const char* prog, token_id start_id,
  const char* expected_desc)
{
  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("scope2");
 
  ast_t* program;
  DO(load_test_program("prog", &program));
  
  ast_result_t r = ast_visit(&program, NULL, pass_expr);

  if(r != AST_OK)
    print_errors();

  ASSERT_EQ(AST_OK, r);

  ast_t* op_ast = find_sub_tree(program, start_id);
  ASSERT_NE((void*)NULL, op_ast);
  ast_t* op_type = ast_type(op_ast);
  DO(check_tree(expected_desc, op_type));
  
  ast_free(program);
}


static void type_check(const char* prog, ast_result_t expect)
{
  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("scope2");
  
  ast_t* program;
  DO(load_test_program("prog", &program));

  ast_result_t r = ast_visit(&program, NULL, pass_expr);

  if(r != expect)
    print_errors();

  ASSERT_EQ(expect, r);

  ast_free(program);
}


static void operand(const char* type, const char* int_lit,
  const char* float_lit, const char* str_lit, const char* var,
  const char** out_arg_type, const char** out_expr)
{
  if(strcmp(type, "String") == 0)
  {
    *out_arg_type = "None";
    *out_expr = str_lit;
    return;
  }

  if(strcmp(type, "UIntLiteral") == 0)
  {
    *out_arg_type = "None";
    *out_expr = int_lit;
    return;
  }

  if(strcmp(type, "FloatLiteral") == 0)
  {
    *out_arg_type = "None";
    *out_expr = float_lit;
    return;
  }

  *out_arg_type = type;
  *out_expr = var;
}


static const char* op_result(const char* type)
{
  if(strcmp(type, "Bool") == 0)
  {
    return
      "(uniontype"
      "  (nominal (id hygid) (id True) x val x)"
      "  (nominal (id hygid) (id False) x val x))";
  }

  static char buf[50];
  sprintf(buf, "(nominal(id hygid) (id %s) x val x)", type);
  return buf;
}


// Standard operator tests

static void test_bin_op_bad(token_id op_id, const char* type_a,
  const char* type_b)
{
  const char* arg_type_a;
  const char* arg_type_b;
  const char* expr_a;
  const char* expr_b;

  operand(type_a, "1", "1.1", "\"wibble\"", "a", &arg_type_a, &expr_a);
  operand(type_b, "2", "2.1", "\"wobble\"", "b", &arg_type_b, &expr_b);

  char prog[100];
  sprintf(prog, "trait T1 trait T2 class Bar is T1 fun box f()=>None "
    "class Foo fun ref bar(a:%s, b:%s)=>%s %s %s",
    arg_type_a, arg_type_b, expr_a, lexer_print(op_id), expr_b);

  type_check(prog, AST_FATAL);
}


static void test_bin_op(token_id op_id, const char* type_a, const char* type_b,
  const char* type_result, token_id replace_id = TK_NONE)
{
  const char* arg_type_a;
  const char* arg_type_b;
  const char* expr_a;
  const char* expr_b;

  operand(type_a, "1", "1.1", "\"wibble\"", "a", &arg_type_a, &expr_a);
  operand(type_b, "2", "2.1", "\"wobble\"", "b", &arg_type_b, &expr_b);
  const char* ast_result = op_result(type_result);

  char prog[100];
  sprintf(prog, "trait T1 trait T2 class Bar is T1 fun box f()=>None "
    "class Foo fun ref bar(a:%s, b:%s)=>%s %s %s",
    arg_type_a, arg_type_b, expr_a, lexer_print(op_id), expr_b);

  if(replace_id == TK_NONE)
    replace_id = op_id;

  type_check_and_compare_ast(prog, replace_id, ast_result);
}


static void test_unary_op_bad(token_id op_id, const char* type_op)
{
  const char* arg_type;
  const char* expr_op;

  operand(type_op, "1", "1.1", "\"wibble\"", "a", &arg_type, &expr_op);

  char prog[100];
  sprintf(prog, "class Foo fun ref bar(a:%s)=>%s %s",
    arg_type, lexer_print(op_id), expr_op);

  type_check(prog, AST_FATAL);
}


static void test_unary_op(token_id op_id, const char* type_op, 
  const char* type_result)
{
  const char* arg_type;
  const char* expr_op;

  operand(type_op, "1", "1.1", "\"wibble\"", "a", &arg_type, &expr_op);
  const char* ast_result = op_result(type_result);

  char prog[100];
  sprintf(prog, "class Foo fun ref bar(a:%s)=>%s %s",
    arg_type, lexer_print(op_id), expr_op);

  type_check_and_compare_ast(prog, op_id, ast_result);
}


static void test_bin_op_to_fun(token_id op_id, const char* type_b,
  const char* type_fn_arg, const char* fn_name, ast_result_t expect)
{
  const char* arg_type_b;
  const char* expr_b;

  operand(type_b, "2", "2.1", "\"wobble\"", "b", &arg_type_b, &expr_b);

  char prog[200];
  sprintf(prog, "class Foo "
    "fun box bar(a: Foo, b:%s):Bool => a %s %s "
    "fun box %s(other:%s):Bool => 1<2",
    arg_type_b, lexer_print(op_id), expr_b,
    fn_name, type_fn_arg);

  DO(type_check(prog, expect));
}


// Test cases

class ExprOperatorTest: public testing::Test
{};


// Arithmetic

static void standard_arithmetic(token_id op_id)
{
  DO(test_bin_op(op_id, "FloatLiteral", "FloatLiteral", "FloatLiteral"));
  DO(test_bin_op(op_id, "UIntLiteral", "FloatLiteral", "FloatLiteral"));
  DO(test_bin_op(op_id, "FloatLiteral", "UIntLiteral", "FloatLiteral"));

  DO(test_bin_op(op_id, "U32", "UIntLiteral", "U32"));
  DO(test_bin_op(op_id, "UIntLiteral", "I32", "I32"));
  DO(test_bin_op(op_id, "F16", "UIntLiteral", "F16"));
  DO(test_bin_op(op_id, "UIntLiteral", "F16", "F16"));
  DO(test_bin_op(op_id, "F32", "FloatLiteral", "F32"));
  DO(test_bin_op(op_id, "FloatLiteral", "F32", "F32"));
  
  DO(test_bin_op(op_id, "U8", "U8", "U8"));
  DO(test_bin_op(op_id, "U16", "U16", "U16"));
  DO(test_bin_op(op_id, "U32", "U32", "U32"));
  DO(test_bin_op(op_id, "U64", "U64", "U64"));
  DO(test_bin_op(op_id, "U128", "U128", "U128"));
  DO(test_bin_op(op_id, "I8", "I8", "I8"));
  DO(test_bin_op(op_id, "I16", "I16", "I16"));
  DO(test_bin_op(op_id, "I32", "I32", "I32"));
  DO(test_bin_op(op_id, "I64", "I64", "I64"));
  DO(test_bin_op(op_id, "I128", "I128", "I128"));
  DO(test_bin_op(op_id, "F16", "F16", "F16"));
  DO(test_bin_op(op_id, "F32", "F32", "F32"));
  DO(test_bin_op(op_id, "F64", "F64", "F64"));

  DO(test_bin_op_bad(op_id, "U8", "FloatLiteral"));
  DO(test_bin_op_bad(op_id, "FloatLiteral", "U8"));
  DO(test_bin_op_bad(op_id, "U8", "Bool"));
  DO(test_bin_op_bad(op_id, "Bool", "U8"));
  DO(test_bin_op_bad(op_id, "U8", "U16"));
  DO(test_bin_op_bad(op_id, "U8", "I8"));
  DO(test_bin_op_bad(op_id, "U8", "F16"));
  DO(test_bin_op_bad(op_id, "F32", "I32"));
  DO(test_bin_op_bad(op_id, "String", "I32"));
  DO(test_bin_op_bad(op_id, "F32", "String"));
  DO(test_bin_op_bad(op_id, "Foo", "U8"));
  DO(test_bin_op_bad(op_id, "Foo", "String"));
  DO(test_bin_op_bad(op_id, "I16", "Foo"));
  DO(test_bin_op_bad(op_id, "String", "Foo"));
  DO(test_bin_op_bad(op_id, "Foo", "Foo"));
}


TEST(ExprOperatorTest, Plus)
{
  DO(test_bin_op(TK_PLUS,
    "UIntLiteral", "UIntLiteral", "UIntLiteral"));

  DO(standard_arithmetic(TK_PLUS));

  // TODO: string concatenation isn't implemented yet
  //DO(test_bin_op(TK_PLUS, "String", "String", "String"));
}


TEST(ExprOperatorTest, Minus)
{
  // TODO: Is this sensible?
  DO(test_bin_op(TK_MINUS,
    "UIntLiteral", "UIntLiteral", "SIntLiteral"));

  DO(standard_arithmetic(TK_MINUS));
  DO(test_bin_op_bad(TK_MINUS, "String", "String"));
}


TEST(ExprOperatorTest, Multiply)
{
  DO(test_bin_op(TK_MULTIPLY,
    "UIntLiteral", "UIntLiteral", "UIntLiteral"));

  DO(standard_arithmetic(TK_MULTIPLY));
  DO(test_bin_op_bad(TK_MULTIPLY, "String", "String"));
}


TEST(ExprOperatorTest, Divide)
{
  DO(test_bin_op(TK_DIVIDE,
    "UIntLiteral", "UIntLiteral", "UIntLiteral"));

  DO(standard_arithmetic(TK_DIVIDE));
  DO(test_bin_op_bad(TK_DIVIDE, "String", "String"));
}


TEST(ExprOperatorTest, Mod)
{
  DO(test_bin_op(TK_MOD,
    "UIntLiteral", "UIntLiteral", "UIntLiteral"));

  DO(standard_arithmetic(TK_MOD));
  DO(test_bin_op_bad(TK_MOD, "String", "String"));
}


// TODO: This fails (and should be our current definitions). Is this sensible?
TEST(ExprOperatorTest, PlusSIntLiteralAndU8)
{
  const char* prog =
    "class Foo"
    "  fun ref bar(x:U8)=>"
    "    x + -1";
  
  const char* expected =
    "(+"
    "  (paramref (id x) [(nominal (id hygid) (id U8) x val x)])"
    "  (1 [(nominal (id hygid) (id SIntLiteral) x val x)])"
    "  [(nominal (id hygid) (id U8) x val x)])";

  DO(type_check_and_compare_ast(prog, TK_PLUS, expected));
}


TEST(ExprOperatorTest, UnaryMinus)
{
  DO(test_unary_op(TK_UNARY_MINUS, "FloatLiteral", "FloatLiteral"));
  DO(test_unary_op(TK_UNARY_MINUS, "UIntLiteral", "SIntLiteral"));

  DO(test_unary_op(TK_UNARY_MINUS, "U8", "U8"));
  DO(test_unary_op(TK_UNARY_MINUS, "U16", "U16"));
  DO(test_unary_op(TK_UNARY_MINUS, "U32", "U32"));
  DO(test_unary_op(TK_UNARY_MINUS, "U64", "U64"));
  DO(test_unary_op(TK_UNARY_MINUS, "U128", "U128"));
  DO(test_unary_op(TK_UNARY_MINUS, "I8", "I8"));
  DO(test_unary_op(TK_UNARY_MINUS, "I16", "I16"));
  DO(test_unary_op(TK_UNARY_MINUS, "I32", "I32"));
  DO(test_unary_op(TK_UNARY_MINUS, "I64", "I64"));
  DO(test_unary_op(TK_UNARY_MINUS, "I128", "I128"));
  DO(test_unary_op(TK_UNARY_MINUS, "F16", "F16"));
  DO(test_unary_op(TK_UNARY_MINUS, "F32", "F32"));
  DO(test_unary_op(TK_UNARY_MINUS, "F64", "F64"));

  DO(test_unary_op_bad(TK_UNARY_MINUS, "Bool"));
  DO(test_unary_op_bad(TK_UNARY_MINUS, "String"));
  DO(test_unary_op_bad(TK_UNARY_MINUS, "Foo"));
}


// Shift

static void standard_shift(token_id op_id)
{
  DO(test_bin_op(op_id, "UIntLiteral", "UIntLiteral", "UIntLiteral"));

  // TODO: Really? 1 << 3.2 is allowed?
  //DO(test_bin_op_bad(op_id,
  //  "UIntLiteral", "FloatLiteral"));

  DO(test_bin_op_bad(op_id, "FloatLiteral", "UIntLiteral"));

  DO(test_bin_op(op_id, "U32", "UIntLiteral", "U32"));
  DO(test_bin_op(op_id, "UIntLiteral", "I32", "I32"));

  DO(test_bin_op(op_id, "U8", "U8", "U8"));
  DO(test_bin_op(op_id, "U16", "U16", "U16"));
  DO(test_bin_op(op_id, "U32", "U32", "U32"));
  DO(test_bin_op(op_id, "U64", "U64", "U64"));
  DO(test_bin_op(op_id, "U128", "U128", "U128"));
  DO(test_bin_op(op_id, "I8", "I8", "I8"));
  DO(test_bin_op(op_id, "I16", "I16", "I16"));
  DO(test_bin_op(op_id, "I32", "I32", "I32"));
  DO(test_bin_op(op_id, "I64", "I64", "I64"));
  DO(test_bin_op(op_id, "I128", "I128", "I128"));

  DO(test_bin_op_bad(op_id, "U8", "FloatLiteral"));
  DO(test_bin_op_bad(op_id, "FloatLiteral", "U8"));
  DO(test_bin_op_bad(op_id, "U8", "Bool"));
  DO(test_bin_op_bad(op_id, "Bool", "U8"));

  // TODO: Why do we require both operands to be the same type?
  DO(test_bin_op_bad(op_id, "U8", "U16"));
  DO(test_bin_op_bad(op_id, "U8", "I8"));

  DO(test_bin_op_bad(op_id, "U8", "F16"));
  DO(test_bin_op_bad(op_id, "F32", "I32"));
  DO(test_bin_op_bad(op_id, "String", "I32"));
  DO(test_bin_op_bad(op_id, "F32", "String"));
  DO(test_bin_op_bad(op_id, "String", "String"));
  DO(test_bin_op_bad(op_id, "Foo", "U8"));
  DO(test_bin_op_bad(op_id, "Foo", "String"));
  DO(test_bin_op_bad(op_id, "I16", "Foo"));
  DO(test_bin_op_bad(op_id, "String", "Foo"));
  DO(test_bin_op_bad(op_id, "Foo", "Foo"));
}


TEST(ExprOperatorTest, LeftShift)
{
  DO(standard_shift(TK_LSHIFT));
}


TEST(ExprOperatorTest, RightShift)
{
  DO(standard_shift(TK_RSHIFT));
}


// Comparison

static void standard_comparison(token_id op_id, const char* fn_name)
{
  DO(test_bin_op(op_id, "UIntLiteral", "UIntLiteral", "Bool"));
  DO(test_bin_op(op_id, "UIntLiteral", "FloatLiteral", "Bool"));
  DO(test_bin_op(op_id, "FloatLiteral", "UIntLiteral", "Bool"));
  DO(test_bin_op(op_id, "FloatLiteral", "FloatLiteral", "Bool"));

  DO(test_bin_op(op_id, "U32", "UIntLiteral", "Bool"));
  DO(test_bin_op(op_id, "UIntLiteral", "I32", "Bool"));
  DO(test_bin_op(op_id, "F16", "UIntLiteral", "Bool"));
  DO(test_bin_op(op_id, "UIntLiteral", "F16", "Bool"));
  DO(test_bin_op(op_id, "F32", "FloatLiteral", "Bool"));
  DO(test_bin_op(op_id, "FloatLiteral", "F32", "Bool"));

  DO(test_bin_op(op_id, "U8", "U8", "Bool"));
  DO(test_bin_op(op_id, "U16", "U16", "Bool"));
  DO(test_bin_op(op_id, "U32", "U32", "Bool"));
  DO(test_bin_op(op_id, "U64", "U64", "Bool"));
  DO(test_bin_op(op_id, "U128", "U128", "Bool"));
  DO(test_bin_op(op_id, "I8", "I8", "Bool"));
  DO(test_bin_op(op_id, "I16", "I16", "Bool"));
  DO(test_bin_op(op_id, "I32", "I32", "Bool"));
  DO(test_bin_op(op_id, "I64", "I64", "Bool"));
  DO(test_bin_op(op_id, "I128", "I128", "Bool"));
  DO(test_bin_op(op_id, "F16", "F16", "Bool"));
  DO(test_bin_op(op_id, "F32", "F32", "Bool"));
  DO(test_bin_op(op_id, "F64", "F64", "Bool"));

  DO(test_bin_op_bad(op_id, "U8", "FloatLiteral"));
  DO(test_bin_op_bad(op_id, "FloatLiteral", "U8"));

  // TODO: These work for == and !=. This has to be an oversight, surely?
  DO(test_bin_op_bad(op_id, "U8", "Bool"));
  DO(test_bin_op_bad(op_id, "Bool", "U8"));
  DO(test_bin_op_bad(op_id, "Bool", "Foo"));
  DO(test_bin_op_bad(op_id, "Foo", "Bool"));

  DO(test_bin_op_bad(op_id, "U8", "U16"));
  DO(test_bin_op_bad(op_id, "U8", "I8"));
  DO(test_bin_op_bad(op_id, "U8", "F16"));
  DO(test_bin_op_bad(op_id, "F32", "I32"));
  DO(test_bin_op_bad(op_id, "F32", "String"));
  DO(test_bin_op_bad(op_id, "I16", "Foo"));

  DO(test_bin_op_bad(op_id, "String", "I32"));
  DO(test_bin_op_bad(op_id, "Foo", "U8"));
  DO(test_bin_op_bad(op_id, "Foo", "String"));
  DO(test_bin_op_bad(op_id, "String", "Foo"));
  DO(test_bin_op_bad(op_id, "Foo", "Foo"));

  DO(test_bin_op_to_fun(op_id, "Foo", "Foo", fn_name, AST_OK));
  DO(test_bin_op_to_fun(op_id, "UIntLiteral", "U8", fn_name, AST_OK));
  DO(test_bin_op_to_fun(op_id, "U32", "U32", fn_name, AST_OK));
  DO(test_bin_op_to_fun(op_id, "FloatLiteral", "F16", fn_name, AST_OK));
  DO(test_bin_op_to_fun(op_id, "F32", "F32", fn_name, AST_OK));
  DO(test_bin_op_to_fun(op_id, "Bool", "Bool", fn_name, AST_OK));
  DO(test_bin_op_to_fun(op_id, "String", "String", fn_name, AST_OK));
  DO(test_bin_op_to_fun(op_id, "Foo", "U32", fn_name, AST_FATAL));
}


TEST(ExprOperatorTest, Equal)
{
  DO(standard_comparison(TK_EQ, "eq"));
  DO(test_bin_op(TK_EQ, "Bool", "Bool", "Bool"));
}


TEST(ExprOperatorTest, NotEqual)
{
  DO(standard_comparison(TK_NE, "ne"));
  DO(test_bin_op(TK_NE, "Bool", "Bool", "Bool"));
}


TEST(ExprOperatorTest, LessThan)
{
  DO(standard_comparison(TK_LT, "lt"));
  DO(test_bin_op_bad(TK_LT, "Bool", "Bool"));
}


TEST(ExprOperatorTest, LessThanEqual)
{
  DO(standard_comparison(TK_LE, "le"));
  DO(test_bin_op_bad(TK_LE, "Bool", "Bool"));
}


TEST(ExprOperatorTest, GreaterThan)
{
  DO(standard_comparison(TK_GT, "gt"));
  DO(test_bin_op_bad(TK_GT, "Bool", "Bool"));
}


TEST(ExprOperatorTest, GreaterThanEqual)
{
  DO(standard_comparison(TK_GE, "ge"));
  DO(test_bin_op_bad(TK_GE, "Bool", "Bool"));
}


// Logical

static void standard_logical(token_id op_id)
{
  DO(test_bin_op(op_id, "UIntLiteral", "UIntLiteral", "UIntLiteral"));

  DO(test_bin_op(op_id, "U32", "UIntLiteral", "U32"));
  DO(test_bin_op(op_id, "UIntLiteral", "I32", "I32"));

  DO(test_bin_op(op_id, "U8", "U8", "U8"));
  DO(test_bin_op(op_id, "U16", "U16", "U16"));
  DO(test_bin_op(op_id, "U32", "U32", "U32"));
  DO(test_bin_op(op_id, "U64", "U64", "U64"));
  DO(test_bin_op(op_id, "U128", "U128", "U128"));
  DO(test_bin_op(op_id, "I8", "I8", "I8"));
  DO(test_bin_op(op_id, "I16", "I16", "I16"));
  DO(test_bin_op(op_id, "I32", "I32", "I32"));
  DO(test_bin_op(op_id, "I64", "I64", "I64"));
  DO(test_bin_op(op_id, "I128", "I128", "I128"));
  DO(test_bin_op(op_id, "Bool", "Bool", "Bool"));

  DO(test_bin_op_bad(op_id, "UIntLiteral", "FloatLiteral"));
  DO(test_bin_op_bad(op_id, "FloatLiteral", "UIntLiteral"));
  DO(test_bin_op_bad(op_id, "FloatLiteral", "FloatLiteral"));

  DO(test_bin_op_bad(op_id, "U8", "FloatLiteral"));
  DO(test_bin_op_bad(op_id, "FloatLiteral", "U8"));
  DO(test_bin_op_bad(op_id, "U8", "Bool"));
  DO(test_bin_op_bad(op_id, "Bool", "U8"));
  DO(test_bin_op_bad(op_id, "U8", "U16"));
  DO(test_bin_op_bad(op_id, "U8", "I8"));
  DO(test_bin_op_bad(op_id, "U8", "F16"));
  DO(test_bin_op_bad(op_id, "F32", "I32"));
  DO(test_bin_op_bad(op_id, "F16", "F16"));
  DO(test_bin_op_bad(op_id, "F32", "F32"));
  DO(test_bin_op_bad(op_id, "F64", "F64"));
  DO(test_bin_op_bad(op_id, "F32", "UIntLiteral"));
  DO(test_bin_op_bad(op_id, "UIntLiteral", "F32"));
  DO(test_bin_op_bad(op_id, "F32", "FloatLiteral"));
  DO(test_bin_op_bad(op_id, "FloatLiteral", "F32"));
  DO(test_bin_op_bad(op_id, "U32", "String"));
  DO(test_bin_op_bad(op_id, "F32", "String"));
  DO(test_bin_op_bad(op_id, "String", "I32"));
  DO(test_bin_op_bad(op_id, "String", "String"));
  DO(test_bin_op_bad(op_id, "Foo", "U8"));
  DO(test_bin_op_bad(op_id, "Foo", "F16"));
  DO(test_bin_op_bad(op_id, "Foo", "String"));
  DO(test_bin_op_bad(op_id, "String", "Foo"));
  DO(test_bin_op_bad(op_id, "Foo", "Foo"));
}


TEST(ExprOperatorTest, And)
{
  DO(standard_logical(TK_AND));
}


TEST(ExprOperatorTest, Or)
{
  DO(standard_logical(TK_OR));
}


TEST(ExprOperatorTest, Xor)
{
  DO(standard_logical(TK_XOR));
}


TEST(ExprOperatorTest, Not)
{
  DO(test_unary_op(TK_NOT, "UIntLiteral", "UIntLiteral"));

  DO(test_unary_op(TK_NOT, "U8", "U8"));
  DO(test_unary_op(TK_NOT, "U16", "U16"));
  DO(test_unary_op(TK_NOT, "U32", "U32"));
  DO(test_unary_op(TK_NOT, "U64", "U64"));
  DO(test_unary_op(TK_NOT, "U128", "U128"));
  DO(test_unary_op(TK_NOT, "I8", "I8"));
  DO(test_unary_op(TK_NOT, "I16", "I16"));
  DO(test_unary_op(TK_NOT, "I32", "I32"));
  DO(test_unary_op(TK_NOT, "I64", "I64"));
  DO(test_unary_op(TK_NOT, "I128", "I128"));
  DO(test_unary_op(TK_NOT, "Bool", "Bool"));

  // TODO: We allow negation of floats? What does that even mean?
  //DO(test_unary_op_bad(TK_NOT, "FloatLiteral"));
  //DO(test_unary_op_bad(TK_NOT, "F16"));
  //DO(test_unary_op_bad(TK_NOT, "F32"));
  //DO(test_unary_op_bad(TK_NOT, "F64"));

  DO(test_unary_op_bad(TK_NOT, "String"));
  DO(test_unary_op_bad(TK_NOT, "Foo"));
}


// Identity test

static void standard_identity(token_id op_id, token_id alt_id)
{
  DO(test_bin_op(op_id, "UIntLiteral", "UIntLiteral", "Bool", alt_id));
  DO(test_bin_op(op_id, "UIntLiteral", "FloatLiteral", "Bool", alt_id));
  DO(test_bin_op(op_id, "FloatLiteral", "UIntLiteral", "Bool", alt_id));
  DO(test_bin_op(op_id, "FloatLiteral", "FloatLiteral", "Bool", alt_id));

  DO(test_bin_op(op_id, "U32", "UIntLiteral", "Bool", alt_id));
  DO(test_bin_op(op_id, "UIntLiteral", "I32", "Bool", alt_id));
  DO(test_bin_op(op_id, "F16", "UIntLiteral", "Bool", alt_id));
  DO(test_bin_op(op_id, "UIntLiteral", "F16", "Bool", alt_id));
  DO(test_bin_op(op_id, "F32", "FloatLiteral", "Bool", alt_id));
  DO(test_bin_op(op_id, "FloatLiteral", "F32", "Bool", alt_id));

  DO(test_bin_op(op_id, "U8", "U8", "Bool", alt_id));
  DO(test_bin_op(op_id, "U16", "U16", "Bool", alt_id));
  DO(test_bin_op(op_id, "U32", "U32", "Bool", alt_id));
  DO(test_bin_op(op_id, "U64", "U64", "Bool", alt_id));
  DO(test_bin_op(op_id, "U128", "U128", "Bool", alt_id));
  DO(test_bin_op(op_id, "I8", "I8", "Bool", alt_id));
  DO(test_bin_op(op_id, "I16", "I16", "Bool", alt_id));
  DO(test_bin_op(op_id, "I32", "I32", "Bool", alt_id));
  DO(test_bin_op(op_id, "I64", "I64", "Bool", alt_id));
  DO(test_bin_op(op_id, "I128", "I128", "Bool", alt_id));
  DO(test_bin_op(op_id, "F16", "F16", "Bool", alt_id));
  DO(test_bin_op(op_id, "F32", "F32", "Bool", alt_id));
  DO(test_bin_op(op_id, "F64", "F64", "Bool", alt_id));
  DO(test_bin_op(op_id, "Bool", "Bool", "Bool", alt_id));

  // TODO: Foo is math-compatible with itself. Really?
  //DO(test_bin_op(op_id, "Foo", "Foo", "Bool", op_id));

  // Bar provides T1
  DO(test_bin_op(op_id, "Bar", "T1", "Bool", op_id));
  DO(test_bin_op(op_id, "T1", "Bar", "Bool", op_id));
  DO(test_bin_op(op_id, "T1", "T2", "Bool", op_id));

  DO(test_bin_op(op_id, "Bar", "{fun box f()}", "Bool", op_id));
  DO(test_bin_op(op_id, "{fun box f()}", "Bar", "Bool", op_id));
  DO(test_bin_op(op_id, "T1", "{fun box f()}", "Bool", op_id));
  DO(test_bin_op(op_id, "{fun box f()}", "T1", "Bool", op_id));

  DO(test_bin_op_bad(op_id, "U8", "FloatLiteral"));
  DO(test_bin_op_bad(op_id, "FloatLiteral", "U8"));
  DO(test_bin_op_bad(op_id, "U8", "Bool"));
  DO(test_bin_op_bad(op_id, "Bool", "U8"));
  DO(test_bin_op_bad(op_id, "U8", "U16"));
  DO(test_bin_op_bad(op_id, "U8", "I8"));
  DO(test_bin_op_bad(op_id, "U8", "F16"));
  DO(test_bin_op_bad(op_id, "F32", "I32"));
  DO(test_bin_op_bad(op_id, "String", "I32"));
  DO(test_bin_op_bad(op_id, "F32", "String"));
  DO(test_bin_op_bad(op_id, "Foo", "U8"));
  DO(test_bin_op_bad(op_id, "Foo", "String"));
  DO(test_bin_op_bad(op_id, "I16", "Foo"));
  DO(test_bin_op_bad(op_id, "String", "Foo"));

  DO(test_bin_op_bad(op_id, "Foo", "Bar"));
  DO(test_bin_op_bad(op_id, "Foo", "T1"));
  DO(test_bin_op_bad(op_id, "T1", "Foo"));
  DO(test_bin_op_bad(op_id, "Foo", "{fun box f()}"));
  DO(test_bin_op_bad(op_id, "{fun box f()}", "Foo"));
}


TEST(ExprOperatorTest, Is)
{
  DO(standard_identity(TK_IS, TK_EQ));
}


TEST(ExprOperatorTest, Isnt)
{
  DO(standard_identity(TK_ISNT, TK_NE));
}





// TODO: Writing to function parameters?

// TODO: TK_ASSIGN, TK_CONSUME, TK_RECOVER

*/
