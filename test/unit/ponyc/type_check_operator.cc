#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/lexer.h>
#include <pass/expr.h>
#include <ds/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>

#include "util.h"
#include "builtin_ast.h"
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
    ASSERT_EQ(AST_FATAL, pass_expr(&op_ast, NULL));
  }

  void test_bin_op(token_id op_id, const char* type_a, const char* type_b,
    const char* type_result)
  {
    snprintf(op_buf, sizeof(op_buf), "(%s (fvarref (id a) [%s])(fvarref (id b) [%s]))",
      lexer_print(op_id), type_a, type_b);

    DO(add_op());
    ASSERT_EQ(AST_OK, pass_expr(&op_ast, NULL));
    ASSERT_EQ(op_id, ast_id(op_ast));
    DO(get_op_type());
    DO(compare_result(type_result, op_type));
  }

  void test_bin_op_to_fun(token_id op_id, const char* type_a,
    const char* type_b, const char* type_result, const char* fn_name)
  {
    ASSERT_NE((void*)NULL, builder_add(builder, "test", fns_desc));

    snprintf(op_buf, sizeof(op_buf), "(%s (fvarref (id a) [%s])(fvarref (id b) [%s]))",
      lexer_print(op_id), type_a, type_b);

    DO(add_op());
    ASSERT_EQ(AST_OK, pass_expr(&op_ast, NULL));

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

    ASSERT_EQ(AST_FATAL, pass_expr(&op_ast, NULL));
  }

  void test_unary_op(token_id op_id, const char* type_op,
    const char* type_result)
  {
    snprintf(op_buf, sizeof(op_buf), "(- (fvarref (id op) [%s]))", type_op);

    DO(add_op());
    ast_setid(op_ast, op_id);

    ASSERT_EQ(AST_OK, pass_expr(&op_ast, NULL));
    ASSERT_EQ(op_id, ast_id(op_ast));
    DO(get_op_type());

    DO(compare_result(type_result, op_type));
  }


  // Test groups

  void standard_identity(token_id op_id)
  {
    DO(test_bin_op_bad(op_id, t_u_lit, t_u_lit));
    DO(test_bin_op_bad(op_id, t_u_lit, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_u_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_f_lit));

    DO(test_bin_op_bad(op_id, t_u32, t_u_lit));
    DO(test_bin_op_bad(op_id, t_u_lit, t_i32));
    DO(test_bin_op_bad(op_id, t_f32, t_f_lit));
    DO(test_bin_op_bad(op_id, t_f_lit, t_f32));

    DO(test_bin_op_bad(op_id, t_u8, t_u8));
    DO(test_bin_op_bad(op_id, t_u16, t_u16));
    DO(test_bin_op_bad(op_id, t_u32, t_u32));
    DO(test_bin_op_bad(op_id, t_u64, t_u64));
    DO(test_bin_op_bad(op_id, t_u128, t_u128));
    DO(test_bin_op_bad(op_id, t_i8, t_i8));
    DO(test_bin_op_bad(op_id, t_i16, t_i16));
    DO(test_bin_op_bad(op_id, t_i32, t_i32));
    DO(test_bin_op_bad(op_id, t_i64, t_i64));
    DO(test_bin_op_bad(op_id, t_i128, t_i128));
    DO(test_bin_op_bad(op_id, t_f32, t_f32));
    DO(test_bin_op_bad(op_id, t_f64, t_f64));
    DO(test_bin_op_bad(op_id, t_bool, t_bool));

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
