#include <gtest/gtest.h>
#include <platform.h>

#include <type/subtype.h>

#include "util.h"
#include <stdio.h>


static const char* src =
  "trait T1\n"
  "  fun f()\n"

  "trait T1b\n"
  "  fun f()\n"

  "trait T2\n"
  "  fun g()\n"

  "trait T3 is (T1 & T2)\n"

  "interface I1\n"
  "  fun f()\n"

  "interface I1b\n"
  "  fun f()\n"

  "interface I2\n"
  "  fun g()\n"

  "interface I3\n"
  "  fun f()\n"
  "  fun g()\n"

  "class C1 is T1\n"
  "  fun f() => None\n"

  "class C2 is T2\n"
  "  fun g() => None\n"

  "class C3 is T3\n"
  "  fun f() => None\n"
  "  fun g() => None\n"

  "primitive P1\n"

  "primitive P2 is T2\n"
  "  fun g() => None";


class SubTypeTest: public PassTest
{
protected:
  void test_unary(bool (*fn)(ast_t*), const char* type, bool expect)
  {
    ASSERT_NE((void*)NULL, fn);
    ASSERT_NE((void*)NULL, type);

    DO(generate_types(src, type));
    ASSERT_EQ(expect, fn(prog_type_1));
    TearDown();
  }

  void test_binary(bool(*fn)(ast_t*, ast_t*), const char* type_a,
    const char* type_b, bool expect)
  {
    ASSERT_NE((void*)NULL, fn);
    ASSERT_NE((void*)NULL, type_a);
    ASSERT_NE((void*)NULL, type_b);

    DO(generate_types(src, type_a, type_b));
    ASSERT_EQ(expect, fn(prog_type_1, prog_type_2));
    TearDown();
  }
};


// TODO: is_literal, is_constructable, is_known

TEST_F(SubTypeTest, IsSubType)
{
  // class, trait
  DO(test_binary(is_subtype, "C1", "T1", true));
  DO(test_binary(is_subtype, "C1", "T2", false));
  DO(test_binary(is_subtype, "C1", "T3", false));
  DO(test_binary(is_subtype, "C3", "T1", true));
  DO(test_binary(is_subtype, "C3", "T2", true));
  DO(test_binary(is_subtype, "C3", "T3", true));

  // class, interface
  DO(test_binary(is_subtype, "C1", "I1", true));
  DO(test_binary(is_subtype, "C1", "I2", false));
  DO(test_binary(is_subtype, "C1", "I3", false));
  DO(test_binary(is_subtype, "C3", "I1", true));
  DO(test_binary(is_subtype, "C3", "I2", true));
  DO(test_binary(is_subtype, "C3", "I3", true));

  // class, class
  DO(test_binary(is_subtype, "C1", "C1", true));
  DO(test_binary(is_subtype, "C1", "C3", false));
  DO(test_binary(is_subtype, "C3", "C1", false));

  // trait, trait
  DO(test_binary(is_subtype, "T1", "T1", true));
  DO(test_binary(is_subtype, "T1", "T3", false));
  DO(test_binary(is_subtype, "T3", "T1", true));
  DO(test_binary(is_subtype, "T1", "T1b", false));
  DO(test_binary(is_subtype, "T1b", "T1", false));

  // interface, interface
  DO(test_binary(is_subtype, "I1", "I1", true));
  DO(test_binary(is_subtype, "I1", "I3", false));
  DO(test_binary(is_subtype, "I3", "I1", true));
  DO(test_binary(is_subtype, "I1", "I1b", true));
  DO(test_binary(is_subtype, "I1b", "I1", true));

  // trait, interface
  DO(test_binary(is_subtype, "T1", "I1", true));
  DO(test_binary(is_subtype, "T1", "I2", false));
  DO(test_binary(is_subtype, "I1", "T1", false));

  // primitive, primitive
  DO(test_binary(is_subtype, "P1", "P1", true));
  DO(test_binary(is_subtype, "P1", "P2", false));
  DO(test_binary(is_subtype, "P2", "P1", false));

  // primitive, trait
  DO(test_binary(is_subtype, "P1", "T1 box", false));
  DO(test_binary(is_subtype, "P1", "T2 box", false));
  DO(test_binary(is_subtype, "P2", "T1 box", false));
  DO(test_binary(is_subtype, "P2", "T2 box", true));

  // union
  DO(test_binary(is_subtype, "C1", "(C1 | C2)", true));
  DO(test_binary(is_subtype, "C2", "(C1 | C2)", true));
  DO(test_binary(is_subtype, "C3", "(C1 | C2)", false));
  DO(test_binary(is_subtype, "(C1 | C2)", "C1", false));
  DO(test_binary(is_subtype, "(C1 | C2)", "(C1 | C2)", true));
  DO(test_binary(is_subtype, "C1", "(T1 | T2)", true));
  DO(test_binary(is_subtype, "T1", "(C1 | C2)", false));

  // intersect
  DO(test_binary(is_subtype, "T1", "(T1 & T2)", false));
  DO(test_binary(is_subtype, "C1", "(T1 & T2)", false));
  DO(test_binary(is_subtype, "T3", "(T1 & T2)", true));
  DO(test_binary(is_subtype, "C3", "(T1 & T2)", true));
  DO(test_binary(is_subtype, "(T1 & T2)", "(T1 & T2)", true));
  DO(test_binary(is_subtype, "(T1 & T2)", "C3", false));

  // subtype is intersect of non-independent and combinable types
  // TODO: fix these
  //DO(test_binary(is_subtype, "(I1 & I2)", "I3", true));
  //DO(test_binary(is_subtype, "((C1 | C2) & (C1 | C3))", "C1", true));

  // tuple
  DO(test_binary(is_subtype, "T1", "(T1, T1)", false));
  DO(test_binary(is_subtype, "(T1, T1)", "T1", false));
  DO(test_binary(is_subtype, "(C1, C2)", "(T1, T2)", true));
  DO(test_binary(is_subtype, "(C1, C2)", "(T2, T1)", false));
  DO(test_binary(is_subtype, "(T1, T2)", "(T1, T2)", true));
  DO(test_binary(is_subtype, "(T1, T2)", "(C1, C2)", false));

  // capabilitites
  DO(test_binary(is_subtype, "C1 ref", "T1 ref", true));
  DO(test_binary(is_subtype, "C1 val", "T1 val", true));
  DO(test_binary(is_subtype, "C1 box", "T1 box", true));
  DO(test_binary(is_subtype, "C1 ref", "T1 box", true));
  DO(test_binary(is_subtype, "C1 val", "T1 box", true));
  DO(test_binary(is_subtype, "C1 ref", "T1 tag", true));
  DO(test_binary(is_subtype, "C1 iso", "T1 ref", true));
  DO(test_binary(is_subtype, "C1 iso", "T1 val", true));
  DO(test_binary(is_subtype, "C1 iso", "T1 box", true));
  DO(test_binary(is_subtype, "C1 iso", "T1 tag", true));
  DO(test_binary(is_subtype, "C1 ref", "T1 val", false));
  DO(test_binary(is_subtype, "C1 val", "T1 ref", false));
  DO(test_binary(is_subtype, "C1 box", "T1 ref", false));
  DO(test_binary(is_subtype, "C1 box", "T1 val", false));
  DO(test_binary(is_subtype, "C1 tag", "T1 box", false));
  DO(test_binary(is_subtype, "C1 tag", "T1 iso", false));
}


TEST_F(SubTypeTest, IsEqType)
{
  DO(test_binary(is_eqtype, "C1", "C1", true));
  DO(test_binary(is_eqtype, "C2", "C2", true));
  DO(test_binary(is_eqtype, "T1", "T1", true));
  DO(test_binary(is_eqtype, "T2", "T2", true));
  DO(test_binary(is_eqtype, "I1", "I1", true));
  DO(test_binary(is_eqtype, "I2", "I2", true));

  DO(test_binary(is_eqtype, "C1", "C2", false));
  DO(test_binary(is_eqtype, "C2", "C1", false));
  DO(test_binary(is_eqtype, "T1", "T2", false));
  DO(test_binary(is_eqtype, "T2", "T1", false));
  DO(test_binary(is_eqtype, "I1", "I2", false));
  DO(test_binary(is_eqtype, "I2", "I1", false));

  // TODO: fix this
  //DO(test_binary(is_eqtype, "I1", "I1b", true));
  DO(test_binary(is_eqtype, "T1", "T1b", false));

  DO(test_binary(is_eqtype, "C1", "T1", false));
  DO(test_binary(is_eqtype, "T1", "C1", false));

  DO(test_binary(is_eqtype, "C1 iso", "C1 iso", true));
  DO(test_binary(is_eqtype, "C1 ref", "C1 ref", true));
  DO(test_binary(is_eqtype, "C1 val", "C1 val", true));
  DO(test_binary(is_eqtype, "C1 box", "C1 box", true));
  DO(test_binary(is_eqtype, "C1 tag", "C1 tag", true));
  DO(test_binary(is_eqtype, "C1 iso", "C1 ref", false));
  DO(test_binary(is_eqtype, "C1 ref", "C1 val", false));
  DO(test_binary(is_eqtype, "C1 val", "C1 ref", false));
  DO(test_binary(is_eqtype, "C1 box", "C1 val", false));
  DO(test_binary(is_eqtype, "C1 box", "C1 ref", false));
}


TEST_F(SubTypeTest, IsNone)
{
  DO(test_unary(is_none, "Bool", false));
  DO(test_unary(is_none, "U8", false));
  DO(test_unary(is_none, "U16", false));
  DO(test_unary(is_none, "U32", false));
  DO(test_unary(is_none, "U64", false));
  DO(test_unary(is_none, "U128", false));
  DO(test_unary(is_none, "I8", false));
  DO(test_unary(is_none, "I16", false));
  DO(test_unary(is_none, "I32", false));
  DO(test_unary(is_none, "I64", false));
  DO(test_unary(is_none, "I128", false));
  DO(test_unary(is_none, "F32", false));
  DO(test_unary(is_none, "F64", false));
  DO(test_unary(is_none, "C1", false));
  DO(test_unary(is_none, "T1", false));
  DO(test_unary(is_none, "None", true));
  DO(test_unary(is_none, "(Bool | None)", false));
  DO(test_unary(is_none, "(Bool & None)", false));
}


TEST_F(SubTypeTest, IsBool)
{
  DO(test_unary(is_bool, "Bool", true));
  DO(test_unary(is_bool, "U8", false));
  DO(test_unary(is_bool, "U16", false));
  DO(test_unary(is_bool, "U32", false));
  DO(test_unary(is_bool, "U64", false));
  DO(test_unary(is_bool, "U128", false));
  DO(test_unary(is_bool, "I8", false));
  DO(test_unary(is_bool, "I16", false));
  DO(test_unary(is_bool, "I32", false));
  DO(test_unary(is_bool, "I64", false));
  DO(test_unary(is_bool, "I128", false));
  DO(test_unary(is_bool, "F32", false));
  DO(test_unary(is_bool, "F64", false));
  DO(test_unary(is_bool, "C1", false));
  DO(test_unary(is_bool, "T1", false));
  DO(test_unary(is_bool, "None", false));
  DO(test_unary(is_bool, "(Bool | None)", false));
  DO(test_unary(is_bool, "(Bool & None)", false));
}


TEST_F(SubTypeTest, IsFloat)
{
  DO(test_unary(is_float, "Bool", false));
  DO(test_unary(is_float, "U8", false));
  DO(test_unary(is_float, "U16", false));
  DO(test_unary(is_float, "U32", false));
  DO(test_unary(is_float, "U64", false));
  DO(test_unary(is_float, "U128", false));
  DO(test_unary(is_float, "I8", false));
  DO(test_unary(is_float, "I16", false));
  DO(test_unary(is_float, "I32", false));
  DO(test_unary(is_float, "I64", false));
  DO(test_unary(is_float, "I128", false));
  DO(test_unary(is_float, "F32", true));
  DO(test_unary(is_float, "F64", true));
  DO(test_unary(is_float, "C1", false));
  DO(test_unary(is_float, "T1", false));
  DO(test_unary(is_float, "None", false));
  DO(test_unary(is_float, "(F32 | None)", false));
  DO(test_unary(is_float, "(F32 & None)", false));
  DO(test_unary(is_float, "(F32 | F64)", false));
  DO(test_unary(is_float, "(F32 & F64)", false));
}


TEST_F(SubTypeTest, IsInteger)
{
  DO(test_unary(is_integer, "Bool", false));
  DO(test_unary(is_integer, "U8", true));
  DO(test_unary(is_integer, "U16", true));
  DO(test_unary(is_integer, "U32", true));
  DO(test_unary(is_integer, "U64", true));
  DO(test_unary(is_integer, "U128", true));
  DO(test_unary(is_integer, "I8", true));
  DO(test_unary(is_integer, "I16", true));
  DO(test_unary(is_integer, "I32", true));
  DO(test_unary(is_integer, "I64", true));
  DO(test_unary(is_integer, "I128", true));
  DO(test_unary(is_integer, "F32", false));
  DO(test_unary(is_integer, "F64", false));
  DO(test_unary(is_integer, "C1", false));
  DO(test_unary(is_integer, "T1", false));
  DO(test_unary(is_integer, "None", false));
  DO(test_unary(is_integer, "(U8 | None)", false));
  DO(test_unary(is_integer, "(U8 & None)", false));
  DO(test_unary(is_integer, "(U8 | I32)", false));
  DO(test_unary(is_integer, "(U8 & I32)", false));
}


TEST_F(SubTypeTest, IsMachineWord)
{
  DO(test_unary(is_machine_word, "Bool", true));
  DO(test_unary(is_machine_word, "U8", true));
  DO(test_unary(is_machine_word, "U16", true));
  DO(test_unary(is_machine_word, "U32", true));
  DO(test_unary(is_machine_word, "U64", true));
  DO(test_unary(is_machine_word, "U128", true));
  DO(test_unary(is_machine_word, "I8", true));
  DO(test_unary(is_machine_word, "I16", true));
  DO(test_unary(is_machine_word, "I32", true));
  DO(test_unary(is_machine_word, "I64", true));
  DO(test_unary(is_machine_word, "I128", true));
  DO(test_unary(is_machine_word, "F32", true));
  DO(test_unary(is_machine_word, "F64", true));
  DO(test_unary(is_machine_word, "C1", false));
  DO(test_unary(is_machine_word, "T1", false));
  DO(test_unary(is_machine_word, "None", false));
  DO(test_unary(is_machine_word, "(U8 | None)", false));
  DO(test_unary(is_machine_word, "(U8 & None)", false));
  DO(test_unary(is_machine_word, "(U8 | I32)", false));
  DO(test_unary(is_machine_word, "(U8 & I32)", false));
}


TEST_F(SubTypeTest, IsConcrete)
{
  DO(test_unary(is_concrete, "Bool", true));
  DO(test_unary(is_concrete, "U8", true));
  DO(test_unary(is_concrete, "U16", true));
  DO(test_unary(is_concrete, "U32", true));
  DO(test_unary(is_concrete, "U64", true));
  DO(test_unary(is_concrete, "U128", true));
  DO(test_unary(is_concrete, "I8", true));
  DO(test_unary(is_concrete, "I16", true));
  DO(test_unary(is_concrete, "I32", true));
  DO(test_unary(is_concrete, "I64", true));
  DO(test_unary(is_concrete, "I128", true));
  DO(test_unary(is_concrete, "F32", true));
  DO(test_unary(is_concrete, "F64", true));
  DO(test_unary(is_concrete, "C1", true));
  DO(test_unary(is_concrete, "T1", false));
  DO(test_unary(is_concrete, "None", true));
  DO(test_unary(is_concrete, "(T1 | None)", false));
  DO(test_unary(is_concrete, "(T1 & None)", true));
  DO(test_unary(is_concrete, "(C1 | T1)", false));
  DO(test_unary(is_concrete, "(C1 & T1)", true));
}
