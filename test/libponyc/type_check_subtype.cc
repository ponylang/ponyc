#include <gtest/gtest.h>
#include <platform.h>

#include <ast/builder.h>
#include <type/subtype.h>

#include "util.h"
#include "builtin_ast.h"
#include <stdio.h>

class SubTypeTest: public testing::Test
{
protected:
  builder_t* builder;

  virtual void SetUp()
  {
    DO(build_ast_from_string(builtin, NULL, &builder));
    ASSERT_NE((void*)NULL, builder);
  }

  virtual void TearDown()
  {
    builder_free(builder);
  }

  virtual void test_unary(bool (*fn)(ast_t*), const char* type, bool expect)
  {
    ast_t* ast = builder_add(builder, "test", type);
    ASSERT_NE((void*)NULL, ast);

    ASSERT_EQ(expect, fn(ast));
  }

  virtual void test_binary(bool(*fn)(ast_t*, ast_t*), const char* type_a,
    const char* type_b, bool expect)
  {
    ast_t* ast_a = builder_add(builder, "test", type_a);
    ASSERT_NE((void*)NULL, ast_a);

    ast_t* ast_b = builder_add(builder, "test", type_b);
    ASSERT_NE((void*)NULL, ast_b);

    ASSERT_EQ(expect, fn(ast_a, ast_b));
  }
};


// TODO: is_subtype, is_literal, is_builtin, is_id_compatible

TEST_F(SubTypeTest, IsSubType)
{
  DO(test_binary(is_subtype, t_bar, t_t1, true));
  DO(test_binary(is_subtype, t_bar, t_t2, false));
  DO(test_binary(is_subtype, t_bar, t_t3, true));
  DO(test_binary(is_subtype, t_bar, t_iface, true));
  DO(test_binary(is_subtype, t_foo, t_iface, false));

  DO(test_binary(is_subtype, t_bool, t_bool, true));
  DO(test_binary(is_subtype, t_string, t_string, true));
  DO(test_binary(is_subtype, t_none, t_none, true));
  DO(test_binary(is_subtype, t_foo, t_foo, true));
  DO(test_binary(is_subtype, t_t1, t_t1, true));

  DO(test_binary(is_subtype, t_u8, t_u8, true));
  DO(test_binary(is_subtype, t_u16, t_u16, true));
  DO(test_binary(is_subtype, t_u32, t_u32, true));
  DO(test_binary(is_subtype, t_u64, t_u64, true));
  DO(test_binary(is_subtype, t_u128, t_u128, true));
  DO(test_binary(is_subtype, t_i8, t_i8, true));
  DO(test_binary(is_subtype, t_i16, t_i16, true));
  DO(test_binary(is_subtype, t_i32, t_i32, true));
  DO(test_binary(is_subtype, t_i64, t_i64, true));
  DO(test_binary(is_subtype, t_i128, t_i128, true));
  DO(test_binary(is_subtype, t_f32, t_f32, true));
  DO(test_binary(is_subtype, t_f64, t_f64, true));

  DO(test_binary(is_subtype, t_u8, t_u16, false));
  DO(test_binary(is_subtype, t_i16, t_i32, false));
  DO(test_binary(is_subtype, t_f32, t_f64, false));
  DO(test_binary(is_subtype, t_u8, t_i8, false));
  DO(test_binary(is_subtype, t_i32, t_f32, false));

  DO(test_binary(is_subtype, t_u8, t_bool, false));
  DO(test_binary(is_subtype, t_i32, t_none, false));
  DO(test_binary(is_subtype, t_f32, t_string, false));
  DO(test_binary(is_subtype, t_t1, t_u32, false));
  DO(test_binary(is_subtype, t_foo, t_i32, false));
  DO(test_binary(is_subtype, t_iface, t_u32, false));

  // TODO(andy): unions, intersects, tuples
}


TEST_F(SubTypeTest, IsEqType)
{
  DO(test_binary(is_eqtype, t_u8, t_u8, true));
  DO(test_binary(is_eqtype, t_i16, t_i16, true));
  DO(test_binary(is_eqtype, t_f32, t_f32, true));

  DO(test_binary(is_eqtype, t_bool, t_bool, true));
  DO(test_binary(is_eqtype, t_string, t_string, true));
  DO(test_binary(is_eqtype, t_none, t_none, true));
  DO(test_binary(is_eqtype, t_foo, t_foo, true));
  DO(test_binary(is_eqtype, t_t1, t_t1, true));
  DO(test_binary(is_eqtype, t_iface, t_iface, true));

  DO(test_binary(is_eqtype, t_u8, t_u16, false));
  DO(test_binary(is_eqtype, t_i16, t_i32, false));
  DO(test_binary(is_eqtype, t_f32, t_f64, false));
  DO(test_binary(is_eqtype, t_u8, t_i8, false));
  DO(test_binary(is_eqtype, t_i32, t_f32, false));

  DO(test_binary(is_eqtype, t_u8, t_bool, false));
  DO(test_binary(is_eqtype, t_i32, t_none, false));
  DO(test_binary(is_eqtype, t_f32, t_string, false));
  DO(test_binary(is_eqtype, t_t1, t_u32, false));
  DO(test_binary(is_eqtype, t_t1, t_t2, false));
  DO(test_binary(is_eqtype, t_bar, t_t1, false));
  DO(test_binary(is_eqtype, t_foo, t_i32, false));
  DO(test_binary(is_eqtype, t_iface, t_u32, false));
}


TEST_F(SubTypeTest, IsBool)
{
  DO(test_unary(is_bool, t_bool, true));
  DO(test_unary(is_bool, t_u8, false));
  DO(test_unary(is_bool, t_u16, false));
  DO(test_unary(is_bool, t_u32, false));
  DO(test_unary(is_bool, t_u64, false));
  DO(test_unary(is_bool, t_u128, false));
  DO(test_unary(is_bool, t_i8, false));
  DO(test_unary(is_bool, t_i16, false));
  DO(test_unary(is_bool, t_i32, false));
  DO(test_unary(is_bool, t_i64, false));
  DO(test_unary(is_bool, t_i128, false));
  DO(test_unary(is_bool, t_f32, false));
  DO(test_unary(is_bool, t_f64, false));
  DO(test_unary(is_bool, t_none, false));
  DO(test_unary(is_bool, t_string, false));
  DO(test_unary(is_bool, t_t1, false));
  DO(test_unary(is_bool, t_foo, false));
  DO(test_unary(is_bool, t_iface, false));
}


TEST_F(SubTypeTest, IsSigned)
{
  DO(test_unary(is_signed, t_bool, false));
  DO(test_unary(is_signed, t_u8, false));
  DO(test_unary(is_signed, t_u16, false));
  DO(test_unary(is_signed, t_u32, false));
  DO(test_unary(is_signed, t_u64, false));
  DO(test_unary(is_signed, t_u128, false));
  DO(test_unary(is_signed, t_i8, true));
  DO(test_unary(is_signed, t_i16, true));
  DO(test_unary(is_signed, t_i32, true));
  DO(test_unary(is_signed, t_i64, true));
  DO(test_unary(is_signed, t_i128, true));
  DO(test_unary(is_signed, t_f32, false));
  DO(test_unary(is_signed, t_f64, false));
  DO(test_unary(is_signed, t_none, false));
  DO(test_unary(is_signed, t_string, false));
  DO(test_unary(is_signed, t_t1, false));
  DO(test_unary(is_signed, t_foo, false));
  DO(test_unary(is_signed, t_iface, false));
}
