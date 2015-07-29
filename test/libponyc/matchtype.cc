#include <gtest/gtest.h>
#include <platform.h>

#include <type/matchtype.h>

#include "util.h"


static const char* src =
  "trait T1\n"
  "  fun f()\n"

  "trait T2\n"
  "  fun g()\n"

  "trait T3\n"
  "  fun h()\n"

  "interface I1\n"
  "  fun f()\n"

  "interface I2\n"
  "  fun g()\n"

  "class C1 is T1\n"
  "  fun f() => None\n"

  "class C2\n"

  "class C3 is T1, T2\n"
  "  fun f() => None\n"
  "  fun g() => None";


class MatchTypeTest : public PassTest
{
protected:
  void test(const char* operand, const char* pattern, matchtype_t expect,
    const char* type_param_name = NULL,
    const char* type_param_constraint = NULL)
  {
    ASSERT_NE((void*)NULL, operand);
    ASSERT_NE((void*)NULL, pattern);

    DO(generate_types(src, operand, pattern, NULL, type_param_name,
      type_param_constraint));

    ASSERT_EQ(expect, is_matchtype(prog_type_1, prog_type_2));
    TearDown();
  }
};


TEST_F(MatchTypeTest, SimpleTypes)
{
  DO(test("C1", "C1", MATCHTYPE_ACCEPT));
  DO(test("C1", "C2", MATCHTYPE_REJECT));
  DO(test("C2", "C1", MATCHTYPE_REJECT));

  DO(test("C1", "T1", MATCHTYPE_ACCEPT));
  DO(test("T1", "C1", MATCHTYPE_ACCEPT));

  DO(test("C1", "T2", MATCHTYPE_REJECT));
  DO(test("T2", "C1", MATCHTYPE_REJECT));

  DO(test("C1", "I1", MATCHTYPE_ACCEPT));
  DO(test("I1", "C1", MATCHTYPE_ACCEPT));

  DO(test("C1", "I2", MATCHTYPE_REJECT));
  DO(test("I2", "C1", MATCHTYPE_REJECT));

  DO(test("T1", "T2", MATCHTYPE_ACCEPT));
  DO(test("T2", "T1", MATCHTYPE_ACCEPT));
}


TEST_F(MatchTypeTest, CompoundOperand)
{
  DO(test("(C1 | C2)", "C1", MATCHTYPE_ACCEPT));
  DO(test("(C1 | C2)", "C2", MATCHTYPE_ACCEPT));

  DO(test("(C1 | C2)", "T1", MATCHTYPE_ACCEPT));
  DO(test("(C1 | C2)", "T2", MATCHTYPE_REJECT));

  DO(test("(C1 | C2)", "I1", MATCHTYPE_ACCEPT));
  DO(test("(C1 | C2)", "I2", MATCHTYPE_REJECT));

  DO(test("(C1 | T2)", "C1", MATCHTYPE_ACCEPT));
  DO(test("(C1 | T2)", "C2", MATCHTYPE_REJECT));
  DO(test("(C1 | T2)", "T1", MATCHTYPE_ACCEPT));
  DO(test("(C1 | T2)", "T2", MATCHTYPE_ACCEPT));

  DO(test("(T1 & T2)", "T1", MATCHTYPE_ACCEPT));
  DO(test("(T1 & T2)", "T2", MATCHTYPE_ACCEPT));
  DO(test("(T1 & T2)", "T3", MATCHTYPE_ACCEPT));

  DO(test("(T1 & T2)", "C1", MATCHTYPE_REJECT));
  DO(test("(T1 & T2)", "C2", MATCHTYPE_REJECT));
  DO(test("(T1 & T2)", "C3", MATCHTYPE_ACCEPT));

  DO(test("(T1 & T2)", "I1", MATCHTYPE_ACCEPT));
  DO(test("(T1 & T2)", "I2", MATCHTYPE_ACCEPT));
}


TEST_F(MatchTypeTest, CompoundPattern)
{
  DO(test("C1", "(C1 | C2)", MATCHTYPE_ACCEPT));
  DO(test("C2", "(C1 | C2)", MATCHTYPE_ACCEPT));

  DO(test("T1", "(C1 | C2)", MATCHTYPE_ACCEPT));
  DO(test("T2", "(C1 | C2)", MATCHTYPE_REJECT));

  DO(test("I1", "(C1 | C2)", MATCHTYPE_ACCEPT));
  DO(test("I2", "(C1 | C2)", MATCHTYPE_REJECT));

  DO(test("C1", "(C1 | T2)", MATCHTYPE_ACCEPT));
  DO(test("C2", "(C1 | T2)", MATCHTYPE_REJECT));
  DO(test("T1", "(C1 | T2)", MATCHTYPE_ACCEPT));
  DO(test("T2", "(C1 | T2)", MATCHTYPE_ACCEPT));

  DO(test("T1", "(T1 & T2)", MATCHTYPE_ACCEPT));
  DO(test("T2", "(T1 & T2)", MATCHTYPE_ACCEPT));
  DO(test("T3", "(T1 & T2)", MATCHTYPE_ACCEPT));

  DO(test("C1", "(T1 & T2)", MATCHTYPE_REJECT));
  DO(test("C2", "(T1 & T2)", MATCHTYPE_REJECT));
  DO(test("C3", "(T1 & T2)", MATCHTYPE_ACCEPT));

  DO(test("I1", "(T1 & T2)", MATCHTYPE_ACCEPT));
  DO(test("I2", "(T1 & T2)", MATCHTYPE_ACCEPT));
}


TEST_F(MatchTypeTest, BothCompound)
{
  DO(test("(C1 | C2)", "(T1 | T2)", MATCHTYPE_ACCEPT));
  DO(test("(T1 | T2)", "(C1 | C2)", MATCHTYPE_ACCEPT));

  DO(test("(C1 | C2)", "(C3 | T2)", MATCHTYPE_REJECT));
  DO(test("(C3 | T2)", "(C1 | C2)", MATCHTYPE_REJECT));

  DO(test("(C1 | C2)", "(T1 & T2)", MATCHTYPE_REJECT));
  DO(test("(T1 & T2)", "(C1 | C2)", MATCHTYPE_REJECT));

  DO(test("(C1 | C3)", "(T1 & T2)", MATCHTYPE_ACCEPT));
  DO(test("(T1 & T2)", "(C1 | C3)", MATCHTYPE_ACCEPT));

  DO(test("(T1 & T2)", "(T1 | T2)", MATCHTYPE_ACCEPT));
  DO(test("(T1 | T2)", "(T1 & T2)", MATCHTYPE_ACCEPT));

  DO(test("(T1 & T2)", "(I1 & I2)", MATCHTYPE_ACCEPT));
  DO(test("(I1 & I2)", "(T1 & T2)", MATCHTYPE_ACCEPT));
}


TEST_F(MatchTypeTest, Tuples)
{
  DO(test("C1", "(C1, C1)", MATCHTYPE_REJECT));
  DO(test("(C1, C1)", "C1", MATCHTYPE_REJECT));

  DO(test("(C1, C3)", "(T1, T2)", MATCHTYPE_ACCEPT));
  DO(test("(C1, C2)", "(T1, T2)", MATCHTYPE_REJECT));

  DO(test("(C1, C2)", "(T1, _)", MATCHTYPE_ACCEPT));
  DO(test("(C1, C2)", "(_, _)", MATCHTYPE_ACCEPT));
}


TEST_F(MatchTypeTest, Capabilities)
{
  DO(test("C1 ref", "C1 ref", MATCHTYPE_ACCEPT));
  DO(test("C1 val", "C1 val", MATCHTYPE_ACCEPT));
  DO(test("C1 box", "C1 box", MATCHTYPE_ACCEPT));

  DO(test("C1 ref", "C1 box", MATCHTYPE_ACCEPT));
  DO(test("C1 ref", "C1 val", MATCHTYPE_DENY));
  DO(test("C1 box", "C1 ref", MATCHTYPE_DENY));
  DO(test("C1 box", "C2 ref", MATCHTYPE_REJECT));

  DO(test("(C1 ref, C2 ref)", "(C1 ref, C2 ref)", MATCHTYPE_ACCEPT));
  DO(test("(C1 ref, C2 ref)", "(C1 val, C2 ref)", MATCHTYPE_DENY));
  DO(test("(C1 ref, C2 ref)", "(C1 ref, C2 val)", MATCHTYPE_DENY));

  DO(test("(C1 ref | C2 ref)", "(C1 ref | C2 ref)", MATCHTYPE_ACCEPT));
  DO(test("(C1 ref | C2 ref)", "(C1 val | C2 ref)", MATCHTYPE_DENY));
  DO(test("(C1 ref | C2 ref)", "(C1 ref | C2 val)", MATCHTYPE_DENY));

  DO(test("(T1 ref & T2 ref)", "(T1 ref | T2 ref)", MATCHTYPE_ACCEPT));
  DO(test("(T1 ref & T2 ref)", "(T1 val | T2 ref)", MATCHTYPE_DENY));
  DO(test("(T1 ref & T2 ref)", "(T1 ref | T2 val)", MATCHTYPE_DENY));

  DO(test("(T1 ref & T2 ref)", "(T1 ref & T2 ref)", MATCHTYPE_ACCEPT));
  DO(test("(T1 ref & T2 ref)", "(T1 val & T2 ref)", MATCHTYPE_DENY));
  DO(test("(T1 ref & T2 ref)", "(T1 ref & T2 val)", MATCHTYPE_DENY));
}


TEST_F(MatchTypeTest, TypeParams)
{
  // TODO: allow any tag pattern for no constraint?
  //DO(test("A", "C1 tag", MATCHTYPE_ACCEPT, "A", NULL));

  DO(test("A", "C1", MATCHTYPE_REJECT, "A", "C2 ref"));
  DO(test("A", "C1", MATCHTYPE_REJECT, "A", "T2 ref"));
  DO(test("A", "C1", MATCHTYPE_ACCEPT, "A", "T1 ref"));
  DO(test("A", "T1", MATCHTYPE_ACCEPT, "A", "T2 ref"));

  DO(test("A", "T1", MATCHTYPE_DENY, "A", "T2 box"));

  DO(test("A", "C1", MATCHTYPE_ACCEPT, "A", "(T1 ref | T2 ref)"));
}
