#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/lexer.h"
#include "../../src/libponyc/pass/expr.h"
#include "../../src/libponyc/ds/stringtab.h"
#include "../../src/libponyc/pass/pass.h"
#include "../../src/libponyc/pkg/package.h"
PONY_EXTERN_C_END

#include "util.h"
#include "builtin_ast.h"
#include <gtest/gtest.h>
#include <stdio.h>


//static const char* t_fns = NOMINAL_CAP(Fns, box);

static const char* fns_desc =
"(class{scope eq ne lt le gt ge}{def Fns} (id Fns) x ref x"
"  (members"
"    (fun{scope}{def eq} box (id eq) x"
"      (params (param (id a) " NOMINAL_CAP(Fns, box) " x))"
"      (uniontype " NOMINAL(True) NOMINAL(False) ") x x)"
"    (fun{scope}{def ne} box (id ne) x"
"      (params (param (id a) " NOMINAL_CAP(Fns, box) " x))"
"      (uniontype " NOMINAL(True) NOMINAL(False) ") x x)"
"    (fun{scope}{def lt} box (id lt) x"
"      (params (param (id a) " NOMINAL_CAP(Fns, box) " x))"
"      (uniontype " NOMINAL(True) NOMINAL(False) ") x x)"
"    (fun{scope}{def le} box (id le) x"
"      (params (param (id a) " NOMINAL_CAP(Fns, box) " x))"
"      (uniontype " NOMINAL(True) NOMINAL(False) ") x x)"
"    (fun{scope}{def gt} box (id gt) x"
"      (params (param (id a) " NOMINAL_CAP(Fns, box) " x))"
"      (uniontype " NOMINAL(True) NOMINAL(False) ") x x)"
"    (fun{scope}{def ge} box (id ge) x"
"      (params (param (id a) " NOMINAL_CAP(Fns, box) " x))"
"      (uniontype " NOMINAL(True) NOMINAL(False) ") x x)))";


class ExprOperatorTest : public testing::Test
{
protected:
  builder_t* builder;
  char op_buf[1000];
  ast_t* op_ast;
  ast_t* op_type;

  virtual void SetUp()
  {
    free_errors();

    DO(build_ast_from_string(builtin, NULL, &builder));
    ASSERT_NE((void*)NULL, builder);
  }

  virtual void TearDown()
  {
    builder_free(builder);

    if(HasFatalFailure())
      print_errors();
  }


  // Workers

  void add_op()
  {
    op_ast = builder_add(builder, "test", op_buf);
    ASSERT_NE((void*)NULL, op_ast);
  }

  void get_op_type()
  {
    ASSERT_NE((void*)NULL, op_ast);
    op_type = ast_type(op_ast);
    ASSERT_NE((void*)NULL, op_type);
  }

  void compare_result(const char* expected_desc, ast_t* actual)
  {
    ast_t* expected_ast = builder_add(builder, "test", expected_desc);
    ASSERT_NE((void*)NULL, expected_ast);

    ASSERT_TRUE(build_compare_asts_no_sibling(expected_ast, actual));
  }


  // Standard tests

  void test_bin_op_bad(token_id op_id, const char* type_a,
    const char* type_b)
  {
    snprintf(op_buf, sizeof(op_buf), "(%s (fvarref (id a) [%s])(fvarref (id b) [%s]))",
      lexer_print(op_id), type_a, type_b);

    DO(add_op());
    ASSERT_EQ(AST_FATAL, pass_expr(&op_ast));
  }

  void test_bin_op_replace(token_id op_id, const char* type_a,
    const char* type_b, const char* type_result, token_id replace_id)
  {
    snprintf(op_buf, sizeof(op_buf), "(%s (fvarref (id a) [%s])(fvarref (id b) [%s]))",
      lexer_print(op_id), type_a, type_b);

    DO(add_op());
    ASSERT_EQ(AST_OK, pass_expr(&op_ast));
    ASSERT_EQ(replace_id, ast_id(op_ast));
    DO(get_op_type());
    DO(compare_result(type_result, op_type));
  }

  void test_bin_op(token_id op_id, const char* type_a, const char* type_b,
    const char* type_result)
  {
    test_bin_op_replace(op_id, type_a, type_b, type_result, op_id);
  }

  void test_bin_op_to_fun(token_id op_id, const char* type_a,
    const char* type_b, const char* type_result, const char* fn_name)
  {
    ASSERT_NE((void*)NULL, builder_add(builder, "test", fns_desc));

    snprintf(op_buf, sizeof(op_buf), "(%s (fvarref (id a) [%s])(fvarref (id b) [%s]))",
      lexer_print(op_id), type_a, type_b);

    DO(add_op());
    ASSERT_EQ(AST_OK, pass_expr(&op_ast));

    char result_buf[1000];
    snprintf(op_buf, sizeof(op_buf),
      "(call"
      "  (funref (fvarref(id a) [%s]) (id %s)"
      "    [(funtype box x (params(param(id a) %s x)) %s)])"
      "  (positionalargs (fvarref(id b) [%s]))"
      "  x [%s])", type_a, fn_name, type_a, type_result, type_b, type_result);

    DO(compare_result(result_buf, op_ast));
  }

  void test_unary_op_bad(token_id op_id, const char* type_op)
  {
    snprintf(op_buf, sizeof(op_buf), "(- (fvarref (id op) [%s]))", type_op);

    DO(add_op());
    ast_setid(op_ast, op_id);

    ASSERT_EQ(AST_FATAL, pass_expr(&op_ast));
  }

  void test_unary_op(token_id op_id, const char* type_op,
    const char* type_result)
  {
    snprintf(op_buf, sizeof(op_buf), "(- (fvarref (id op) [%s]))", type_op);

    DO(add_op());
    ast_setid(op_ast, op_id);

    ASSERT_EQ(AST_OK, pass_expr(&op_ast));
    ASSERT_EQ(op_id, ast_id(op_ast));
    DO(get_op_type());

    DO(compare_result(type_result, op_type));
  }


  // Test groups

  void standard_arithmetic(token_id op_id)
  {
    DO(test_bin_op(op_id, t_f_lit, t_f_lit, t_f_lit));
    DO(test_bin_op(op_id, t_u_lit, t_f_lit, t_f_lit));
    DO(test_bin_op(op_id, t_f_lit, t_u_lit, t_f_lit));

    DO(test_bin_op(op_id, t_u32, t_u_lit, t_u32));
    DO(test_bin_op(op_id, t_u_lit, t_i32, t_i32));
    DO(test_bin_op(op_id, t_f32, t_f_lit, t_f32));
    DO(test_bin_op(op_id, t_f_lit, t_f32, t_f32));

    DO(test_bin_op(op_id, t_u8, t_u8, t_u8));
    DO(test_bin_op(op_id, t_u16, t_u16, t_u16));
    DO(test_bin_op(op_id, t_u32, t_u32, t_u32));
    DO(test_bin_op(op_id, t_u64, t_u64, t_u64));
    DO(test_bin_op(op_id, t_u128, t_u128, t_u128));
    DO(test_bin_op(op_id, t_i8, t_i8, t_i8));
    DO(test_bin_op(op_id, t_i16, t_i16, t_i16));
    DO(test_bin_op(op_id, t_i32, t_i32, t_i32));
    DO(test_bin_op(op_id, t_i64, t_i64, t_i64));
    DO(test_bin_op(op_id, t_i128, t_i128, t_i128));
    DO(test_bin_op(op_id, t_f32, t_f32, t_f32));
    DO(test_bin_op(op_id, t_f64, t_f64, t_f64));

    DO(test_bin_op_bad(op_id, t_u8, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_u8));
    DO(test_bin_op_bad(op_id, t_u8, t_bool));
    DO(test_bin_op_bad(op_id, t_bool, t_u8));
    DO(test_bin_op_bad(op_id, t_u8, t_u16));
    DO(test_bin_op_bad(op_id, t_u8, t_i8));
    DO(test_bin_op_bad(op_id, t_f32, t_i32));
    DO(test_bin_op_bad(op_id, t_string, t_i32));
    DO(test_bin_op_bad(op_id, t_f32, t_string));
    DO(test_bin_op_bad(op_id, t_foo, t_u8));
    DO(test_bin_op_bad(op_id, t_foo, t_string));
    DO(test_bin_op_bad(op_id, t_i16, t_foo));
    DO(test_bin_op_bad(op_id, t_string, t_foo));
    DO(test_bin_op_bad(op_id, t_foo, t_foo));
  }

  void standard_shift(token_id op_id)
  {
    DO(test_bin_op(op_id, t_u_lit, t_u_lit, t_u_lit));

    // TODO: Really? 1 << 3.2 is allowed?
    //DO(test_bin_op_bad(op_id,
    //  t_u_lit, t_f_lit));

    DO(test_bin_op_bad(op_id, t_f_lit, t_u_lit));

    DO(test_bin_op(op_id, t_u32, t_u_lit, t_u32));
    DO(test_bin_op(op_id, t_u_lit, t_i32, t_i32));

    DO(test_bin_op(op_id, t_u8, t_u8, t_u8));
    DO(test_bin_op(op_id, t_u16, t_u16, t_u16));
    DO(test_bin_op(op_id, t_u32, t_u32, t_u32));
    DO(test_bin_op(op_id, t_u64, t_u64, t_u64));
    DO(test_bin_op(op_id, t_u128, t_u128, t_u128));
    DO(test_bin_op(op_id, t_i8, t_i8, t_i8));
    DO(test_bin_op(op_id, t_i16, t_i16, t_i16));
    DO(test_bin_op(op_id, t_i32, t_i32, t_i32));
    DO(test_bin_op(op_id, t_i64, t_i64, t_i64));
    DO(test_bin_op(op_id, t_i128, t_i128, t_i128));

    DO(test_bin_op_bad(op_id, t_u8, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_u8));
    DO(test_bin_op_bad(op_id, t_u8, t_bool));
    DO(test_bin_op_bad(op_id, t_bool, t_u8));

    // TODO: Why do we require both operands to be the same type?
    DO(test_bin_op_bad(op_id, t_u8, t_u16));
    DO(test_bin_op_bad(op_id, t_u8, t_i8));

    DO(test_bin_op_bad(op_id, t_f32, t_i32));
    DO(test_bin_op_bad(op_id, t_string, t_i32));
    DO(test_bin_op_bad(op_id, t_f32, t_string));
    DO(test_bin_op_bad(op_id, t_string, t_string));
    DO(test_bin_op_bad(op_id, t_foo, t_u8));
    DO(test_bin_op_bad(op_id, t_foo, t_string));
    DO(test_bin_op_bad(op_id, t_i16, t_foo));
    DO(test_bin_op_bad(op_id, t_string, t_foo));
    DO(test_bin_op_bad(op_id, t_foo, t_foo));
  }

  void standard_comparison(token_id op_id, const char* fn_name)
  {
    /*
    DO(test_bin_op(op_id, t_u_lit, t_u_lit, t_bool));
    DO(test_bin_op(op_id, t_u_lit, t_f_lit, t_bool));
    DO(test_bin_op(op_id, t_f_lit, t_u_lit, t_bool));
    DO(test_bin_op(op_id, t_f_lit, t_f_lit, t_bool));

    DO(test_bin_op(op_id, t_u32, t_u_lit, t_bool));
    DO(test_bin_op(op_id, t_u_lit, t_i32, t_bool));
    DO(test_bin_op(op_id, t_f32, t_f_lit, t_bool));
    DO(test_bin_op(op_id, t_f_lit, t_f32, t_bool));

    DO(test_bin_op(op_id, t_u8, t_u8, t_bool));
    DO(test_bin_op(op_id, t_u16, t_u16, t_bool));
    DO(test_bin_op(op_id, t_u32, t_u32, t_bool));
    DO(test_bin_op(op_id, t_u64, t_u64, t_bool));
    DO(test_bin_op(op_id, t_u128, t_u128, t_bool));
    DO(test_bin_op(op_id, t_i8, t_i8, t_bool));
    DO(test_bin_op(op_id, t_i16, t_i16, t_bool));
    DO(test_bin_op(op_id, t_i32, t_i32, t_bool));
    DO(test_bin_op(op_id, t_i64, t_i64, t_bool));
    DO(test_bin_op(op_id, t_i128, t_i128, t_bool));
    DO(test_bin_op(op_id, t_f32, t_f32, t_bool));
    DO(test_bin_op(op_id, t_f64, t_f64, t_bool));

    // TODO: non arithmetic primitives should use ==, not eq()
    //DO(test_bin_op(op_id, t_none, t_none, t_bool));

    DO(test_bin_op_bad(op_id, t_u8, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_u8));

    // TODO: These work for == and !=. This has to be an oversight, surely?
    DO(test_bin_op_bad(op_id, t_u8, t_bool));
    DO(test_bin_op_bad(op_id, t_bool, t_u8));
    DO(test_bin_op_bad(op_id, t_bool, t_foo));
    DO(test_bin_op_bad(op_id, t_foo, t_bool));

    DO(test_bin_op_bad(op_id, t_u8, t_u16));
    DO(test_bin_op_bad(op_id, t_u8, t_i8));
    DO(test_bin_op_bad(op_id, t_f32, t_i32));
    DO(test_bin_op_bad(op_id, t_f32, t_string));
    DO(test_bin_op_bad(op_id, t_i16, t_foo));

    DO(test_bin_op_bad(op_id, t_string, t_i32));
    DO(test_bin_op_bad(op_id, t_foo, t_u8));
    DO(test_bin_op_bad(op_id, t_foo, t_string));
    DO(test_bin_op_bad(op_id, t_string, t_foo));
    DO(test_bin_op_bad(op_id, t_foo, t_foo));

    DO(test_bin_op_to_fun(op_id, t_fns, t_fns, t_bool, fn_name));
    */
  }

  void standard_logical(token_id op_id)
  {
    DO(test_bin_op(op_id, t_u_lit, t_u_lit, t_u_lit));

    DO(test_bin_op(op_id, t_u32, t_u_lit, t_u32));
    DO(test_bin_op(op_id, t_u_lit, t_i32, t_i32));

    DO(test_bin_op(op_id, t_u8, t_u8, t_u8));
    DO(test_bin_op(op_id, t_u16, t_u16, t_u16));
    DO(test_bin_op(op_id, t_u32, t_u32, t_u32));
    DO(test_bin_op(op_id, t_u64, t_u64, t_u64));
    DO(test_bin_op(op_id, t_u128, t_u128, t_u128));
    DO(test_bin_op(op_id, t_i8, t_i8, t_i8));
    DO(test_bin_op(op_id, t_i16, t_i16, t_i16));
    DO(test_bin_op(op_id, t_i32, t_i32, t_i32));
    DO(test_bin_op(op_id, t_i64, t_i64, t_i64));
    DO(test_bin_op(op_id, t_i128, t_i128, t_i128));

    DO(test_bin_op_bad(op_id, t_u_lit, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_u_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_f_lit));

    DO(test_bin_op_bad(op_id, t_u8, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_u8));
    DO(test_bin_op_bad(op_id, t_u8, t_bool));
    DO(test_bin_op_bad(op_id, t_bool, t_u8));
    DO(test_bin_op_bad(op_id, t_u8, t_u16));
    DO(test_bin_op_bad(op_id, t_u8, t_i8));
    DO(test_bin_op_bad(op_id, t_f32, t_i32));
    DO(test_bin_op_bad(op_id, t_f32, t_f32));
    DO(test_bin_op_bad(op_id, t_f64, t_f64));
    DO(test_bin_op_bad(op_id, t_f32, t_u_lit));
    DO(test_bin_op_bad(op_id, t_u_lit, t_f32));
    DO(test_bin_op_bad(op_id, t_f32, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_f32));
    DO(test_bin_op_bad(op_id, t_u32, t_string));
    DO(test_bin_op_bad(op_id, t_f32, t_string));
    DO(test_bin_op_bad(op_id, t_string, t_i32));
    DO(test_bin_op_bad(op_id, t_string, t_string));
    DO(test_bin_op_bad(op_id, t_foo, t_u8));
    DO(test_bin_op_bad(op_id, t_foo, t_string));
    DO(test_bin_op_bad(op_id, t_string, t_foo));
    DO(test_bin_op_bad(op_id, t_foo, t_foo));
  }

  void standard_identity(token_id op_id, token_id alt_id)
  {
    DO(test_bin_op_replace(op_id, t_u_lit, t_u_lit, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_u_lit, t_f_lit, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_f_lit, t_u_lit, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_f_lit, t_f_lit, t_bool, alt_id));

    DO(test_bin_op_replace(op_id, t_u32, t_u_lit, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_u_lit, t_i32, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_f32, t_f_lit, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_f_lit, t_f32, t_bool, alt_id));

    DO(test_bin_op_replace(op_id, t_u8, t_u8, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_u16, t_u16, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_u32, t_u32, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_u64, t_u64, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_u128, t_u128, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_i8, t_i8, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_i16, t_i16, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_i32, t_i32, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_i64, t_i64, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_i128, t_i128, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_f32, t_f32, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_f64, t_f64, t_bool, alt_id));
    DO(test_bin_op_replace(op_id, t_bool, t_bool, t_bool, alt_id));

    // TODO: Foo is math-compatible with itself. Really?
    //DO(test_bin_op(op_id, t_foo, t_foo, t_bool, op_id));

    // Bar provides T1
    DO(test_bin_op(op_id, t_bar, t_t1, t_bool));
    DO(test_bin_op(op_id, t_t1, t_bar, t_bool));
    DO(test_bin_op(op_id, t_t1, t_t2, t_bool));

    DO(test_bin_op(op_id, t_bar, t_structural, t_bool));
    DO(test_bin_op(op_id, t_structural, t_bar, t_bool));
    DO(test_bin_op(op_id, t_t1, t_structural, t_bool));
    DO(test_bin_op(op_id, t_structural, t_t1, t_bool));

    DO(test_bin_op_bad(op_id, t_u8, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_u8));
    DO(test_bin_op_bad(op_id, t_u8, t_bool));
    DO(test_bin_op_bad(op_id, t_bool, t_u8));
    DO(test_bin_op_bad(op_id, t_u8, t_u16));
    DO(test_bin_op_bad(op_id, t_u8, t_i8));
    DO(test_bin_op_bad(op_id, t_f32, t_i32));
    DO(test_bin_op_bad(op_id, t_string, t_i32));
    DO(test_bin_op_bad(op_id, t_f32, t_string));
    DO(test_bin_op_bad(op_id, t_foo, t_u8));
    DO(test_bin_op_bad(op_id, t_foo, t_string));
    DO(test_bin_op_bad(op_id, t_i16, t_foo));
    DO(test_bin_op_bad(op_id, t_string, t_foo));

    DO(test_bin_op_bad(op_id, t_foo, t_bar));
    DO(test_bin_op_bad(op_id, t_foo, t_t1));
    DO(test_bin_op_bad(op_id, t_t1, t_foo));
    DO(test_bin_op_bad(op_id, t_foo, t_structural));
    DO(test_bin_op_bad(op_id, t_structural, t_foo));
  }
};


// Test cases

// Arithmetic

TEST_F(ExprOperatorTest, Plus)
{
  DO(test_bin_op(TK_PLUS, t_u_lit, t_u_lit, t_u_lit));
  DO(standard_arithmetic(TK_PLUS));

  // TODO: string concatenation isn't implemented yet
  //DO(test_bin_op(TK_PLUS, t_string, t_string, t_string));
}


TEST_F(ExprOperatorTest, Minus)
{
  // TODO: Is this sensible?
  DO(test_bin_op(TK_MINUS,
    t_u_lit, t_u_lit, t_s_lit));

  DO(standard_arithmetic(TK_MINUS));
  DO(test_bin_op_bad(TK_MINUS, t_string, t_string));
}


TEST_F(ExprOperatorTest, Multiply)
{
  DO(test_bin_op(TK_MULTIPLY,
    t_u_lit, t_u_lit, t_u_lit));

  DO(standard_arithmetic(TK_MULTIPLY));
  DO(test_bin_op_bad(TK_MULTIPLY, t_string, t_string));
}


TEST_F(ExprOperatorTest, Divide)
{
  DO(test_bin_op(TK_DIVIDE,
    t_u_lit, t_u_lit, t_u_lit));

  DO(standard_arithmetic(TK_DIVIDE));
  DO(test_bin_op_bad(TK_DIVIDE, t_string, t_string));
}


TEST_F(ExprOperatorTest, Mod)
{
  DO(test_bin_op(TK_MOD,
    t_u_lit, t_u_lit, t_u_lit));

  DO(standard_arithmetic(TK_MOD));
  DO(test_bin_op_bad(TK_MOD, t_string, t_string));
}


// TODO: This fails (and should be our current definitions). Is this sensible?
TEST_F(ExprOperatorTest, PlusSIntLiteralAndU8)
{
  /*
  const char* prog =
    "class Foo"
    "  fun ref bar(x:U8)=>"
    "    x + -1";

  const char* expected =
    "(+"
    "  (paramref (id x) [(nominal  x (id U8) x val x)])"
    "  (1 [(nominal  x (id SIntLiteral) x val x)])"
    "  [(nominal  x (id U8) x val x)])";

  DO(type_check_and_compare_ast(prog, TK_PLUS, expected));
  */
}


TEST_F(ExprOperatorTest, UnaryMinus)
{
  DO(test_unary_op(TK_UNARY_MINUS, t_f_lit, t_f_lit));
  DO(test_unary_op(TK_UNARY_MINUS, t_u_lit, t_s_lit));

  DO(test_unary_op(TK_UNARY_MINUS, t_u8, t_u8));
  DO(test_unary_op(TK_UNARY_MINUS, t_u16, t_u16));
  DO(test_unary_op(TK_UNARY_MINUS, t_u32, t_u32));
  DO(test_unary_op(TK_UNARY_MINUS, t_u64, t_u64));
  DO(test_unary_op(TK_UNARY_MINUS, t_u128, t_u128));
  DO(test_unary_op(TK_UNARY_MINUS, t_i8, t_i8));
  DO(test_unary_op(TK_UNARY_MINUS, t_i16, t_i16));
  DO(test_unary_op(TK_UNARY_MINUS, t_i32, t_i32));
  DO(test_unary_op(TK_UNARY_MINUS, t_i64, t_i64));
  DO(test_unary_op(TK_UNARY_MINUS, t_i128, t_i128));
  DO(test_unary_op(TK_UNARY_MINUS, t_f32, t_f32));
  DO(test_unary_op(TK_UNARY_MINUS, t_f64, t_f64));

  DO(test_unary_op_bad(TK_UNARY_MINUS, t_bool));
  DO(test_unary_op_bad(TK_UNARY_MINUS, t_string));
  DO(test_unary_op_bad(TK_UNARY_MINUS, t_foo));
}


// Shift

TEST_F(ExprOperatorTest, LeftShift)
{
  DO(standard_shift(TK_LSHIFT));
}


TEST_F(ExprOperatorTest, RightShift)
{
  DO(standard_shift(TK_RSHIFT));
}


// Comparison

TEST_F(ExprOperatorTest, Equal)
{
  DO(standard_comparison(TK_EQ, "eq"));
  DO(test_bin_op(TK_EQ, t_bool, t_bool, t_bool));
}


TEST_F(ExprOperatorTest, NotEqual)
{
  DO(standard_comparison(TK_NE, "ne"));
  DO(test_bin_op(TK_NE, t_bool, t_bool, t_bool));
}


TEST_F(ExprOperatorTest, LessThan)
{
  DO(standard_comparison(TK_LT, "lt"));
  DO(test_bin_op_bad(TK_LT, t_bool, t_bool));
}


TEST_F(ExprOperatorTest, LessThanEqual)
{
  DO(standard_comparison(TK_LE, "le"));
  DO(test_bin_op_bad(TK_LE, t_bool, t_bool));
}


TEST_F(ExprOperatorTest, GreaterThan)
{
  DO(standard_comparison(TK_GT, "gt"));
  DO(test_bin_op_bad(TK_GT, t_bool, t_bool));
}


TEST_F(ExprOperatorTest, GreaterThanEqual)
{
  DO(standard_comparison(TK_GE, "ge"));
  DO(test_bin_op_bad(TK_GE, t_bool, t_bool));
}


// Logical

TEST_F(ExprOperatorTest, And)
{
  DO(standard_logical(TK_AND));

  // TODO: Bool and Bool shortcut
}


TEST_F(ExprOperatorTest, Or)
{
  DO(standard_logical(TK_OR));

  // TODO: Bool or Bool shortcut
}


TEST_F(ExprOperatorTest, Xor)
{
  DO(standard_logical(TK_XOR));

  DO(test_bin_op(TK_XOR, t_bool, t_bool, t_bool));
}


TEST_F(ExprOperatorTest, Not)
{
  DO(test_unary_op(TK_NOT, t_u_lit, t_u_lit));

  DO(test_unary_op(TK_NOT, t_u8, t_u8));
  DO(test_unary_op(TK_NOT, t_u16, t_u16));
  DO(test_unary_op(TK_NOT, t_u32, t_u32));
  DO(test_unary_op(TK_NOT, t_u64, t_u64));
  DO(test_unary_op(TK_NOT, t_u128, t_u128));
  DO(test_unary_op(TK_NOT, t_i8, t_i8));
  DO(test_unary_op(TK_NOT, t_i16, t_i16));
  DO(test_unary_op(TK_NOT, t_i32, t_i32));
  DO(test_unary_op(TK_NOT, t_i64, t_i64));
  DO(test_unary_op(TK_NOT, t_i128, t_i128));
  DO(test_unary_op(TK_NOT, t_bool, t_bool));

  // TODO: We allow negation of floats? What does that even mean?
  //DO(test_unary_op_bad(TK_NOT, t_f_lit));
  //DO(test_unary_op_bad(TK_NOT, t_f32));
  //DO(test_unary_op_bad(TK_NOT, t_f64));

  DO(test_unary_op_bad(TK_NOT, t_string));
  DO(test_unary_op_bad(TK_NOT, t_foo));
}


// Identity test

TEST_F(ExprOperatorTest, Is)
{
  DO(standard_identity(TK_IS, TK_EQ));
}


TEST_F(ExprOperatorTest, Isnt)
{
  DO(standard_identity(TK_ISNT, TK_NE));
}


// TODO: Writing to function parameters (fail)
// TODO: TK_ASSIGN, TK_CONSUME, TK_RECOVER
