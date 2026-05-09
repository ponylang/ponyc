#include <gtest/gtest.h>
#include <platform.h>
#include <type/subtype.h>
#include <type/alias.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

class SubTypeTest: public PassTest
{};


// TODO: is_literal, is_constructable, is_known


TEST_F(SubTypeTest, IsSubTypeClassTrait)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "trait T3 is (T1 & T2)\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C3 is T3\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c3: C3, t1: T1, t2: T2, t3: T3)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("t1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("t2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("t3"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("t1"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("t2"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("t3"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeClassInterface)
{
  const char* src =
    "interface I1\n"
    "  fun f()\n"

    "interface I2\n"
    "  fun g()\n"

    "interface I3\n"
    "  fun f()\n"
    "  fun g()\n"

    "class C1\n"
    "  fun f() => None\n"

    "class C3\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Z\n"
    "  fun z(c1: C1, c3: C3, i1: I1, i2: I2, i3: I3)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("i1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("i2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("i3"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("i1"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("i2"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("i3"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeClassClass)
{
  const char* src =
    "class C1\n"
    "  fun f() => None\n"

    "class C2\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Z\n"
    "  fun z(c1: C1, c2: C2)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("c1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("c2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c2"), type_of("c1"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeTraitTrait)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T1b\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "trait T3 is (T1 & T2)\n"

    "interface Test\n"
    "  fun z(t1: T1, t1b: T1b, t3: T3)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("t1"), type_of("t1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t3"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("t3"), type_of("t1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1b"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1b"), type_of("t1"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeInterfaceInterface)
{
  const char* src =
    "interface I1\n"
    "  fun f()\n"

    "interface I1b\n"
    "  fun f()\n"

    "interface I2\n"
    "  fun g()\n"

    "interface I3 is (I1 & I2)\n"

    "interface Test\n"
    "  fun z(i1: I1, i1b: I1b, i3: I3)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("i1"), type_of("i1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("i1"), type_of("i3"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("i3"), type_of("i1"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("i1"), type_of("i1b"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("i1b"), type_of("i1"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeTraitInterface)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "interface I1\n"
    "  fun f()\n"

    "interface I2\n"
    "  fun g()\n"

    "interface Test\n"
    "  fun z(i1: I1, i2: I2, t1: T1)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("t1"), type_of("i1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("i2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("i1"), type_of("t1"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypePrimitivePrimitive)
{
  const char* src =
    "primitive P1\n"
    "primitive P2\n"

    "interface Test\n"
    "  fun z(p1: P1, p2: P2)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("p1"), type_of("p1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("p1"), type_of("p2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("p2"), type_of("p1"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypePrimitiveTrait)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "primitive P1\n"

    "primitive P2 is T2\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(p1: P1, p2: P2, t1: T1 box, t2: T2 box)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_FALSE(is_subtype(type_of("p1"), type_of("t1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("p1"), type_of("t2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("p2"), type_of("t1"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("p2"), type_of("t2"), NULL, &opt));

  pass_opt_init(&opt);
}

TEST_F(SubTypeTest, IsSubTypeUnion)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "trait T3 is (T1 & T2)\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2 is T2\n"
    "  fun g() => None\n"

    "class C3 is T3\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, c3: C3,\n"
    "    t1: T1, t2: T2,\n"
    "    c1or2: (C1 | C2), t1or2: (T1 | T2))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("c1or2"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c2"), type_of("c1or2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c3"), type_of("c1or2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1or2"), type_of("c1"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c1or2"), type_of("c1or2"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("t1or2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1or2"), type_of("c1"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeIntersect)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "trait T3 is (T1 & T2)\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2 is T2\n"
    "  fun g() => None\n"

    "class C3 is T3\n"
    "  fun f() => None\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, c3: C3,\n"
    "    t1: T1, t2: T2, t3: T3,\n"
    "    t1and2: (T1 & T2),\n"
    "    t3iso: T3 iso,\n"
    "    t3trn: T3 trn,\n"
    "    t1val: T1 val,\n"
    "    t1isoand2iso: (T1 iso & T2 iso),\n"
    "    t1trnand2trn: (T1 trn & T2 trn),\n"
    "    t1valand2box: (T1 val & T2 box),\n"
    "    t1refand2box: (T1 ref & T2 box))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);


  // intersections of caps
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1and2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("t1and2"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("t3"), type_of("t1and2"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("t1and2"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("t1and2"), type_of("t1and2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1and2"), type_of("c3"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1val"), type_of("t1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1val"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1refand2box"), NULL, &opt));

  ASSERT_FALSE(is_subtype(type_of("t3iso"), type_of("t1refand2box"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t3iso"), type_of("t1valand2box"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t3trn"), type_of("t1refand2box"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t3trn"), type_of("t1valand2box"), NULL, &opt));

  ASSERT_FALSE(is_subtype(type_of("t1val"), type_of("t1valand2box"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("t1refand2box"), type_of("t1"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("t1valand2box"), type_of("t1val"), NULL, &opt));

  // ephemerals
  ast_t* t3isohat = consume_type(type_of("t3iso"), TK_NONE, true);
  ast_t* t3trnhat = consume_type(type_of("t3trn"), TK_NONE, true);
  ASSERT_TRUE(is_subtype(t3isohat, type_of("t1refand2box"), NULL, &opt));
  ASSERT_TRUE(is_subtype(t3isohat, type_of("t1valand2box"), NULL, &opt));
  ASSERT_TRUE(is_subtype(t3trnhat, type_of("t1refand2box"), NULL, &opt));
  ASSERT_TRUE(is_subtype(t3trnhat, type_of("t1valand2box"), NULL, &opt));
  ast_free_unattached(t3isohat);
  ast_free_unattached(t3trnhat);

  ast_t* t1t2iso = consume_type(type_of("t1isoand2iso"), TK_NONE, true);
  ast_t* t1t2trn = consume_type(type_of("t1trnand2trn"), TK_NONE, true);
  ASSERT_TRUE(is_subtype(t1t2iso, type_of("t1refand2box"), NULL, &opt));
  ASSERT_TRUE(is_subtype(t1t2trn, type_of("t1refand2box"), NULL, &opt));
  ASSERT_TRUE(is_subtype(t1t2iso, type_of("t1valand2box"), NULL, &opt));
  ASSERT_TRUE(is_subtype(t1t2trn, type_of("t1valand2box"), NULL, &opt));
  ast_free_unattached(t1t2iso);
  ast_free_unattached(t1t2trn);

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeIntersectInterface)
{
  const char* src =
    "interface I1\n"
    "  fun f()\n"

    "interface I2\n"
    "  fun g()\n"

    "interface I3\n"
    "  fun f()\n"
    "  fun g()\n"

    "interface Test\n"
    "  fun z(i1: I1, i2: I2, i3: I3,\n"
    "    i1and2: (I1 & I2))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // TODO: Fix this, intersect of non-independent and combinable types
  // (I1 & I2) <: I3
  //ASSERT_TRUE(is_subtype(type_of("i1and2"), type_of("i3"), NULL, &opt));

  // I3 <: (I1 & I2)
  ASSERT_TRUE(is_subtype(type_of("i3"), type_of("i1and2"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeIntersectOfUnion)
{
  const char* src =
    "class C1\n"
    "class C2\n"
    "class C3\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, c3: C3,\n"
    "    iofu: ((C1 | C2) & (C1 | C3)))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("iofu"), NULL, &opt));

  // TODO: Fix this, intersect of non-independent and combinable types
  // ((C1 | C2) & (C1 | C3)) <: C1
  //ASSERT_TRUE(is_subtype(type_of("iofu"), type_of("c1"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeTuple)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2 is T2\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, t1: T1, t2: T2,\n"
    "    c1c2: (C1, C2), t1t1: (T1, T1), t1t2: (T1, T2), t2t1: (T2, T1))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1t1"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1t1"), type_of("t1"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("c1c2"), type_of("t1t2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1c2"), type_of("t2t1"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("t1t2"), type_of("t1t2"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t1t2"), type_of("c1c2"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeUnionOfTuple)
{
  const char* src =
    "class C1\n"
    "class C2\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2,\n"
    "    uoft3: ((C1, C1) | (C1, C2) | (C2, C1)),\n"
    "    uoft4: ((C1, C1) | (C1, C2) | (C2, C1) | (C2, C2)),\n"
    "    tofu: ((C1 | C2), (C1 | C2)))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // C1 <!: ((C1 | C2), (C1 | C2))
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("tofu"), NULL, &opt));

  // ((C1, C1) | (C1, C2) | (C2, C1) | (C2, C2)) <: ((C1 | C2), (C1 | C2))
  ASSERT_TRUE(is_subtype(type_of("uoft4"), type_of("tofu"), NULL, &opt));

  // ((C1, C1) | (C1, C2) | (C2, C1)) <: ((C1 | C2), (C1 | C2))
  ASSERT_TRUE(is_subtype(type_of("uoft3"), type_of("tofu"), NULL, &opt));

  // ((C1 | C2), (C1 | C2)) <: ((C1, C1) | (C1, C2) | (C2, C1) | (C2, C2))
  ASSERT_TRUE(is_subtype(type_of("tofu"), type_of("uoft4"), NULL, &opt));

  // ((C1 | C2), (C1 | C2)) <!: ((C1, C1) | (C1, C2) | (C2, C1))
  ASSERT_FALSE(is_subtype(type_of("tofu"), type_of("uoft3"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeCap)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "interface Test\n"
    "  fun z(c1iso: C1 iso, c1ref: C1 ref, c1val: C1 val,\n"
    "    c1box: C1 box, c1tag: C1 tag,\n"
    "    t1iso: T1 iso, t1ref: T1 ref, t1val: T1 val,\n"
    "    t1box: T1 box, t1tag: T1 tag)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE (is_subtype(type_of("c1iso"), type_of("t1iso"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1ref"), type_of("t1iso"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1val"), type_of("t1iso"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1box"), type_of("t1iso"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1tag"), type_of("t1iso"), NULL, &opt));

  ASSERT_FALSE(is_subtype(type_of("c1iso"), type_of("t1ref"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1ref"), type_of("t1ref"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1val"), type_of("t1ref"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1box"), type_of("t1ref"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1tag"), type_of("t1ref"), NULL, &opt));

  ASSERT_FALSE(is_subtype(type_of("c1iso"), type_of("t1val"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1ref"), type_of("t1val"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1val"), type_of("t1val"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1box"), type_of("t1val"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1tag"), type_of("t1val"), NULL, &opt));

  ASSERT_FALSE(is_subtype(type_of("c1iso"), type_of("t1box"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1ref"), type_of("t1box"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1val"), type_of("t1box"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1box"), type_of("t1box"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("c1tag"), type_of("t1box"), NULL, &opt));

  ASSERT_TRUE (is_subtype(type_of("c1iso"), type_of("t1tag"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1ref"), type_of("t1tag"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1val"), type_of("t1tag"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1box"), type_of("t1tag"), NULL, &opt));
  ASSERT_TRUE (is_subtype(type_of("c1tag"), type_of("t1tag"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeNosupertypeInterface)
{
  const char* src =
    "primitive \\nosupertype\\ P\n"

    "interface Test\n"
    "  fun z(p: P, a: Any val)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_FALSE(is_subtype(type_of("p"), type_of("a"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsEqType)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "interface I1\n"
    "  fun f()\n"

    "interface I1b\n"
    "  fun f()\n"

    "interface I2\n"
    "  fun g()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2 is T2\n"
    "  fun g() => None\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2, i1: I1, i1b: I1b, i2: I2, t1: T1, t2: T2)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_eqtype(type_of("c1"), type_of("c1"), NULL, &opt));
  ASSERT_TRUE(is_eqtype(type_of("c2"), type_of("c2"), NULL, &opt));
  ASSERT_TRUE(is_eqtype(type_of("i1"), type_of("i1"), NULL, &opt));
  ASSERT_TRUE(is_eqtype(type_of("i2"), type_of("i2"), NULL, &opt));
  ASSERT_TRUE(is_eqtype(type_of("t1"), type_of("t1"), NULL, &opt));
  ASSERT_TRUE(is_eqtype(type_of("t2"), type_of("t2"), NULL, &opt));

  ASSERT_FALSE(is_eqtype(type_of("c1"), type_of("c2"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c2"), type_of("c1"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("i1"), type_of("i2"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("i2"), type_of("i1"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("t1"), type_of("t2"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("t2"), type_of("t1"), NULL, &opt));

  ASSERT_FALSE(is_eqtype(type_of("c1"), type_of("i1"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1"), type_of("t1"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("i1"), type_of("c1"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("i1"), type_of("t1"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("t1"), type_of("c1"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("t1"), type_of("i1"), NULL, &opt));

  ASSERT_TRUE(is_eqtype(type_of("i1"), type_of("i1b"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsEqTypeCap)
{
  const char* src =
    "class C1\n"

    "interface Test\n"
    "  fun z(c1iso: C1 iso, c1ref: C1 ref, c1val: C1 val,\n"
    "    c1box: C1 box, c1tag: C1 tag)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE (is_eqtype(type_of("c1iso"), type_of("c1iso"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1ref"), type_of("c1iso"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1val"), type_of("c1iso"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1box"), type_of("c1iso"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1tag"), type_of("c1iso"), NULL, &opt));

  ASSERT_FALSE(is_eqtype(type_of("c1iso"), type_of("c1ref"), NULL, &opt));
  ASSERT_TRUE (is_eqtype(type_of("c1ref"), type_of("c1ref"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1val"), type_of("c1ref"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1box"), type_of("c1ref"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1tag"), type_of("c1ref"), NULL, &opt));

  ASSERT_FALSE(is_eqtype(type_of("c1iso"), type_of("c1val"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1ref"), type_of("c1val"), NULL, &opt));
  ASSERT_TRUE (is_eqtype(type_of("c1val"), type_of("c1val"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1box"), type_of("c1val"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1tag"), type_of("c1val"), NULL, &opt));

  ASSERT_FALSE(is_eqtype(type_of("c1iso"), type_of("c1box"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1ref"), type_of("c1box"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1val"), type_of("c1box"), NULL, &opt));
  ASSERT_TRUE (is_eqtype(type_of("c1box"), type_of("c1box"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1tag"), type_of("c1box"), NULL, &opt));

  ASSERT_FALSE(is_eqtype(type_of("c1iso"), type_of("c1tag"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1ref"), type_of("c1tag"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1val"), type_of("c1tag"), NULL, &opt));
  ASSERT_FALSE(is_eqtype(type_of("c1box"), type_of("c1tag"), NULL, &opt));
  ASSERT_TRUE (is_eqtype(type_of("c1tag"), type_of("c1tag"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsNone)
{
  const char* src =
    "trait T1\n"
    "class C1\n"
    "primitive P1\n"

    "interface Test\n"
    "  fun z(none: None, bool: Bool,\n"
    "    u8: U8, u16: U16, u32: U32, usize: USize, u64: U64, u128: U128,\n"
    "    i8: I8, i16: I16, i32: I32, isize: ISize, i64: I64, i128: I128,\n"
    "    f32: F32, f64: F64,\n"
    "    c1: C1, p1: P1, t1: T1,\n"
    "    union: (Bool | None), intersect: (Bool & None))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_none(type_of("none")));
  ASSERT_FALSE(is_none(type_of("bool")));
  ASSERT_FALSE(is_none(type_of("u8")));
  ASSERT_FALSE(is_none(type_of("u16")));
  ASSERT_FALSE(is_none(type_of("u32")));
  ASSERT_FALSE(is_none(type_of("usize")));
  ASSERT_FALSE(is_none(type_of("u64")));
  ASSERT_FALSE(is_none(type_of("u128")));
  ASSERT_FALSE(is_none(type_of("i8")));
  ASSERT_FALSE(is_none(type_of("i16")));
  ASSERT_FALSE(is_none(type_of("i32")));
  ASSERT_FALSE(is_none(type_of("isize")));
  ASSERT_FALSE(is_none(type_of("i64")));
  ASSERT_FALSE(is_none(type_of("i128")));
  ASSERT_FALSE(is_none(type_of("f32")));
  ASSERT_FALSE(is_none(type_of("f64")));
  ASSERT_FALSE(is_none(type_of("c1")));
  ASSERT_FALSE(is_none(type_of("p1")));
  ASSERT_FALSE(is_none(type_of("t1")));
  ASSERT_FALSE(is_none(type_of("union")));
  ASSERT_FALSE(is_none(type_of("intersect")));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsBool)
{
  const char* src =
    "trait T1\n"
    "class C1\n"
    "primitive P1\n"

    "interface Test\n"
    "  fun z(none: None, bool: Bool,\n"
    "    u8: U8, u16: U16, u32: U32, usize: USize, u64: U64, u128: U128,\n"
    "    i8: I8, i16: I16, i32: I32, isize: ISize, i64: I64, i128: I128,\n"
    "    f32: F32, f64: F64,\n"
    "    c1: C1, p1: P1, t1: T1,\n"
    "    union: (Bool | None), intersect: (Bool & None))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_FALSE(is_bool(type_of("none")));
  ASSERT_TRUE(is_bool(type_of("bool")));
  ASSERT_FALSE(is_bool(type_of("u8")));
  ASSERT_FALSE(is_bool(type_of("u16")));
  ASSERT_FALSE(is_bool(type_of("u32")));
  ASSERT_FALSE(is_bool(type_of("usize")));
  ASSERT_FALSE(is_bool(type_of("u64")));
  ASSERT_FALSE(is_bool(type_of("u128")));
  ASSERT_FALSE(is_bool(type_of("i8")));
  ASSERT_FALSE(is_bool(type_of("i16")));
  ASSERT_FALSE(is_bool(type_of("i32")));
  ASSERT_FALSE(is_bool(type_of("isize")));
  ASSERT_FALSE(is_bool(type_of("i64")));
  ASSERT_FALSE(is_bool(type_of("i128")));
  ASSERT_FALSE(is_bool(type_of("f32")));
  ASSERT_FALSE(is_bool(type_of("f64")));
  ASSERT_FALSE(is_bool(type_of("c1")));
  ASSERT_FALSE(is_bool(type_of("p1")));
  ASSERT_FALSE(is_bool(type_of("t1")));
  ASSERT_FALSE(is_bool(type_of("union")));
  ASSERT_FALSE(is_bool(type_of("intersect")));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsFloat)
{
  const char* src =
    "trait T1\n"
    "class C1\n"
    "primitive P1\n"

    "interface Test\n"
    "  fun z(none: None, bool: Bool,\n"
    "    u8: U8, u16: U16, u32: U32, usize: USize, u64: U64, u128: U128,\n"
    "    i8: I8, i16: I16, i32: I32, isize: ISize, i64: I64, i128: I128,\n"
    "    f32: F32, f64: F64,\n"
    "    c1: C1, p1: P1, t1: T1,\n"
    "    f32ornone: (F32 | None), f32andnone: (F32 & None),\n"
    "    f32or64: (F32 | F64), f32and64: (F32 & F64))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_FALSE(is_float(type_of("none")));
  ASSERT_FALSE(is_float(type_of("bool")));
  ASSERT_FALSE(is_float(type_of("u8")));
  ASSERT_FALSE(is_float(type_of("u16")));
  ASSERT_FALSE(is_float(type_of("u32")));
  ASSERT_FALSE(is_float(type_of("usize")));
  ASSERT_FALSE(is_float(type_of("u64")));
  ASSERT_FALSE(is_float(type_of("u128")));
  ASSERT_FALSE(is_float(type_of("i8")));
  ASSERT_FALSE(is_float(type_of("i16")));
  ASSERT_FALSE(is_float(type_of("i32")));
  ASSERT_FALSE(is_float(type_of("isize")));
  ASSERT_FALSE(is_float(type_of("i64")));
  ASSERT_FALSE(is_float(type_of("i128")));
  ASSERT_TRUE(is_float(type_of("f32")));
  ASSERT_TRUE(is_float(type_of("f64")));
  ASSERT_FALSE(is_float(type_of("c1")));
  ASSERT_FALSE(is_float(type_of("p1")));
  ASSERT_FALSE(is_float(type_of("t1")));
  ASSERT_FALSE(is_float(type_of("f32ornone")));
  ASSERT_FALSE(is_float(type_of("f32andnone")));
  ASSERT_FALSE(is_float(type_of("f32or64")));
  ASSERT_FALSE(is_float(type_of("f32and64")));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsInteger)
{
  const char* src =
    "trait T1\n"
    "class C1\n"
    "primitive P1\n"

    "interface Test\n"
    "  fun z(none: None, bool: Bool,\n"
    "    u8: U8, u16: U16, u32: U32, usize: USize, u64: U64, u128: U128,\n"
    "    i8: I8, i16: I16, i32: I32, isize: ISize, i64: I64, i128: I128,\n"
    "    f32: F32, f64: F64,\n"
    "    c1: C1, p1: P1, t1: T1,\n"
    "    u8ornone: (U8 | None), u8andnone: (U8 & None),\n"
    "    u8ori32: (U8 | I32), u8andi32: (U8 & I32))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_FALSE(is_integer(type_of("none")));
  ASSERT_FALSE(is_integer(type_of("bool")));
  ASSERT_TRUE(is_integer(type_of("u8")));
  ASSERT_TRUE(is_integer(type_of("u16")));
  ASSERT_TRUE(is_integer(type_of("u32")));
  ASSERT_TRUE(is_integer(type_of("usize")));
  ASSERT_TRUE(is_integer(type_of("u64")));
  ASSERT_TRUE(is_integer(type_of("u128")));
  ASSERT_TRUE(is_integer(type_of("i8")));
  ASSERT_TRUE(is_integer(type_of("i16")));
  ASSERT_TRUE(is_integer(type_of("i32")));
  ASSERT_TRUE(is_integer(type_of("isize")));
  ASSERT_TRUE(is_integer(type_of("i64")));
  ASSERT_TRUE(is_integer(type_of("i128")));
  ASSERT_FALSE(is_integer(type_of("f32")));
  ASSERT_FALSE(is_integer(type_of("f64")));
  ASSERT_FALSE(is_integer(type_of("c1")));
  ASSERT_FALSE(is_integer(type_of("p1")));
  ASSERT_FALSE(is_integer(type_of("t1")));
  ASSERT_FALSE(is_integer(type_of("u8ornone")));
  ASSERT_FALSE(is_integer(type_of("u8andnone")));
  ASSERT_FALSE(is_integer(type_of("u8ori32")));
  ASSERT_FALSE(is_integer(type_of("u8andi32")));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsMachineWord)
{
  const char* src =
    "trait T1\n"
    "class C1\n"
    "primitive P1\n"

    "interface Test\n"
    "  fun z(none: None, bool: Bool,\n"
    "    u8: U8, u16: U16, u32: U32, usize: USize, u64: U64, u128: U128,\n"
    "    i8: I8, i16: I16, i32: I32, isize: ISize, i64: I64, i128: I128,\n"
    "    f32: F32, f64: F64,\n"
    "    c1: C1, p1: P1, t1: T1,\n"
    "    u8ornone: (U8 | None), u8andnone: (U8 & None),\n"
    "    u8ori32: (U8 | I32), u8andi32: (U8 & I32))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_FALSE(is_machine_word(type_of("none")));
  ASSERT_TRUE(is_machine_word(type_of("bool")));
  ASSERT_TRUE(is_machine_word(type_of("u8")));
  ASSERT_TRUE(is_machine_word(type_of("u16")));
  ASSERT_TRUE(is_machine_word(type_of("u32")));
  ASSERT_TRUE(is_machine_word(type_of("usize")));
  ASSERT_TRUE(is_machine_word(type_of("u64")));
  ASSERT_TRUE(is_machine_word(type_of("u128")));
  ASSERT_TRUE(is_machine_word(type_of("i8")));
  ASSERT_TRUE(is_machine_word(type_of("i16")));
  ASSERT_TRUE(is_machine_word(type_of("i32")));
  ASSERT_TRUE(is_machine_word(type_of("isize")));
  ASSERT_TRUE(is_machine_word(type_of("i64")));
  ASSERT_TRUE(is_machine_word(type_of("i128")));
  ASSERT_TRUE(is_machine_word(type_of("f32")));
  ASSERT_TRUE(is_machine_word(type_of("f64")));
  ASSERT_FALSE(is_machine_word(type_of("c1")));
  ASSERT_FALSE(is_machine_word(type_of("p1")));
  ASSERT_FALSE(is_machine_word(type_of("t1")));
  ASSERT_FALSE(is_machine_word(type_of("u8ornone")));
  ASSERT_FALSE(is_machine_word(type_of("u8andnone")));
  ASSERT_FALSE(is_machine_word(type_of("u8ori32")));
  ASSERT_FALSE(is_machine_word(type_of("u8andi32")));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsConcrete)
{
  const char* src =
    "trait T1\n"
    "class C1\n"
    "primitive P1\n"

    "interface Test\n"
    "  fun z(none: None, bool: Bool,\n"
    "    u8: U8, u16: U16, u32: U32, usize: USize, u64: U64, u128: U128,\n"
    "    i8: I8, i16: I16, i32: I32, isize: ISize, i64: I64, i128: I128,\n"
    "    f32: F32, f64: F64,\n"
    "    c1: C1, p1: P1, t1: T1,\n"
    "    t1ornone: (T1 | None), t1andnone: (T1 val & None),\n"
    "    c1ort1: (C1 | T1), c1andt1: (C1 & T1))";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_concrete(type_of("none")));
  ASSERT_TRUE(is_concrete(type_of("bool")));
  ASSERT_TRUE(is_concrete(type_of("u8")));
  ASSERT_TRUE(is_concrete(type_of("u16")));
  ASSERT_TRUE(is_concrete(type_of("u32")));
  ASSERT_TRUE(is_concrete(type_of("usize")));
  ASSERT_TRUE(is_concrete(type_of("u64")));
  ASSERT_TRUE(is_concrete(type_of("u128")));
  ASSERT_TRUE(is_concrete(type_of("i8")));
  ASSERT_TRUE(is_concrete(type_of("i16")));
  ASSERT_TRUE(is_concrete(type_of("i32")));
  ASSERT_TRUE(is_concrete(type_of("isize")));
  ASSERT_TRUE(is_concrete(type_of("i64")));
  ASSERT_TRUE(is_concrete(type_of("i128")));
  ASSERT_TRUE(is_concrete(type_of("f32")));
  ASSERT_TRUE(is_concrete(type_of("f64")));
  ASSERT_TRUE(is_concrete(type_of("c1")));
  ASSERT_TRUE(is_concrete(type_of("p1")));
  ASSERT_FALSE(is_concrete(type_of("t1")));
  ASSERT_FALSE(is_concrete(type_of("t1ornone")));
  ASSERT_TRUE(is_concrete(type_of("t1andnone")));
  ASSERT_FALSE(is_concrete(type_of("c1ort1")));
  ASSERT_TRUE(is_concrete(type_of("c1andt1")));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsSubTypeArrow)
{
  const char* src =
    "trait T1\n"
    "interface Test\n"
    "  fun f[A, B: T1 #read](a: A, this_a: this->A, b_a: B->A,\n"
    "    b_b_a: B->B->A, box_a: box->A, val_a: val->A)\n"
    "";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("a"), type_of("a"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("a"), type_of("this_a"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("a"), type_of("b_a"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("a"), type_of("b_b_a"), NULL, &opt));

  ASSERT_FALSE(is_subtype(type_of("this_a"), type_of("a"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("this_a"), type_of("this_a"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("this_a"), type_of("b_a"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("this_a"), type_of("b_b_a"), NULL, &opt));

  ASSERT_FALSE(is_subtype(type_of("b_a"), type_of("a"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("b_a"), type_of("this_a"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("b_a"), type_of("b_a"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("b_a"), type_of("b_b_a"), NULL, &opt));

  ASSERT_FALSE(is_subtype(type_of("b_b_a"), type_of("a"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("b_b_a"), type_of("this_a"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("b_b_a"), type_of("b_a"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("b_b_a"), type_of("b_b_a"), NULL, &opt));

  ASSERT_TRUE(is_subtype(type_of("val_a"), type_of("box_a"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsFunTagSubBe)
{
  const char* src =
    "trait tag T\n"
    "  be foo()\n"

    "class tag C is T\n"
    "  fun tag foo() => None\n"

    "interface Test\n"
    "  fun z(t: T, c: C)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("c"), type_of("t"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsBeSubFunTag)
{
  const char* src =
    "trait tag T\n"
    "  fun tag foo()\n"

    "actor A is T\n"
    "  be foo() => None\n"

    "interface Test\n"
    "  fun z(t: T, a: A)";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("a"), type_of("t"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, IsTypeparamSubIntersect)
{
  const char* src =
    "class B\n"
    "  fun f[A: (I32 & Real[A])](a: A, b: (I32 & Real[A])) =>\n"
    "    None";

  TEST_COMPILE(src);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(is_subtype(type_of("a"), type_of("b"), NULL, &opt));
  ASSERT_TRUE(is_subtype(type_of("b"), type_of("a"), NULL, &opt));

  pass_opt_init(&opt);
}


TEST_F(SubTypeTest, TupleValRefSubAnyBox)
{
  const char* src =
    "class C\n"

    "primitive P\n"
    "  fun apply(x: (C val, C ref)) =>\n"
    "    let x': Any box = consume x";

  TEST_COMPILE(src);
}


TEST_F(SubTypeTest, TupleTagTagSubAnyTag)
{
  const char* src =
    "class C\n"

    "primitive P\n"
    "  fun apply(x: (C tag, C tag)) =>\n"
    "    let x': Any tag = consume x";

  TEST_COMPILE(src);
}


TEST_F(SubTypeTest, TupleTagTagNotSubAnyVal)
{
  const char* src =
    "class C\n"

    "primitive P\n"
    "  fun apply(x: (C tag, C tag)) =>\n"
    "    let x': Any val = consume x";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}


TEST_F(SubTypeTest, TupleRefValOrValSubAnyBox)
{
  const char* src =
    "class C\n"
    "class D\n"

    "primitive P\n"
    "  fun apply(x: (C ref, (C val | D val))) =>\n"
    "    let x': Any box = consume x";

  TEST_COMPILE(src);
}


TEST_F(SubTypeTest, TupleValTagSubAnyShare)
{
  const char* src =
    "class C[A: Any #share]\n"
    "primitive P\n"
    "  fun apply() =>\n"
    "    C[(String val, String tag)]";

  TEST_COMPILE(src);
}


TEST_F(SubTypeTest, TupleValRefNotSubAnyShare)
{
  const char* src =
    "class C[A: Any #share]\n"
    "primitive P\n"
    "  fun apply() =>\n"
    "    C[(String val, String ref)]";

  TEST_ERRORS_1(src, "type argument is outside its constraint");
}


TEST_F(SubTypeTest, BoxArrowTypeParamReifiedWithTypeParam)
{
  const char* src =
    "interface _V[A: _V[A] ref]\n"
    "  fun ref reset(delta: A): A\n"
    "  fun ref converge(other: box->A)\n"

    "class ref Container[V: _V[V] ref] is _V[Container[V]]\n"
    "  let _value: V\n"
    "  new ref create(value': V) => _value = value'\n"

    "  fun ref reset(delta: Container[V]): Container[V] =>\n"
    "    _value.reset(delta._value)\n"
    "    delta\n"

    "  fun ref converge(other: Container[V] box) =>\n"
    "    _value.converge(other._value)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}
TEST_F(SubTypeTest, ConsumeIsoSubRef)
{
  const char* src =
    "class C\n"

    "primitive P\n"
    "  fun apply(x: C iso) =>\n"
    "    let x': C ref = consume x";

  TEST_COMPILE(src);
}
TEST_F(SubTypeTest, IsoNotSubRef)
{
  const char* src =
    "class C\n"

    "primitive P\n"
    "  fun apply(x: C iso) =>\n"
    "    let x': C ref = x";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}
TEST_F(SubTypeTest, CantConsumeRefToIso)
{
  const char* src =
    "class C\n"

    "primitive P\n"
    "  fun apply(x: C ref) =>\n"
    "    let x': C ref = consume iso x";

  TEST_ERRORS_1(src, "can't consume to this capability");
}
TEST_F(SubTypeTest, ConsumeRefNotIso)
{
  const char* src =
    "class C\n"

    "primitive P\n"
    "  fun apply(x: C ref) =>\n"
    "    let x': C iso = consume x";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}

TEST_F(SubTypeTest, RecursiveInterfaceCompiles)
{
  // Guards against the ponylang/ponyc#1216 fix being too aggressive.
  // Self-referential interfaces must still type-check via the existing
  // exact_nominal path, independent of the divergence guard in
  // is_nominal_sub_nominal.
  const char* src =
    "interface I\n"
    "  fun ref f(): I\n"

    "interface J[A]\n"
    "  fun ref g(): J[A]\n"

    "class C is I\n"
    "  fun ref f(): I => this\n"

    "class D[A] is J[A]\n"
    "  fun ref g(): J[A] => this\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: I = C\n"
    "    let d: J[U32] = D[U32]";

  TEST_COMPILE(src);
}

TEST_F(SubTypeTest, MutuallyRecursiveInterfacesCompile)
{
  // Additional ponylang/ponyc#1216 guardrail: mutually-recursive
  // interfaces where each interface's method returns the other must
  // still type-check. The subtype check recurses through B's
  // structural check and back to A, closing the proof via
  // check_assume/exact_nominal across def-pair boundaries rather than
  // inside a single def.
  const char* src =
    "interface A\n"
    "  fun ref get_b(): B\n"

    "interface B\n"
    "  fun ref get_a(): A\n"

    "class ConcreteA is A\n"
    "  new create() => None\n"
    "  fun ref get_b(): B => ConcreteB\n"

    "class ConcreteB is B\n"
    "  new create() => None\n"
    "  fun ref get_a(): A => ConcreteA\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: A = ConcreteA\n"
    "    let b: B = ConcreteB";

  TEST_COMPILE(src);
}

TEST_F(SubTypeTest, IsExactTypeDistinguishesTypeParamScopes)
{
  // Soundness regression covering jemc's review of the
  // ponylang/ponyc#1216 / #5198 / #3930 fix. An earlier iteration of
  // exact_nominal compared assume-stack entries with ast_print_type,
  // which collapses type parameters that share a source name across
  // distinct scopes — `_W[T]` on a parameter of one class and `_W[T]`
  // on a parameter of another class print identically even though
  // each `T` references its own typeparam definition. With that
  // comparison, a coinductive proof in check_assume could close on a
  // pair that only matched by name; that is unsoundness, since the
  // current pair would not actually have been proved.
  //
  // is_exact_type compares definitions by pointer (semantic identity),
  // so it correctly distinguishes the two cases. The test asserts both
  // halves of the soundness story:
  //   1. ast_print_type produces identical strings for the two types
  //      (proving the previous comparison was wrong on this input).
  //   2. is_exact_type returns false (proving the new comparison is
  //      right).
  // Without (1), the test could not distinguish the new fix from the
  // old one — it is the counterfactual that turns this into a
  // regression test for the soundness gap rather than just a behavior
  // check.
  const char* src =
    "class _W[X]\n"

    "class _C1[T]\n"
    "  fun ref method_c1(p_in_c1: _W[T]) => None\n"

    "class _C2[T]\n"
    "  fun ref method_c2(p_in_c2: _W[T]) => None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ast_t* t_c1 = type_of("p_in_c1");
  ast_t* t_c2 = type_of("p_in_c2");
  ASSERT_NE((void*)NULL, t_c1);
  ASSERT_NE((void*)NULL, t_c2);

  ASSERT_STREQ(ast_print_type(t_c1), ast_print_type(t_c2));

  ASSERT_FALSE(is_exact_type(t_c1, t_c2));
}

TEST_F(SubTypeTest, IsExactTypeDistinguishesMethodTypeParamScopes)
{
  // Companion to IsExactTypeDistinguishesTypeParamScopes covering the
  // method-scope collision, which is the more common shape in real
  // code: two methods on the same class each declare their own type
  // parameter `T`. The two `T`s are distinct typeparam definitions
  // and their uses in identically-shaped types must not be conflated.
  const char* src =
    "class _W[X]\n"

    "class _C\n"
    "  fun ref method_a[T](p_in_a: _W[T]) => None\n"
    "  fun ref method_b[T](p_in_b: _W[T]) => None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ast_t* t_a = type_of("p_in_a");
  ast_t* t_b = type_of("p_in_b");
  ASSERT_NE((void*)NULL, t_a);
  ASSERT_NE((void*)NULL, t_b);

  ASSERT_STREQ(ast_print_type(t_a), ast_print_type(t_b));

  ASSERT_FALSE(is_exact_type(t_a, t_b));
}

TEST_F(SubTypeTest, IsExactTypeMatchesSameTypeParamRef)
{
  // Control case: two uses of the same type parameter from the same
  // scope must compare exactly equal. Without this guarantee
  // check_assume could not close legitimate coinductive proofs, and
  // the existing recursive-interface tests would regress.
  const char* src =
    "class _W[X]\n"

    "class _C[T]\n"
    "  fun ref method_a(p_a: _W[T]) => None\n"
    "  fun ref method_b(p_b: _W[T]) => None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ast_t* t_a = type_of("p_a");
  ast_t* t_b = type_of("p_b");
  ASSERT_NE((void*)NULL, t_a);
  ASSERT_NE((void*)NULL, t_b);

  ASSERT_TRUE(is_exact_type(t_a, t_b));
}

TEST_F(SubTypeTest, IsExactTypeMatchesIdenticalConcreteNominals)
{
  // Control case: identical fully-concrete nominals must compare equal.
  const char* src =
    "class _W[X]\n"
    "primitive _Tag\n"

    "class _C\n"
    "  fun ref method_a(p_a: _W[_Tag]) => None\n"
    "  fun ref method_b(p_b: _W[_Tag]) => None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ast_t* t_a = type_of("p_a");
  ast_t* t_b = type_of("p_b");
  ASSERT_NE((void*)NULL, t_a);
  ASSERT_NE((void*)NULL, t_b);

  ASSERT_TRUE(is_exact_type(t_a, t_b));
}

TEST_F(SubTypeTest, IsExactTypeWalksTupleTypes)
{
  // Covers the default walk in exact_type for TK_TUPLETYPE: identical
  // tuple types compare equal; tuples differing in element type or
  // element order compare unequal. The recursive-interface tests
  // exercise this implicitly via `Iter[(B, A)]`, but a direct unit
  // test gives explicit coverage of the compound-walk branch and
  // matches the rigor of the typeparam-scope tests above.
  const char* src =
    "class _C\n"
    "  fun ref method_a(p_a: (U32, String)) => None\n"
    "  fun ref method_b(p_b: (U32, String)) => None\n"
    "  fun ref method_c(p_c: (String, U32)) => None\n"
    "  fun ref method_d(p_d: (U32, U32)) => None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ast_t* t_a = type_of("p_a");
  ast_t* t_b = type_of("p_b");
  ast_t* t_c = type_of("p_c");
  ast_t* t_d = type_of("p_d");
  ASSERT_NE((void*)NULL, t_a);
  ASSERT_NE((void*)NULL, t_b);
  ASSERT_NE((void*)NULL, t_c);
  ASSERT_NE((void*)NULL, t_d);

  ASSERT_TRUE(is_exact_type(t_a, t_b));
  ASSERT_FALSE(is_exact_type(t_a, t_c));
  ASSERT_FALSE(is_exact_type(t_a, t_d));
}

TEST_F(SubTypeTest, IsExactTypeWalksUnionTypes)
{
  // Covers the default walk in exact_type for TK_UNIONTYPE: unions
  // with the same members compare equal; unions with a different
  // member compare unequal. exact_type compares positionally rather
  // than as sets — for the assume-stack use case the same source AST
  // shape always produces the same member order, so positional
  // comparison is correct.
  const char* src =
    "class _C\n"
    "  fun ref method_a(p_a: (U32 | String)) => None\n"
    "  fun ref method_b(p_b: (U32 | String)) => None\n"
    "  fun ref method_c(p_c: (U32 | I32)) => None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ast_t* t_a = type_of("p_a");
  ast_t* t_b = type_of("p_b");
  ast_t* t_c = type_of("p_c");
  ASSERT_NE((void*)NULL, t_a);
  ASSERT_NE((void*)NULL, t_b);
  ASSERT_NE((void*)NULL, t_c);

  ASSERT_TRUE(is_exact_type(t_a, t_b));
  ASSERT_FALSE(is_exact_type(t_a, t_c));
}

TEST_F(SubTypeTest, IsExactTypeWalksIntersectionTypes)
{
  // Covers the default walk in exact_type for TK_ISECTTYPE:
  // intersections with the same members compare equal; intersections
  // with a different member compare unequal. Custom traits avoid any
  // dependence on the exact intersection behavior of stdlib types.
  const char* src =
    "trait _T1\n"
    "trait _T2\n"
    "trait _T3\n"

    "class _C\n"
    "  fun ref method_a(p_a: (_T1 & _T2)) => None\n"
    "  fun ref method_b(p_b: (_T1 & _T2)) => None\n"
    "  fun ref method_c(p_c: (_T1 & _T3)) => None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);

  ast_t* t_a = type_of("p_a");
  ast_t* t_b = type_of("p_b");
  ast_t* t_c = type_of("p_c");
  ASSERT_NE((void*)NULL, t_a);
  ASSERT_NE((void*)NULL, t_b);
  ASSERT_NE((void*)NULL, t_c);

  ASSERT_TRUE(is_exact_type(t_a, t_b));
  ASSERT_FALSE(is_exact_type(t_a, t_c));
}
