#include <gtest/gtest.h>
#include <platform.h>
#include <type/matchtype.h>
#include <ast/astbuild.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

class MatchTypeTest : public PassTest
{};


TEST_F(MatchTypeTest, SimpleTypes)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "interface I1\n"
    "  fun f()\n"

    "interface I2\n"
    "  fun g()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, i1: I1, i2: I2, t1: T1, t2: T2)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("c1"), &opt));
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("c2"), &opt));
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c2"), type_of("c1"), &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("t1"), &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("c1"), &opt));

  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("t2"), &opt));
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("t2"), type_of("c1"), &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("i1"), &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("i1"), type_of("c1"), &opt));

  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("i2"), &opt));
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("i2"), type_of("c1"), &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("t2"), &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("t2"), type_of("t1"), &opt));

  pass_opt_done(&opt);
}


TEST_F(MatchTypeTest, CompoundOperand)
{
  const char* src =
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

    "class C3 is (T1 & T2)\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, c3: C3, i1: I1, i2: I2, t1: T1, t2: T2, t3: T3,\n"
    "    c1or2: (C1 | C2), c1ort2: (C1 | T2), t1and2: (T1 & T2))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // (C1 | C2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1or2"), type_of("c1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1or2"), type_of("c2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1or2"), type_of("t1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1or2"), type_of("t2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1or2"), type_of("i1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1or2"), type_of("i2"), &opt));

  // (C1 | T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1ort2"), type_of("c1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1ort2"), type_of("c2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1ort2"), type_of("t1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1ort2"), type_of("t2"), &opt));

  // (T1 & T2)
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("t1and2"), type_of("c1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("t1and2"), type_of("c2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("c3"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("t1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("t2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("t3"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("i1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("i2"), &opt));

  pass_opt_done(&opt);
}


TEST_F(MatchTypeTest, CompoundPattern)
{
  const char* src =
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

    "class C3 is (T1 & T2)\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, c3: C3, i1: I1, i2: I2, t1: T1, t2: T2, t3: T3,\n"
    "    c1or2: (C1 | C2), c1ort2: (C1 | T2), t1and2: (T1 & T2))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // (C1 | C2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("c1or2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c2"), type_of("c1or2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("c1or2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("t2"), type_of("c1or2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("i1"), type_of("c1or2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("i2"), type_of("c1or2"), &opt));

  // (C1 | T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("c1ort2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c2"), type_of("c1ort2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("c1ort2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t2"), type_of("c1ort2"), &opt));

  // (T1 & T2)
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("t1and2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c2"), type_of("t1and2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c3"), type_of("t1and2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("t1and2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t2"), type_of("t1and2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t3"), type_of("t1and2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("i1"), type_of("t1and2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("i2"), type_of("t1and2"), &opt));

  pass_opt_done(&opt);
}


TEST_F(MatchTypeTest, BothCompound)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "interface I1\n"
    "  fun f()\n"

    "interface I2\n"
    "  fun g()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2\n"

    "class C3 is (T1 & T2)\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, c3: C3, i1: I1, i2: I2, t1: T1, t2: T2,\n"
    "    c1orc2: (C1 | C2), c1orc3: (C1 | C3), c3ort2: (C3 | T2),\n"
    "    t1ort2: (T1 | T2), t1andt2: (T1 & T2), i1andi2: (I1 & I2))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // (C1 | C2) vs (T1 | T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1orc2"), type_of("t1ort2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1ort2"), type_of("c1orc2"), &opt));

  // (C1 | C2) vs (C3 | T2)
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1orc2"), type_of("c3ort2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c3ort2"), type_of("c1orc2"), &opt));

  // (C1 | C2) vs (T1 & T2)
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1orc2"), type_of("t1andt2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("t1andt2"), type_of("c1orc2"), &opt));

  // (C1 | C3) vs (T1 & T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1orc3"), type_of("t1andt2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1andt2"), type_of("c1orc3"), &opt));

  // (T1 & T2) vs (T1 | T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1andt2"), type_of("t1ort2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1ort2"), type_of("t1andt2"), &opt));

  // (T1 & T2) vs (I1 & I2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT,
    is_matchtype(type_of("t1andt2"), type_of("i1andi2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT,
    is_matchtype(type_of("i1andi2"), type_of("t1andt2"), &opt));

  pass_opt_done(&opt);
}


TEST_F(MatchTypeTest, Tuples)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2\n"

    "class C3 is (T1 & T2)\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, c3: C3, t1: T1, t2: T2,\n"
    "    c1c1: (C1, C1), c1c2: (C1, C2), c1c3: (C1, C3),\n"
    "    t1t2: (T1, T2))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("c1c1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1c1"), type_of("c1"), &opt));

  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1c2"), type_of("t1t2"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1c3"), type_of("t1t2"), &opt));

  // We can't make types with don't cares in as the type of a parameter. Modify
  // t1t2 instead.
  ast_t* t1t2 = type_of("t1t2");
  AST_GET_CHILDREN(t1t2, elem1, elem2);
  assert(ast_id(elem1) == TK_NOMINAL);
  assert(ast_id(elem2) == TK_NOMINAL);

  REPLACE(&elem2, NODE(TK_DONTCARETYPE));

  // (T1, _)
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), t1t2, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1c2"), t1t2, &opt));

  REPLACE(&elem1, NODE(TK_DONTCARETYPE));

  // (_, _)
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), t1t2, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1c2"), t1t2, &opt));

  pass_opt_done(&opt);
}


TEST_F(MatchTypeTest, Capabilities)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, t1: T1, t2: T2,\n"
    "    c1ref: C1 ref, c1val: C1 val, c1box: C1 box,\n"
    "    c2ref: C2 ref,\n"
    "    c1refc2ref: (C1 ref, C2 ref),\n"
    "    c1refc2val: (C1 ref, C2 val),\n"
    "    c1valc2ref: (C1 val, C2 ref),\n"
    "    c1reforc2ref: (C1 ref | C2 ref),\n"
    "    c1reforc2val: (C1 ref | C2 val),\n"
    "    c1valorc2ref: (C1 val | C2 ref),\n"
    "    t1refort2ref: (T1 ref | T2 ref),\n"
    "    t1refort2val: (T1 ref | T2 val),\n"
    "    t1valort2ref: (T1 val | T2 ref),\n"
    "    t1refandt2ref: (T1 ref & T2 ref),\n"
    "    t1refandt2box: (T1 ref & T2 box),\n"
    "    t1valandt2box: (T1 val & T2 box))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // Classes
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1ref"), type_of("c1ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("c1ref"), type_of("c1val"), &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1ref"), type_of("c1box"), &opt));

  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("c1val"), type_of("c1ref"), &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1val"), type_of("c1val"), &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1val"), type_of("c1box"), &opt));

  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("c1box"), type_of("c1ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("c1box"), type_of("c1val"), &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1box"), type_of("c1box"), &opt));

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1box"), type_of("c2ref"), &opt));

  // Tuples
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1refc2ref"), type_of("c1refc2ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("c1refc2ref"), type_of("c1valc2ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("c1refc2ref"), type_of("c1refc2val"), &opt));

  // Unions
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1reforc2ref"), type_of("c1reforc2ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("c1reforc2ref"), type_of("c1valorc2ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("c1reforc2ref"), type_of("c1reforc2val"), &opt));

  // Intersect vs union
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1refort2ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1valort2ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1refort2val"), &opt));

  // Intersects
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1refandt2ref"), &opt));
  ASSERT_EQ(MATCHTYPE_DENY,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1valandt2box"), &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1refandt2box"), &opt));

  pass_opt_done(&opt);
}


TEST_F(MatchTypeTest, TypeParams)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2\n"

    "interface Test\n"
    "  fun z[A1: C2 ref, A2: T1 ref, A3: T2 ref, A4: T2 box,\n"
    "    A5: (T1 ref | T2 ref)]\n"
    "    (c1: C1, c2: C2, t1: T1, t2: T2,\n"
    "    ac2: A1, at1: A2, at2: A3, at2box: A4, aunion: A5)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // Ref constraints
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("ac2"), type_of("c1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("at2"), type_of("c1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("at1"), type_of("c1"), &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("at2"), type_of("t1"), &opt));

  // Box constraint
  ASSERT_EQ(
    MATCHTYPE_DENY, is_matchtype(type_of("at2box"), type_of("t1"), &opt));

  // Union constraint
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("aunion"), type_of("c1"), &opt));

  pass_opt_done(&opt);
}
