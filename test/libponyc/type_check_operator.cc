/*
#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/lexer.h>
#include <pass/expr.h>
#include <ast/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>

#include "util.h"
#include "builtin_ast.h"
#include <stdio.h>


class ExprOperatorTest : public testing::Test
{
protected:
  builder_t* builder;
  char op_buf[1000];

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


  // Standard tests

  void test_bin_op_bad(token_id op_id, const char* type_a,
    const char* type_b)
  {
    snprintf(op_buf, sizeof(op_buf), "(%s (fvarref (id a) [%s])(fvarref (id b) [%s]))",
      lexer_print(op_id), type_a, type_b);

    ast_t* op_ast = builder_add(builder, "test", op_buf);
    ASSERT_NE((void*)NULL, op_ast);

    ASSERT_EQ(AST_FATAL, pass_expr(&op_ast, NULL));
  }

  void test_bin_op(token_id op_id, const char* type_a, const char* type_b,
    const char* type_result)
  {
    snprintf(op_buf, sizeof(op_buf), "(%s (fvarref (id a) [%s])(fvarref (id b) [%s]))",
      lexer_print(op_id), type_a, type_b);

    ast_t* op_ast = builder_add(builder, "test", op_buf);
    ASSERT_NE((void*)NULL, op_ast);

    ASSERT_EQ(AST_OK, pass_expr(&op_ast, NULL));
    ASSERT_NE((void*)NULL, op_ast);

    ASSERT_EQ(op_id, ast_id(op_ast));

    ast_t* expected_ast = builder_add(builder, "test", type_result);
    ASSERT_TRUE(build_compare_asts_no_sibling(expected_ast, ast_type(op_ast)));
  }


  // Test groups

  void standard_identity(token_id op_id)
  {
    // TODO(andy): literals
    //DO(test_bin_op_bad(op_id, t_u_lit, t_u_lit));
    //DO(test_bin_op_bad(op_id, t_u_lit, t_f_lit));
    //DO(test_bin_op_bad(op_id, t_f_lit, t_u_lit));
    //DO(test_bin_op_bad(op_id, t_f_lit, t_f_lit));

    //DO(test_bin_op_bad(op_id, t_u32, t_u_lit));
    //DO(test_bin_op_bad(op_id, t_u_lit, t_i32));
    //DO(test_bin_op_bad(op_id, t_f32, t_f_lit));
    //DO(test_bin_op_bad(op_id, t_f_lit, t_f32));

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
    DO(test_bin_op(op_id, t_bool, t_bool, t_bool));

    // Bar provides T1
    DO(test_bin_op(op_id, t_bar, t_t1, t_bool));
    DO(test_bin_op(op_id, t_t1, t_bar, t_bool));
    DO(test_bin_op(op_id, t_t1, t_t2, t_bool));

    // Bar looks like Iface
    DO(test_bin_op(op_id, t_bar, t_iface, t_bool));
    DO(test_bin_op(op_id, t_iface, t_bar, t_bool));
    DO(test_bin_op(op_id, t_t1, t_iface, t_bool));
    DO(test_bin_op(op_id, t_iface, t_t1, t_bool));

    DO(test_bin_op(op_id, t_u8, t_bool, t_bool));
    DO(test_bin_op(op_id, t_bool, t_u8, t_bool));
    DO(test_bin_op(op_id, t_u8, t_u16, t_bool));
    DO(test_bin_op(op_id, t_u8, t_i8, t_bool));
    DO(test_bin_op(op_id, t_f32, t_i32, t_bool));
    DO(test_bin_op(op_id, t_string, t_i32, t_bool));
    DO(test_bin_op(op_id, t_f32, t_string, t_bool));
    DO(test_bin_op(op_id, t_foo, t_u8, t_bool));
    DO(test_bin_op(op_id, t_foo, t_string, t_bool));
    DO(test_bin_op(op_id, t_i16, t_foo, t_bool));
    DO(test_bin_op(op_id, t_string, t_foo, t_bool));

    DO(test_bin_op(op_id, t_foo, t_bar, t_bool));
    DO(test_bin_op(op_id, t_foo, t_t1, t_bool));
    DO(test_bin_op(op_id, t_t1, t_foo, t_bool));
    DO(test_bin_op(op_id, t_foo, t_iface, t_bool));
    DO(test_bin_op(op_id, t_iface, t_foo, t_bool));
  }
};


// Test cases


// Identity test

TEST_F(ExprOperatorTest, Is)
{
  DO(standard_identity(TK_IS));
}


TEST_F(ExprOperatorTest, Isnt)
{
  DO(standard_identity(TK_ISNT));
}


// TODO: Writing to function parameters (fail)
// TODO: TK_ASSIGN, TK_CONSUME, TK_RECOVER

*/
