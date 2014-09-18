#include <platform/platform.h>

PONY_EXTERN_C_BEGIN
#include <ast/builder.h>
#include <type/subtype.h>
PONY_EXTERN_C_END

#include "util.h"
#include "builtin_ast.h"
#include <gtest/gtest.h>
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


// TODO: is_subtype, is_literal, is_builtin, is_id_compatible, is_match_compatible

TEST_F(SubTypeTest, IsSubType)
{
  DO(test_binary(is_subtype, t_u_lit, t_u_lit, true));
  DO(test_binary(is_subtype, t_bar, t_structural, true));


  /* TODO: Everything else
  DO(test_binary(is_subtype, t_u_lit, t_u_lit, true));
  DO(test_binary(is_subtype, t_s_lit, t_s_lit, true));
  DO(test_binary(is_subtype, t_f_lit, t_f_lit, true));
  DO(test_binary(is_subtype, t_u_lit, t_s_lit, true));
  DO(test_binary(is_subtype, t_s_lit, t_u_lit, true));
  DO(test_binary(is_subtype, t_f_lit, t_u_lit, true));
  DO(test_binary(is_subtype, t_f_lit, t_s_lit, true));
  DO(test_binary(is_subtype, t_u_lit, t_f_lit, true));
  DO(test_binary(is_subtype, t_s_lit, t_f_lit, true));

  // TODO: Should Bools (and True and False) be math compatible?
  // What about String, None, Foo, etc?
  DO(test_binary(is_math_compatible, t_true, t_true, true));
  DO(test_binary(is_math_compatible, t_true, t_false, false));
  DO(test_binary(is_math_compatible, t_bool, t_bool, false));
  DO(test_binary(is_math_compatible, t_string, t_string, true));
  DO(test_binary(is_math_compatible, t_none, t_none, true));
  DO(test_binary(is_math_compatible, t_foo, t_foo, true));
  DO(test_binary(is_math_compatible, t_t1, t_t1, true));

  DO(test_binary(is_math_compatible, t_u8, t_u8, true));
  DO(test_binary(is_math_compatible, t_u16, t_u16, true));
  DO(test_binary(is_math_compatible, t_u32, t_u32, true));
  DO(test_binary(is_math_compatible, t_u64, t_u64, true));
  DO(test_binary(is_math_compatible, t_u128, t_u128, true));
  DO(test_binary(is_math_compatible, t_i8, t_i8, true));
  DO(test_binary(is_math_compatible, t_i16, t_i16, true));
  DO(test_binary(is_math_compatible, t_i32, t_i32, true));
  DO(test_binary(is_math_compatible, t_i64, t_i64, true));
  DO(test_binary(is_math_compatible, t_i128, t_i128, true));
  DO(test_binary(is_math_compatible, t_f32, t_f32, true));
  DO(test_binary(is_math_compatible, t_f64, t_f64, true));

  DO(test_binary(is_math_compatible, t_u_lit, t_u8, true));
  DO(test_binary(is_math_compatible, t_u_lit, t_i16, true));
  DO(test_binary(is_math_compatible, t_u_lit, t_f32, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_u8, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_i16, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_f32, true));
  DO(test_binary(is_math_compatible, t_f_lit, t_u8, false));
  DO(test_binary(is_math_compatible, t_f_lit, t_i16, false));
  DO(test_binary(is_math_compatible, t_f_lit, t_f32, true));

  DO(test_binary(is_math_compatible, t_u8, t_u16, false));
  DO(test_binary(is_math_compatible, t_i16, t_i32, false));
  DO(test_binary(is_math_compatible, t_f32, t_f64, false));
  DO(test_binary(is_math_compatible, t_u8, t_i8, false));
  DO(test_binary(is_math_compatible, t_i32, t_f32, false));

  DO(test_binary(is_math_compatible, t_u8, t_bool, false));
  DO(test_binary(is_math_compatible, t_i32, t_none, false));
  DO(test_binary(is_math_compatible, t_f32, t_string, false));
  DO(test_binary(is_math_compatible, t_t1, t_u32, false));
  DO(test_binary(is_math_compatible, t_foo, t_i32, false));
  DO(test_binary(is_math_compatible, t_structural, t_u32, false));
*/
}


TEST_F(SubTypeTest, IsEqType)
{
  DO(test_binary(is_eqtype, t_u_lit, t_u_lit, true));
  DO(test_binary(is_eqtype, t_s_lit, t_s_lit, true));
  DO(test_binary(is_eqtype, t_f_lit, t_f_lit, true));
  DO(test_binary(is_eqtype, t_u_lit, t_s_lit, true));
  DO(test_binary(is_eqtype, t_s_lit, t_u_lit, true));
  DO(test_binary(is_eqtype, t_f_lit, t_u_lit, false));
  DO(test_binary(is_eqtype, t_f_lit, t_s_lit, false));
  DO(test_binary(is_eqtype, t_u_lit, t_f_lit, false));
  DO(test_binary(is_eqtype, t_s_lit, t_f_lit, false));

  DO(test_binary(is_eqtype, t_u8, t_u8, true));
  DO(test_binary(is_eqtype, t_i16, t_i16, true));
  DO(test_binary(is_eqtype, t_f32, t_f32, true));

  DO(test_binary(is_eqtype, t_true, t_true, true));
  DO(test_binary(is_eqtype, t_true, t_false, false));
  DO(test_binary(is_eqtype, t_bool, t_bool, true));
  DO(test_binary(is_eqtype, t_string, t_string, true));
  DO(test_binary(is_eqtype, t_none, t_none, true));
  DO(test_binary(is_eqtype, t_foo, t_foo, true));
  DO(test_binary(is_eqtype, t_t1, t_t1, true));
  DO(test_binary(is_eqtype, t_structural, t_structural, true));

  DO(test_binary(is_eqtype, t_u_lit, t_u8, false));
  DO(test_binary(is_eqtype, t_u_lit, t_i16, false));
  DO(test_binary(is_eqtype, t_u_lit, t_f32, false));
  DO(test_binary(is_eqtype, t_s_lit, t_u8, false));
  DO(test_binary(is_eqtype, t_s_lit, t_i16, false));
  DO(test_binary(is_eqtype, t_s_lit, t_f32, false));
  DO(test_binary(is_eqtype, t_f_lit, t_u8, false));
  DO(test_binary(is_eqtype, t_f_lit, t_i16, false));
  DO(test_binary(is_eqtype, t_f_lit, t_f32, false));

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
  DO(test_binary(is_eqtype, t_structural, t_u32, false));
}


TEST_F(SubTypeTest, IsBool)
{
  DO(test_unary(is_bool, t_true, true));
  DO(test_unary(is_bool, t_false, true));
  DO(test_unary(is_bool, t_bool, true));
  DO(test_unary(is_bool, t_u_lit, false));
  DO(test_unary(is_bool, t_u8, false));
  DO(test_unary(is_bool, t_u16, false));
  DO(test_unary(is_bool, t_u32, false));
  DO(test_unary(is_bool, t_u64, false));
  DO(test_unary(is_bool, t_u128, false));
  DO(test_unary(is_bool, t_s_lit, false));
  DO(test_unary(is_bool, t_i8, false));
  DO(test_unary(is_bool, t_i16, false));
  DO(test_unary(is_bool, t_i32, false));
  DO(test_unary(is_bool, t_i64, false));
  DO(test_unary(is_bool, t_i128, false));
  DO(test_unary(is_bool, t_f_lit, false));
  DO(test_unary(is_bool, t_f32, false));
  DO(test_unary(is_bool, t_f64, false));
  DO(test_unary(is_bool, t_none, false));
  DO(test_unary(is_bool, t_string, false));
  DO(test_unary(is_bool, t_t1, false));
  DO(test_unary(is_bool, t_foo, false));
  DO(test_unary(is_bool, t_structural, false));
}


TEST_F(SubTypeTest, IsSIntLiteral)
{
  DO(test_unary(is_sintliteral, t_true, false));
  DO(test_unary(is_sintliteral, t_false, false));
  DO(test_unary(is_sintliteral, t_bool, false));
  DO(test_unary(is_sintliteral, t_u_lit, false));
  DO(test_unary(is_sintliteral, t_u8, false));
  DO(test_unary(is_sintliteral, t_u16, false));
  DO(test_unary(is_sintliteral, t_u32, false));
  DO(test_unary(is_sintliteral, t_u64, false));
  DO(test_unary(is_sintliteral, t_u128, false));
  DO(test_unary(is_sintliteral, t_s_lit, true));
  DO(test_unary(is_sintliteral, t_i8, false));
  DO(test_unary(is_sintliteral, t_i16, false));
  DO(test_unary(is_sintliteral, t_i32, false));
  DO(test_unary(is_sintliteral, t_i64, false));
  DO(test_unary(is_sintliteral, t_i128, false));
  DO(test_unary(is_sintliteral, t_f_lit, false));
  DO(test_unary(is_sintliteral, t_f32, false));
  DO(test_unary(is_sintliteral, t_f64, false));
  DO(test_unary(is_sintliteral, t_none, false));
  DO(test_unary(is_sintliteral, t_string, false));
  DO(test_unary(is_sintliteral, t_t1, false));
  DO(test_unary(is_sintliteral, t_foo, false));
  DO(test_unary(is_sintliteral, t_structural, false));
}


TEST_F(SubTypeTest, IsUIntLiteral)
{
  DO(test_unary(is_uintliteral, t_true, false));
  DO(test_unary(is_uintliteral, t_false, false));
  DO(test_unary(is_uintliteral, t_bool, false));
  DO(test_unary(is_uintliteral, t_u_lit, true));
  DO(test_unary(is_uintliteral, t_u8, false));
  DO(test_unary(is_uintliteral, t_u16, false));
  DO(test_unary(is_uintliteral, t_u32, false));
  DO(test_unary(is_uintliteral, t_u64, false));
  DO(test_unary(is_uintliteral, t_u128, false));
  DO(test_unary(is_uintliteral, t_s_lit, false));
  DO(test_unary(is_uintliteral, t_i8, false));
  DO(test_unary(is_uintliteral, t_i16, false));
  DO(test_unary(is_uintliteral, t_i32, false));
  DO(test_unary(is_uintliteral, t_i64, false));
  DO(test_unary(is_uintliteral, t_i128, false));
  DO(test_unary(is_uintliteral, t_f_lit, false));
  DO(test_unary(is_uintliteral, t_f32, false));
  DO(test_unary(is_uintliteral, t_f64, false));
  DO(test_unary(is_uintliteral, t_none, false));
  DO(test_unary(is_uintliteral, t_string, false));
  DO(test_unary(is_uintliteral, t_t1, false));
  DO(test_unary(is_uintliteral, t_foo, false));
  DO(test_unary(is_uintliteral, t_structural, false));
}


TEST_F(SubTypeTest, IsIntLiteral)
{
  DO(test_unary(is_intliteral, t_true, false));
  DO(test_unary(is_intliteral, t_false, false));
  DO(test_unary(is_intliteral, t_bool, false));
  DO(test_unary(is_intliteral, t_u_lit, true));
  DO(test_unary(is_intliteral, t_u8, false));
  DO(test_unary(is_intliteral, t_u16, false));
  DO(test_unary(is_intliteral, t_u32, false));
  DO(test_unary(is_intliteral, t_u64, false));
  DO(test_unary(is_intliteral, t_u128, false));
  DO(test_unary(is_intliteral, t_s_lit, true));
  DO(test_unary(is_intliteral, t_i8, false));
  DO(test_unary(is_intliteral, t_i16, false));
  DO(test_unary(is_intliteral, t_i32, false));
  DO(test_unary(is_intliteral, t_i64, false));
  DO(test_unary(is_intliteral, t_i128, false));
  DO(test_unary(is_intliteral, t_f_lit, false));
  DO(test_unary(is_intliteral, t_f32, false));
  DO(test_unary(is_intliteral, t_f64, false));
  DO(test_unary(is_intliteral, t_none, false));
  DO(test_unary(is_intliteral, t_string, false));
  DO(test_unary(is_intliteral, t_t1, false));
  DO(test_unary(is_intliteral, t_foo, false));
  DO(test_unary(is_intliteral, t_structural, false));
}


TEST_F(SubTypeTest, IsFloatLiteral)
{
  DO(test_unary(is_floatliteral, t_true, false));
  DO(test_unary(is_floatliteral, t_false, false));
  DO(test_unary(is_floatliteral, t_bool, false));
  DO(test_unary(is_floatliteral, t_u_lit, false));
  DO(test_unary(is_floatliteral, t_u8, false));
  DO(test_unary(is_floatliteral, t_u16, false));
  DO(test_unary(is_floatliteral, t_u32, false));
  DO(test_unary(is_floatliteral, t_u64, false));
  DO(test_unary(is_floatliteral, t_u128, false));
  DO(test_unary(is_floatliteral, t_s_lit, false));
  DO(test_unary(is_floatliteral, t_i8, false));
  DO(test_unary(is_floatliteral, t_i16, false));
  DO(test_unary(is_floatliteral, t_i32, false));
  DO(test_unary(is_floatliteral, t_i64, false));
  DO(test_unary(is_floatliteral, t_i128, false));
  DO(test_unary(is_floatliteral, t_f_lit, true));
  DO(test_unary(is_floatliteral, t_f32, false));
  DO(test_unary(is_floatliteral, t_f64, false));
  DO(test_unary(is_floatliteral, t_none, false));
  DO(test_unary(is_floatliteral, t_string, false));
  DO(test_unary(is_floatliteral, t_t1, false));
  DO(test_unary(is_floatliteral, t_foo, false));
  DO(test_unary(is_floatliteral, t_structural, false));
}


TEST_F(SubTypeTest, IsArithmetic)
{
  DO(test_unary(is_arithmetic, t_true, false));
  DO(test_unary(is_arithmetic, t_false, false));
  DO(test_unary(is_arithmetic, t_bool, false));
  DO(test_unary(is_arithmetic, t_u_lit, true));
  DO(test_unary(is_arithmetic, t_u8, true));
  DO(test_unary(is_arithmetic, t_u16, true));
  DO(test_unary(is_arithmetic, t_u32, true));
  DO(test_unary(is_arithmetic, t_u64, true));
  DO(test_unary(is_arithmetic, t_u128, true));
  DO(test_unary(is_arithmetic, t_s_lit, true));
  DO(test_unary(is_arithmetic, t_i8, true));
  DO(test_unary(is_arithmetic, t_i16, true));
  DO(test_unary(is_arithmetic, t_i32, true));
  DO(test_unary(is_arithmetic, t_i64, true));
  DO(test_unary(is_arithmetic, t_i128, true));
  DO(test_unary(is_arithmetic, t_f_lit, true));
  DO(test_unary(is_arithmetic, t_f32, true));
  DO(test_unary(is_arithmetic, t_f64, true));
  DO(test_unary(is_arithmetic, t_none, false));
  DO(test_unary(is_arithmetic, t_string, false));
  DO(test_unary(is_arithmetic, t_t1, false));
  DO(test_unary(is_arithmetic, t_foo, false));
  DO(test_unary(is_arithmetic, t_structural, false));
}


TEST_F(SubTypeTest, IsInteger)
{
  DO(test_unary(is_integer, t_true, false));
  DO(test_unary(is_integer, t_false, false));
  DO(test_unary(is_integer, t_bool, false));
  DO(test_unary(is_integer, t_u_lit, true));
  DO(test_unary(is_integer, t_u8, true));
  DO(test_unary(is_integer, t_u16, true));
  DO(test_unary(is_integer, t_u32, true));
  DO(test_unary(is_integer, t_u64, true));
  DO(test_unary(is_integer, t_u128, true));
  DO(test_unary(is_integer, t_s_lit, true));
  DO(test_unary(is_integer, t_i8, true));
  DO(test_unary(is_integer, t_i16, true));
  DO(test_unary(is_integer, t_i32, true));
  DO(test_unary(is_integer, t_i64, true));
  DO(test_unary(is_integer, t_i128, true));
  DO(test_unary(is_integer, t_f_lit, false));
  DO(test_unary(is_integer, t_f32, false));
  DO(test_unary(is_integer, t_f64, false));
  DO(test_unary(is_integer, t_none, false));
  DO(test_unary(is_integer, t_string, false));
  DO(test_unary(is_integer, t_t1, false));
  DO(test_unary(is_integer, t_foo, false));
  DO(test_unary(is_integer, t_structural, false));
}


TEST_F(SubTypeTest, IsFloat)
{
  DO(test_unary(is_float, t_true, false));
  DO(test_unary(is_float, t_false, false));
  DO(test_unary(is_float, t_bool, false));
  DO(test_unary(is_float, t_u_lit, true));
  DO(test_unary(is_float, t_u8, false));
  DO(test_unary(is_float, t_u16, false));
  DO(test_unary(is_float, t_u32, false));
  DO(test_unary(is_float, t_u64, false));
  DO(test_unary(is_float, t_u128, false));
  DO(test_unary(is_float, t_s_lit, true));
  DO(test_unary(is_float, t_i8, false));
  DO(test_unary(is_float, t_i16, false));
  DO(test_unary(is_float, t_i32, false));
  DO(test_unary(is_float, t_i64, false));
  DO(test_unary(is_float, t_i128, false));
  DO(test_unary(is_float, t_f_lit, true));
  DO(test_unary(is_float, t_f32, true));
  DO(test_unary(is_float, t_f64, true));
  DO(test_unary(is_float, t_none, false));
  DO(test_unary(is_float, t_string, false));
  DO(test_unary(is_float, t_t1, false));
  DO(test_unary(is_float, t_foo, false));
  DO(test_unary(is_float, t_structural, false));
}


TEST_F(SubTypeTest, IsSigned)
{
  DO(test_unary(is_signed, t_true, false));
  DO(test_unary(is_signed, t_false, false));
  DO(test_unary(is_signed, t_bool, false));
  DO(test_unary(is_signed, t_u_lit, true));
  DO(test_unary(is_signed, t_u8, false));
  DO(test_unary(is_signed, t_u16, false));
  DO(test_unary(is_signed, t_u32, false));
  DO(test_unary(is_signed, t_u64, false));
  DO(test_unary(is_signed, t_u128, false));
  DO(test_unary(is_signed, t_s_lit, true));
  DO(test_unary(is_signed, t_i8, true));
  DO(test_unary(is_signed, t_i16, true));
  DO(test_unary(is_signed, t_i32, true));
  DO(test_unary(is_signed, t_i64, true));
  DO(test_unary(is_signed, t_i128, true));
  //DO(test_unary(is_signed, t_f_lit, false)); TODO: failure
  //DO(test_unary(is_signed, t_f32, false));  TODO: failure
  //DO(test_unary(is_signed, t_f64, false));  TODO: failure
  DO(test_unary(is_signed, t_none, false));
  DO(test_unary(is_signed, t_string, false));
  DO(test_unary(is_signed, t_t1, false));
  DO(test_unary(is_signed, t_foo, false));
  DO(test_unary(is_signed, t_structural, false));
}


TEST_F(SubTypeTest, IsSingleType)
{
  // TODO: check definition of "singletype"
  // TODO: why are U8 etc data when there are distinct instances?
  DO(test_unary(is_singletype, t_true, true));
  DO(test_unary(is_singletype, t_false, true));
  DO(test_unary(is_singletype, t_bool, false));
  DO(test_unary(is_singletype, t_u_lit, true));
  DO(test_unary(is_singletype, t_u8, true));
  DO(test_unary(is_singletype, t_u16, true));
  DO(test_unary(is_singletype, t_u32, true));
  DO(test_unary(is_singletype, t_u64, true));
  DO(test_unary(is_singletype, t_u128, true));
  DO(test_unary(is_singletype, t_s_lit, true));
  DO(test_unary(is_singletype, t_i8, true));
  DO(test_unary(is_singletype, t_i16, true));
  DO(test_unary(is_singletype, t_i32, true));
  DO(test_unary(is_singletype, t_i64, true));
  DO(test_unary(is_singletype, t_i128, true));
  DO(test_unary(is_singletype, t_f_lit, true));
  DO(test_unary(is_singletype, t_f32, true));
  DO(test_unary(is_singletype, t_f64, true));
  DO(test_unary(is_singletype, t_none, true));
  DO(test_unary(is_singletype, t_string, true));  // TODO: Really?

  //DO(test_unary(is_singletype, t_t1, false)); TODO: failure
  //DO(test_unary(is_singletype, t_foo, false));
  DO(test_unary(is_singletype, t_structural, false));
}


// TODO: is_concrete is probably going to disappear soon, so I'm not bothered
// about making this complete
TEST_F(SubTypeTest, IsConcrete)
{
  DO(test_unary(is_concrete, t_true, true));
  DO(test_unary(is_concrete, t_false, true));
  //DO(test_unary(is_concrete, t_bool, false)); TODO: failure
  DO(test_unary(is_concrete, t_u_lit, true));
  DO(test_unary(is_concrete, t_u8, true));
  DO(test_unary(is_concrete, t_u16, true));
  DO(test_unary(is_concrete, t_u32, true));
  DO(test_unary(is_concrete, t_u64, true));
  DO(test_unary(is_concrete, t_u128, true));
  DO(test_unary(is_concrete, t_s_lit, true));
  DO(test_unary(is_concrete, t_i8, true));
  DO(test_unary(is_concrete, t_i16, true));
  DO(test_unary(is_concrete, t_i32, true));
  DO(test_unary(is_concrete, t_i64, true));
  DO(test_unary(is_concrete, t_i128, true));
  DO(test_unary(is_concrete, t_f_lit, true));
  DO(test_unary(is_concrete, t_f32, true));
  DO(test_unary(is_concrete, t_f64, true));
  DO(test_unary(is_concrete, t_none, true));
  DO(test_unary(is_concrete, t_string, true));
  DO(test_unary(is_concrete, t_t1, false));
  DO(test_unary(is_concrete, t_foo, true));
  DO(test_unary(is_concrete, t_structural, false));
}


TEST_F(SubTypeTest, IsMathCompatible)
{
  DO(test_binary(is_math_compatible, t_u_lit, t_u_lit, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_s_lit, true));
  DO(test_binary(is_math_compatible, t_f_lit, t_f_lit, true));
  DO(test_binary(is_math_compatible, t_u_lit, t_s_lit, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_u_lit, true));
  DO(test_binary(is_math_compatible, t_f_lit, t_u_lit, true));
  DO(test_binary(is_math_compatible, t_f_lit, t_s_lit, true));
  DO(test_binary(is_math_compatible, t_u_lit, t_f_lit, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_f_lit, true));

  DO(test_binary(is_math_compatible, t_true, t_true, false));
  DO(test_binary(is_math_compatible, t_true, t_false, false));
  DO(test_binary(is_math_compatible, t_bool, t_bool, false));
  DO(test_binary(is_math_compatible, t_string, t_string, false));
  DO(test_binary(is_math_compatible, t_none, t_none, false));
  DO(test_binary(is_math_compatible, t_foo, t_foo, false));
  DO(test_binary(is_math_compatible, t_t1, t_t1, false));

  DO(test_binary(is_math_compatible, t_u8, t_u8, true));
  DO(test_binary(is_math_compatible, t_u16, t_u16, true));
  DO(test_binary(is_math_compatible, t_u32, t_u32, true));
  DO(test_binary(is_math_compatible, t_u64, t_u64, true));
  DO(test_binary(is_math_compatible, t_u128, t_u128, true));
  DO(test_binary(is_math_compatible, t_i8, t_i8, true));
  DO(test_binary(is_math_compatible, t_i16, t_i16, true));
  DO(test_binary(is_math_compatible, t_i32, t_i32, true));
  DO(test_binary(is_math_compatible, t_i64, t_i64, true));
  DO(test_binary(is_math_compatible, t_i128, t_i128, true));
  DO(test_binary(is_math_compatible, t_f32, t_f32, true));
  DO(test_binary(is_math_compatible, t_f64, t_f64, true));

  DO(test_binary(is_math_compatible, t_u_lit, t_u8, true));
  DO(test_binary(is_math_compatible, t_u_lit, t_i16, true));
  DO(test_binary(is_math_compatible, t_u_lit, t_f32, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_u8, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_i16, true));
  DO(test_binary(is_math_compatible, t_s_lit, t_f32, true));
  DO(test_binary(is_math_compatible, t_f_lit, t_u8, false));
  DO(test_binary(is_math_compatible, t_f_lit, t_i16, false));
  DO(test_binary(is_math_compatible, t_f_lit, t_f32, true));

  DO(test_binary(is_math_compatible, t_u8, t_u16, false));
  DO(test_binary(is_math_compatible, t_i16, t_i32, false));
  DO(test_binary(is_math_compatible, t_f32, t_f64, false));
  DO(test_binary(is_math_compatible, t_u8, t_i8, false));
  DO(test_binary(is_math_compatible, t_i32, t_f32, false));

  DO(test_binary(is_math_compatible, t_u8, t_bool, false));
  DO(test_binary(is_math_compatible, t_i32, t_none, false));
  DO(test_binary(is_math_compatible, t_f32, t_string, false));
  DO(test_binary(is_math_compatible, t_t1, t_u32, false));
  DO(test_binary(is_math_compatible, t_foo, t_i32, false));
  DO(test_binary(is_math_compatible, t_structural, t_u32, false));
}
