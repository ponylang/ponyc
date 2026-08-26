#include <gtest/gtest.h>
#include <platform.h>
#include <ast/astbuild.h>
#include <type/alias.h>
#include <type/matchtype.h>
#include "util.h"
#include "ponyassert.h"

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

  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("c1"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("c2"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c2"), type_of("c1"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("t1"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("c1"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("t2"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("t2"), type_of("c1"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("i1"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("i1"), type_of("c1"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("i2"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("i2"), type_of("c1"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("t2"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("t2"), type_of("t1"), NULL, &opt));
}


TEST_F(MatchTypeTest, Structs)
{
  const char* src =
    "struct S1\n"

    "interface Test\n"
    "  fun z(s1: S1, s1reforNone: (S1 ref | None))\n";

  TEST_COMPILE(src);

  ASSERT_EQ(
    MATCHTYPE_DENY_NODESC,
    is_matchtype(type_of("s1reforNone"), type_of("s1"), NULL, &opt)
  );
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

  // (C1 | C2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1or2"), type_of("c1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1or2"), type_of("c2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1or2"), type_of("t1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1or2"), type_of("t2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1or2"), type_of("i1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1or2"), type_of("i2"), NULL, &opt));

  // (C1 | T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1ort2"), type_of("c1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1ort2"), type_of("c2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1ort2"), type_of("t1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1ort2"), type_of("t2"), NULL, &opt));

  // (T1 & T2)
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("t1and2"), type_of("c1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("t1and2"), type_of("c2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("c3"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("t1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("t2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("t3"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("i1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1and2"), type_of("i2"), NULL, &opt));
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

  // (C1 | C2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("c1or2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c2"), type_of("c1or2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("c1or2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("t2"), type_of("c1or2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("i1"), type_of("c1or2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("i2"), type_of("c1or2"), NULL, &opt));

  // (C1 | T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1"), type_of("c1ort2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c2"), type_of("c1ort2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("c1ort2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t2"), type_of("c1ort2"), NULL, &opt));

  // (T1 & T2)
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("t1and2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c2"), type_of("t1and2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c3"), type_of("t1and2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1"), type_of("t1and2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t2"), type_of("t1and2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t3"), type_of("t1and2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("i1"), type_of("t1and2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("i2"), type_of("t1and2"), NULL, &opt));
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

  // (C1 | C2) vs (T1 | T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1orc2"), type_of("t1ort2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1ort2"), type_of("c1orc2"), NULL, &opt));

  // (C1 | C2) vs (C3 | T2)
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1orc2"), type_of("c3ort2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c3ort2"), type_of("c1orc2"), NULL, &opt));

  // (C1 | C2) vs (T1 & T2)
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1orc2"), type_of("t1andt2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("t1andt2"), type_of("c1orc2"), NULL, &opt));

  // (C1 | C3) vs (T1 & T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1orc3"), type_of("t1andt2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1andt2"), type_of("c1orc3"), NULL, &opt));

  // (T1 & T2) vs (T1 | T2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1andt2"), type_of("t1ort2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("t1ort2"), type_of("t1andt2"), NULL, &opt));

  // (T1 & T2) vs (I1 & I2)
  ASSERT_EQ(
    MATCHTYPE_ACCEPT,
    is_matchtype(type_of("t1andt2"), type_of("i1andi2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT,
    is_matchtype(type_of("i1andi2"), type_of("t1andt2"), NULL, &opt));
}


TEST_F(MatchTypeTest, Tuples)
{
  const char* src =
    "interface I1\n"

    "interface I2\n"
    "  fun f()\n"

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
    "    c2i1: (C2, I1), c3i1tag: (C3, I1 tag),\n"
    "    t1t2: (T1, T2), i1: I1, i2: I2)";

  TEST_COMPILE(src);

  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1"), type_of("c1c1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1c1"), type_of("c1"), NULL, &opt));

  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c1c2"), type_of("t1t2"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("c1c3"), type_of("t1t2"), NULL, &opt));

  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("i1"), type_of("c1c1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("i2"), type_of("c1c1"), NULL, &opt));

  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c2i1"), type_of("c3i1tag"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("c3i1tag"), type_of("c2i1"), NULL, &opt));

  // We can't make types with don't cares in as the type of a parameter. Modify
  // t1t2 instead.
  ast_t* t1t2 = type_of("t1t2");
  AST_GET_CHILDREN(t1t2, elem1, elem2);
  pony_assert(ast_id(elem1) == TK_NOMINAL);
  pony_assert(ast_id(elem2) == TK_NOMINAL);

  REPLACE(&elem2, NODE(TK_DONTCARETYPE));

  // (T1, _)
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), t1t2, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1c2"), t1t2, NULL, &opt));

  REPLACE(&elem1, NODE(TK_DONTCARETYPE));

  // (_, _)
  ASSERT_EQ(MATCHTYPE_REJECT, is_matchtype(type_of("c1"), t1t2, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(type_of("c1c2"), t1t2, NULL, &opt));
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
    "    c1iso: C1 iso,\n"
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

  // Classes
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1ref"), type_of("c1ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1ref"), type_of("c1val"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1ref"), type_of("c1box"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1val"), type_of("c1ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1val"), type_of("c1val"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1val"), type_of("c1box"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1box"), type_of("c1ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1box"), type_of("c1val"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1box"), type_of("c1box"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1box"), type_of("c2ref"), NULL, &opt));

  // Tuples
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1refc2ref"), type_of("c1refc2ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1refc2ref"), type_of("c1valc2ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1refc2ref"), type_of("c1refc2val"), NULL, &opt));

  // Unions
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1reforc2ref"), type_of("c1reforc2ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1reforc2ref"), type_of("c1valorc2ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1reforc2ref"), type_of("c1reforc2val"), NULL, &opt));

  // Intersect vs union
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1refort2ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1valort2ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1refort2val"), NULL, &opt));

  // Intersects
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1refandt2ref"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1valandt2box"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("t1refandt2ref"), type_of("t1refandt2box"), NULL, &opt));

  // Ephemerality
  ast_t* c1iso_bang = alias(type_of("c1iso"), &opt);
  ast_t* c1iso_eph = consume_type(type_of("c1iso"), TK_NONE, false, &opt);
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1iso"), type_of("c1iso"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(c1iso_bang, type_of("c1iso"), NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("c1iso"), c1iso_eph, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(c1iso_eph, c1iso_eph, NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParams)
{
  const char* src =
    "trait T1\n"
    "  fun f()\n"

    "trait T2\n"
    "  fun g()\n"

    "trait T3[A: T3[A]]\n"
    "  fun h()\n"

    "class C1 is T1\n"
    "  fun f() => None\n"

    "class C2\n"

    "interface Test\n"
    "  fun z[A1: C2 ref, A2: T1 ref, A3: T2 ref, A4: T2 box,\n"
    "    A5: (T1 ref | T2 ref), A6: (T3[A6] ref & (C1 ref | C2 ref))]\n"
    "    (c1: C1, c2: C2, t1: T1, t2: T2,\n"
    "    ac2: A1, at1: A2, at2: A3, at2box: A4, aunion: A5, aisect: A6)";

  TEST_COMPILE(src);

  // Ref constraints
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("ac2"), type_of("c1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_REJECT, is_matchtype(type_of("at2"), type_of("c1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("at1"), type_of("c1"), NULL, &opt));
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("at2"), type_of("t1"), NULL, &opt));

  // Box constraint
  ASSERT_EQ(
    MATCHTYPE_DENY_CAP, is_matchtype(type_of("at2box"), type_of("t1"), NULL, &opt));

  // Union constraint
  ASSERT_EQ(
    MATCHTYPE_ACCEPT, is_matchtype(type_of("aunion"), type_of("c1"), NULL, &opt));;

  // Intersection of generic trait and union constraint
  ASSERT_EQ(
    MATCHTYPE_ACCEPT,
    is_matchtype(type_of("aisect"), type_of("aisect"), NULL, &opt));}


TEST_F(MatchTypeTest, GenericCap)
{
  const char* src =
    "interface tag I\n"

    "interface Test\n"
    "  fun z[Read: I #read, Send: I #send, Share: I #share, Alias: I #alias,\n"
    "        AnyT: I #any]\n"
    "    (read: Read, send: Send, share: Share, alias: Alias, any: AnyT,\n"
    "     iso': I iso, trn': I trn, ref': I ref, val': I val, box': I box,\n"
    "     tag': I tag)";

  TEST_COMPILE(src);

  // Use ephemeral types for gencaps with unique caps in order to get unaliased
  // match operand types.
  ast_t* send_base = type_of("send");
  ast_t* any_base = type_of("any");

  ast_t* read = type_of("read");
  ast_t* send = consume_type(send_base, TK_NONE, true, &opt);
  ast_t* share = type_of("share");
  ast_t* alias = type_of("alias");
  ast_t* any = consume_type(any_base, TK_NONE, true, &opt);

  ast_t* iso = type_of("iso'");
  ast_t* trn = type_of("trn'");
  ast_t* ref = type_of("ref'");
  ast_t* val = type_of("val'");
  ast_t* box = type_of("box'");
  ast_t* tag = type_of("tag'");

  // #read {ref, val, box}
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(read, iso, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(read, trn, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(read, ref, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(read, val, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(read, box, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(read, tag, NULL, &opt));

  // #send {iso, val, tag}
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(send, iso, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(send, trn, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(send, ref, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(send, val, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(send, box, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(send, tag, NULL, &opt));

  // #share {val, tag}
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(share, iso, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(share, trn, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(share, ref, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(share, val, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(share, box, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(share, tag, NULL, &opt));

  // #alias {ref, val, box, tag}
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(alias, iso, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(alias, trn, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(alias, ref, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(alias, val, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(alias, box, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(alias, tag, NULL, &opt));

  // #any {iso, trn, ref, val, box, tag}
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(any, iso, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(any, trn, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(any, ref, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(any, val, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_DENY_CAP, is_matchtype(any, box, NULL, &opt));
  ASSERT_EQ(MATCHTYPE_ACCEPT, is_matchtype(any, tag, NULL, &opt));

  if(send != send_base)
    ast_free_unattached(send);

  if(any != any_base)
    ast_free_unattached(any);
}


// Regression tests for ponylang/ponyc#723: a type parameter appearing inside
// a type argument was being rejected with "this pattern can never match"
// even though the parameter could reify to make the pair equal at runtime.
TEST_F(MatchTypeTest, TypeParamInTypeArgSameDef)
{
  const char* src =
    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "class val C2[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (c1_a: C1[A], c1_wrap_b: C1[Wrap[B] val], c1_u8: C1[U8],\n"
    "     c1_wrap_u8: C1[Wrap[U8] val], c1_wrap_a: C1[Wrap[A] val],\n"
    "     c1_wrap_wrap_b: C1[Wrap[Wrap[B] val] val],\n"
    "     c2_a: C2[A])";

  TEST_COMPILE(src);

  // C1[A] operand, C1[Wrap[B]] pattern — the #723 shape: A could reify to
  // Wrap[B], so this must accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_wrap_b"), NULL, &opt));

  // Symmetric direction.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_wrap_b"), type_of("c1_a"), NULL, &opt));

  // One-level nested with a concrete inner: C1[Wrap[A]] operand vs
  // C1[Wrap[U8]] pattern. A could reify to U8, making the pair equal.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_wrap_a"), type_of("c1_wrap_u8"), NULL, &opt));

  // Recursive nesting: C1[Wrap[A]] operand vs C1[Wrap[Wrap[B]]] pattern.
  // A could reify to Wrap[B], making Wrap[A] equal to Wrap[Wrap[B]].
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_wrap_a"), type_of("c1_wrap_wrap_b"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_u8"), type_of("c1_u8"), NULL, &opt));

  // Fully concrete mismatch stays a compile-time reject — keeps the
  // "this pattern can never match" diagnostic for statically impossible
  // matches.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_u8"), type_of("c1_wrap_u8"), NULL, &opt));

  // A concrete type against a type-parameter-bearing pattern where the
  // concrete side has nothing to substitute: U8 cannot become Wrap[B]
  // under any reification of B.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_u8"), type_of("c1_wrap_b"), NULL, &opt));

  // Different definitions never match, regardless of type-parameter presence.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c2_a"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTypeAlias)
{
  // A type alias reference in a type-argument position must be unfolded
  // before the pair comparison.
  const char* src =
    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "type WrapAlias[X: Any #share] is Wrap[X] val\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (c1_a: C1[A], c1_wrap_alias_b: C1[WrapAlias[B]])";

  TEST_COMPILE(src);

  // C1[A] operand vs C1[WrapAlias[B]] pattern. WrapAlias[B] unfolds to
  // Wrap[B] val — the same shape as TypeParamInTypeArgSameDef's accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_wrap_alias_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnconstrained)
{
  // Bare type parameters produce a NULL upper bound (typeparam_upper).
  // The "unconstrained typeparam unifies with anything" branch must accept
  // any reification-compatible partner.
  const char* src =
    "class val Cell[A]\n"

    "interface Test\n"
    "  fun z[A, B](cell_a: Cell[A], cell_b: Cell[B],\n"
    "              cell_u8: Cell[U8])";

  TEST_COMPILE(src);

  // Two unconstrained typeparams — any reification is possible.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cell_a"), type_of("cell_b"), NULL, &opt));

  // Unconstrained typeparam vs concrete — the concrete is a valid
  // reification.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cell_a"), type_of("cell_u8"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgMultiArg)
{
  // A multi-argument generic exercises the pairwise iteration in
  // match_typeargs_pairwise. A wrong implementation that stopped at the
  // first accepting pair, or got deny/reject precedence wrong, would pass
  // the earlier single-argument tests.
  const char* src =
    "class val Pair[A: Any #share, B: Any #share]\n"
    "  let first: A\n"
    "  let second: B\n"
    "  new val create(a: A, b: B) => first = a; second = b\n"

    "struct SPair[A: Any #share, B: Any #share]\n"
    "  let first: A\n"
    "  let second: B\n"
    "  new create(a: A, b: B) => first = a; second = b\n"

    "interface Test\n"
    "  fun z[A: Any #share, U: U8, I: I32]\n"
    "    (p_a_u: Pair[A, U], p_u_i: Pair[U, I],\n"
    "     p_a_u8: Pair[A, U8], p_u_u8: Pair[U, U8],\n"
    "     sp_u8_a: SPair[U8, A], sp_u8_u8: SPair[U8, U8],\n"
    "     sp_u8_i32: SPair[U8, I32])";

  TEST_COMPILE(src);

  // First pair could unify (A could reify to U), second pair could not
  // (U ~ U8, I ~ I32 — disjoint constraints). Whole list rejects.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("p_a_u"), type_of("p_u_i"), NULL, &opt));

  // Both pairs unify (A could reify to U, U8 concrete matches U8).
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("p_a_u8"), type_of("p_u_u8"), NULL, &opt));

  // Struct pairwise: first pair is eqtype-concrete (U8 vs U8), second
  // contains a typeparam — deny_nodesc must propagate for the whole list.
  ASSERT_EQ(MATCHTYPE_DENY_NODESC,
    is_matchtype(type_of("sp_u8_u8"), type_of("sp_u8_a"), NULL, &opt));

  // Struct pairwise, both concrete: first eqtype, second concrete-unequal
  // — reject (delegated to the entity path via the concrete fallthrough).
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("sp_u8_u8"), type_of("sp_u8_i32"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTypeParamPair)
{
  // Two distinct type parameters, one on each side of the type-argument
  // pair. When both constraints admit a common concrete type, the pair
  // could unify at runtime — accept. When constraints are disjoint,
  // unification is impossible — reject.
  const char* src =
    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share,\n"
    "        U: U8, I: I32]\n"
    "    (c1_a: C1[A], c1_b: C1[B],\n"
    "     c1_u: C1[U], c1_i: C1[I])";

  TEST_COMPILE(src);

  // C1[A] vs C1[B]: constraints are the same (Any #share), so a common
  // reification exists — accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_b"), NULL, &opt));

  // C1[U] vs C1[I]: U is constrained to U8, I to I32; no type is both —
  // reject.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_u"), type_of("c1_i"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgConstraintIncompatible)
{
  // Constraint-incompatible reification: the type parameter's constraint
  // does not admit the concrete pattern, so no reification could make the
  // pair equal — reject in either direction.
  const char* src =
    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: I32 val]\n"
    "    (c1_a: C1[A], c1_string: C1[String val])";

  TEST_COMPILE(src);

  // A is constrained to I32; matching C1[A] against C1[String val] cannot
  // succeed under any reification — String is not a subtype of I32.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_string"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_string"), type_of("c1_a"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgOccurs)
{
  // Occurs check: when the same type parameter appears on one side of a
  // type-argument pair and inside the other, no reification satisfies
  // A = f(A) without an infinite type, so the pair cannot unify.
  const char* src =
    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "type WrapAlias[X: Any #share] is Wrap[X] val\n"

    "primitive P\n"
    "trait val P1\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (c1_a: C1[A], c1_wrap_a: C1[Wrap[A] val],\n"
    "     c1_wrap_wrap_a: C1[Wrap[Wrap[A] val] val],\n"
    "     c1_wrap_alias_a: C1[WrapAlias[A]],\n"
    "     c1_wrap_wrap_alias_a: C1[Wrap[WrapAlias[A]] val],\n"
    "     c1_union_wrap_a: C1[(P | Wrap[A] val)],\n"
    "     c1_tuple_wrap_a: C1[(P, Wrap[A] val)],\n"
    "     c1_isect_wrap_a: C1[(P1 & Wrap[A] val)],\n"
    "     c1_union_direct_a: C1[(A | U8)],\n"
    "     c1_isect_direct_a: C1[(P1 & A)],\n"
    "     c1_wrap_b: C1[Wrap[B] val])";

  TEST_COMPILE(src);

  // Direct occurs: A on one side, Wrap[A] on the other.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_wrap_a"), NULL, &opt));

  // Reversed direction.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_wrap_a"), type_of("c1_a"), NULL, &opt));

  // Nested wrapping: A appears two levels deep.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_wrap_wrap_a"), NULL, &opt));

  // Alias on the pattern side: WrapAlias[A] unfolds to Wrap[A] val.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_wrap_alias_a"), NULL, &opt));

  // Alias nested under a wrapper: Wrap[WrapAlias[A]] — the alias is inside
  // the wrapper, so the pattern-side alias is not unfolded before the
  // occurs walk starts.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_wrap_wrap_alias_a"), NULL, &opt));

  // Union arm carrying occurs through a wrapper: A appears inside Wrap in
  // one arm.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_union_wrap_a"), NULL, &opt));

  // Tuple element carrying occurs through a wrapper.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_tuple_wrap_a"), NULL, &opt));

  // Intersection arm carrying occurs through a wrapper.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_isect_wrap_a"), NULL, &opt));

  // A appears directly as a union arm — not under any generative wrapper.
  // A can reify to (U8 | I32); the pattern reifies to ((U8 | I32) | U8),
  // which equals (U8 | I32) = A. Must accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_union_direct_a"), NULL, &opt));

  // A appears directly as an intersection arm — not under a generative
  // wrapper. Must accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_isect_direct_a"), NULL, &opt));

  // Regression: distinct type parameters, one wrapped in the other. Occurs
  // keys on parameter identity, not "any typeparam present" — this pair
  // must still accept because A can reify to Wrap[B].
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_wrap_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgStructDenies)
{
  // Structs have no runtime type descriptor, so type arguments cannot be
  // checked at runtime. A same-def struct pattern whose type arguments
  // include a type parameter must not accept — the runtime would treat the
  // match as unconditional. Deny with the "lacks a type descriptor"
  // diagnostic rather than the "can never match" reject.
  const char* src =
    "struct SGen[A: Any #share]\n"
    "  let value: A\n"
    "  new create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share, I: I32 val]\n"
    "    (s_a: SGen[A], s_b: SGen[B], s_u8: SGen[U8], s_i32: SGen[I32],\n"
    "     s_i: SGen[I], s_string: SGen[String val])";

  TEST_COMPILE(src);

  // Same-def struct, both type parameters — deny_nodesc.
  ASSERT_EQ(MATCHTYPE_DENY_NODESC,
    is_matchtype(type_of("s_a"), type_of("s_b"), NULL, &opt));

  // Same-def struct, one type parameter one concrete — deny_nodesc.
  ASSERT_EQ(MATCHTYPE_DENY_NODESC,
    is_matchtype(type_of("s_a"), type_of("s_u8"), NULL, &opt));

  // Same-def struct, both concrete equal — accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("s_u8"), type_of("s_u8"), NULL, &opt));

  // Same-def struct, both concrete unequal — reject.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("s_u8"), type_of("s_i32"), NULL, &opt));

  // Same-def struct, constraint-incompatible type parameter vs concrete —
  // pin the current behavior: still deny_nodesc, not reject. The
  // struct_pattern gate denies any type-parameter-bearing pair regardless
  // of whether the constraint could admit the pattern.
  ASSERT_EQ(MATCHTYPE_DENY_NODESC,
    is_matchtype(type_of("s_i"), type_of("s_string"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTraitProvides)
{
  // Class nominally provides a trait; pattern is the same trait with a
  // type parameter in its type argument. The type parameter on the
  // operand side could reify to make the pair equal at runtime — accept.
  // See #5859.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "trait val T[A: Any #share]\n"
    "  fun get_value(): A\n"

    "trait val U[A: Any #share]\n"
    "  fun other(): A\n"

    "class val Cons[A: Any #share] is T[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun get_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (cons_a: Cons[A], t_wrap_b: T[Wrap[B] val] val,\n"
    "     t_u8: T[U8] val, cons_u8: Cons[U8],\n"
    "     u_wrap_b: U[Wrap[B] val] val)";

  TEST_COMPILE(src);

  // Cons[A] operand, T[Wrap[B]] pattern — Cons provides T[A]; A could
  // reify to Wrap[B], so this must accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cons_a"), type_of("t_wrap_b"), NULL, &opt));

  // Cons[U8] operand, T[U8] pattern — the strict subtype check already
  // accepts this. Pin that it still accepts after the fallback runs.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cons_u8"), type_of("t_u8"), NULL, &opt));

  // Cons[U8] operand, T[Wrap[B]] pattern — Cons provides T[U8]; U8 is
  // concrete and cannot equal Wrap[B] under any reification of B. Reject.
  // The strict subtype check already rejects this; the assertion pins
  // that the fallback doesn't over-accept.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("cons_u8"), type_of("t_wrap_b"), NULL, &opt));

  // Cons[A] operand, U[Wrap[B]] pattern — Cons doesn't provide U at all.
  // No reification can add a provides declaration; reject. The strict
  // subtype check already rejects this; the assertion pins that the
  // fallback doesn't over-accept (it's the issue's literal repro for
  // the trait side).
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("cons_a"), type_of("u_wrap_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTraitOccurs)
{
  // Occurs check on the trait-provides path: when the operand's provided
  // trait's type argument shares a type parameter with the pattern's type
  // argument such that the pattern's argument wraps that parameter, no
  // reification satisfies the equality.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "trait val T[A: Any #share]\n"
    "  fun get_value(): A\n"

    "class val Cons[A: Any #share] is T[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun get_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (cons_a: Cons[A], t_wrap_a: T[Wrap[A] val] val,\n"
    "     t_wrap_wrap_a: T[Wrap[Wrap[A] val] val] val)";

  TEST_COMPILE(src);

  // Cons[A] provides T[A]; matched against T[Wrap[A]], the pair is
  // A vs Wrap[A] — A appears inside Wrap[A], reject.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("cons_a"), type_of("t_wrap_a"), NULL, &opt));

  // Nested wrapping on the trait path.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("cons_a"), type_of("t_wrap_wrap_a"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgInterfaceProvides)
{
  // Class nominally provides an interface; pattern is the same interface
  // with a type parameter in its type argument. See #5859.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface val I[A: Any #share]\n"
    "  fun get_value(): A\n"

    "class val Cons[A: Any #share] is I[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun get_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (cons_a: Cons[A], i_wrap_b: I[Wrap[B] val] val,\n"
    "     cons_u8: Cons[U8], i_wrap_u8: I[Wrap[U8] val] val)";

  TEST_COMPILE(src);

  // Cons[A] operand, I[Wrap[B]] pattern — Cons provides I[A]; A could
  // reify to Wrap[B], so this must accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cons_a"), type_of("i_wrap_b"), NULL, &opt));

  // Cons[U8] operand, I[Wrap[U8]] pattern — Cons provides I[U8]; U8 is
  // concrete and cannot equal Wrap[U8]. Reject. The strict subtype
  // check already rejects this; the assertion pins that the fallback
  // doesn't over-accept.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("cons_u8"), type_of("i_wrap_u8"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgInterfaceOccurs)
{
  // Occurs check on the interface-provides path.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface val I[A: Any #share]\n"
    "  fun get_value(): A\n"

    "class val Cons[A: Any #share] is I[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun get_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (cons_a: Cons[A], i_wrap_a: I[Wrap[A] val] val,\n"
    "     i_wrap_wrap_a: I[Wrap[Wrap[A] val] val] val)";

  TEST_COMPILE(src);

  // Cons[A] provides I[A]; matched against I[Wrap[A]], the pair is
  // A vs Wrap[A] — A appears inside Wrap[A], reject.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("cons_a"), type_of("i_wrap_a"), NULL, &opt));

  // Nested wrapping on the interface path.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("cons_a"), type_of("i_wrap_wrap_a"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTraitTransitiveProvides)
{
  // The provides walk is transitive: Cons is Mid, Mid is Top, pattern is
  // Top with a type parameter in its type argument. The reified Mid[A]
  // yields Top[A] via Mid's own provides declaration; A could reify to
  // Wrap[B].
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "trait val Top[A: Any #share]\n"
    "  fun top_value(): A\n"

    "trait val Mid[A: Any #share] is Top[A]\n"

    "class val Cons[A: Any #share] is Mid[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun top_value(): A => _v\n"

    "class val Other[A: Any #share] is Mid[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun top_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (cons_a: Cons[A], top_wrap_b: Top[Wrap[B] val] val,\n"
    "     other_u8: Other[U8], top_wrap_u8: Top[Wrap[U8] val] val)";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cons_a"), type_of("top_wrap_b"), NULL, &opt));

  // Transitive walk with a concrete-only operand: Other[U8] provides
  // Mid[U8] provides Top[U8]; U8 can't equal Wrap[U8] at any layer.
  // The strict subtype check already rejects this; the assertion pins
  // that the fallback doesn't over-accept along the transitive walk.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("other_u8"), type_of("top_wrap_u8"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgActorAndPrimitiveOperand)
{
  // The fallback applies to every entity operand kind is_nominal_match_trait
  // routes to is_entity_match_trait. Pin actor and primitive operands
  // separately from the class cases above.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "trait tag T[A: Any #share]\n"
    "  be do_thing()\n"

    "trait val P[A: Any #share]\n"
    "  fun label(): U64\n"

    "actor ActorCons[A: Any #share] is T[A]\n"
    "  let _v: A\n"
    "  new create(v: A) => _v = v\n"
    "  be do_thing() => None\n"

    "primitive PrimCons[A: Any #share] is P[A]\n"
    "  fun label(): U64 => 0\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (actor_cons_a: ActorCons[A],\n"
    "     t_wrap_b: T[Wrap[B] val] tag,\n"
    "     prim_cons_a: PrimCons[A],\n"
    "     p_wrap_b: P[Wrap[B] val] val)";

  TEST_COMPILE(src);

  // ActorCons[A] operand, T[Wrap[B]] pattern — A could reify to Wrap[B].
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("actor_cons_a"), type_of("t_wrap_b"), NULL, &opt));

  // PrimCons[A] operand, P[Wrap[B]] pattern — same shape via primitive.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("prim_cons_a"), type_of("p_wrap_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTraitPatternCapDenies)
{
  // The fallback is cap-agnostic; the outer cap check in
  // is_entity_match_trait must still deny when the operand refcap can't
  // become the pattern refcap.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "trait val T[A: Any #share]\n"
    "  fun get_value(): A\n"

    "class ref Cons[A: Any #share] is T[A]\n"
    "  let _v: A\n"
    "  new ref create(v: A) => _v = v\n"
    "  fun get_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (cons_a: Cons[A] ref, t_wrap_b: T[Wrap[B] val] val)";

  TEST_COMPILE(src);

  // Cons[A] ref cannot become T[Wrap[B]] val: the fallback finds the
  // same-def provides match, but the outer cap check must deny.
  ASSERT_EQ(MATCHTYPE_DENY_CAP,
    is_matchtype(type_of("cons_a"), type_of("t_wrap_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTraitProvidesTypeAlias)
{
  // A class provides via a type alias. The alias should resolve so the
  // walk finds the trait via the reified provides list.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "trait val Base[A: Any #share]\n"
    "  fun get_value(): A\n"

    "type BaseAlias[X: Any #share] is Base[X] val\n"

    "class val Cons[A: Any #share] is BaseAlias[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun get_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (cons_a: Cons[A], base_wrap_b: Base[Wrap[B] val] val)";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cons_a"), type_of("base_wrap_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTraitProvidesSiblings)
{
  // A class provides more than one trait via an intersection. The pass/
  // flatten step expands the intersection into sibling children in the
  // provides list; pin that the walk finds the matching sibling in
  // either position.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "trait val Left[A: Any #share]\n"
    "  fun left_value(): A\n"

    "trait val Right[A: Any #share]\n"
    "  fun right_value(): A\n"

    "class val Cons[A: Any #share] is (Left[A] & Right[A])\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun left_value(): A => _v\n"
    "  fun right_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (cons_a: Cons[A],\n"
    "     left_wrap_b: Left[Wrap[B] val] val,\n"
    "     right_wrap_b: Right[Wrap[B] val] val)";

  TEST_COMPILE(src);

  // Walk finds the first sibling.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cons_a"), type_of("left_wrap_b"), NULL, &opt));

  // Walk continues past the first sibling to find the second.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("cons_a"), type_of("right_wrap_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTraitProvidesConstraintIncompatible)
{
  // The operand's type parameter's constraint doesn't admit the pattern's
  // concrete type argument — no reification could make the pair equal.
  const char* src =
    "trait val T[A: Any #share]\n"
    "  fun get_value(): A\n"

    "class val Cons[A: I32 val] is T[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun get_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: I32 val]\n"
    "    (cons_a: Cons[A], t_string: T[String val] val)";

  TEST_COMPILE(src);

  // A is constrained to I32; the pattern's type argument is String.
  // No reification of A makes T[A] equal T[String].
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("cons_a"), type_of("t_string"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgStructOperandTraitRejects)
{
  // A struct has no runtime type descriptor, so matching a struct
  // operand against a trait or interface pattern has no runtime
  // discriminator; is_struct_sub_trait denies unconditionally. The
  // provides-walk fallback must not bypass that denial — accepting here
  // would compile a match with no runtime check.
  const char* src =
    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "trait val T[A: Any #share]\n"
    "  fun get_value(): A\n"

    "struct val S[A: Any #share] is T[A]\n"
    "  let _v: A\n"
    "  new val create(v: A) => _v = v\n"
    "  fun get_value(): A => _v\n"

    "interface Test\n"
    "  fun z[A: Any #share, B: Any #share]\n"
    "    (s_a: S[A], t_wrap_b: T[Wrap[B] val] val)";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("s_a"), type_of("t_wrap_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgConstraintOverlap)
{
  // Two type parameters whose constraints only overlap through a common
  // subtype. Both admit P2, so a reification A = B = P2 makes the pair
  // equal — accept.
  const char* src =
    "primitive P1\n"
    "primitive P2\n"
    "primitive P3\n"
    "primitive P4\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: (P1 | P2), B: (P2 | P3), D: (P3 | P4)]\n"
    "    (c1_a: C1[A], c1_b: C1[B], c1_d: C1[D])";

  TEST_COMPILE(src);

  // Constraints share P2 — accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_b"), NULL, &opt));

  // Symmetric direction.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_b"), type_of("c1_a"), NULL, &opt));

  // (P1 | P2) and (P3 | P4) — no common member. Reject; compile check
  // preserved.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_d"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgConstraintOverlapTrait)
{
  // Two type parameters constrained to disjoint traits: neither
  // constraint is a subtype of the other, and precise inhabitation of
  // the two-trait intersection would require walking every class in the
  // program. The check accepts unconditionally when either side is a
  // trait or interface — a common implementer, if one exists (like the
  // class `Both` here), makes runtime match legitimate; when none
  // exists, the runtime descriptor check discriminates.
  const char* src =
    "trait val Named\n"
    "  fun name(): String\n"
    "trait val Aged\n"
    "  fun age(): U32\n"

    "class val Both is (Named & Aged)\n"
    "  fun name(): String => \"x\"\n"
    "  fun age(): U32 => 0\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Named val, B: Aged val]\n"
    "    (c1_a: C1[A], c1_b: C1[B])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgConstraintOverlapTraitNoImplementer)
{
  // Same trait-vs-trait shape as TypeParamInTypeArgConstraintOverlapTrait
  // but with no common implementer in the program. The check still
  // accepts — the constraint-overlap helper doesn't attempt trait
  // inhabitation, so the presence or absence of a common implementer
  // isn't consulted. Runtime descriptor identity discriminates, so an
  // accepted match with no legitimate reification simply never fires.
  const char* src =
    "trait val Named\n"
    "  fun name(): String\n"
    "trait val Aged\n"
    "  fun age(): U32\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Named val, B: Aged val]\n"
    "    (c1_a: C1[A], c1_b: C1[B])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_b"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgConstraintDisjointConcrete)
{
  // Two type parameters constrained to different concrete entities cannot
  // share any inhabitant — Pony has no user-facing subclassing. Covers
  // class, primitive, and actor kinds pairwise. Struct constraints can't
  // satisfy `Any` (structs aren't subtypes of interfaces), so a struct
  // never reaches the constraint-overlap helper.
  const char* src =
    "primitive P1\n"
    "actor A1\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[Ac: String val, Bp: U8, Cp: P1, Ea: A1 tag]\n"
    "    (c1_class_str: C1[Ac], c1_prim_u8: C1[Bp],\n"
    "     c1_prim_p1: C1[Cp], c1_actor: C1[Ea])";

  TEST_COMPILE(src);

  // class vs primitive
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_class_str"), type_of("c1_prim_u8"), NULL, &opt));

  // primitive vs primitive (different defs)
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_prim_u8"), type_of("c1_prim_p1"), NULL, &opt));

  // class vs actor
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_class_str"), type_of("c1_actor"), NULL, &opt));

  // primitive vs actor
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_prim_p1"), type_of("c1_actor"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionArm)
{
  // Type argument is a union containing a type parameter. Distributing
  // over arms: A can reify to U8, making the two type arguments equal
  // as sets — accept.
  const char* src =
    "primitive P\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (c1_a: C1[(P | A)], c1_p_u8: C1[(P | U8)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_a"), type_of("c1_p_u8"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_p_u8"), type_of("c1_a"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionArmDisjoint)
{
  // Union arms on both sides but no arm on the operand side can line up
  // with a pattern arm. No reification of A makes the pair equal —
  // reject.
  const char* src =
    "primitive Foo\n"
    "primitive Bar\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: U16]\n"
    "    (c1_left: C1[(Foo | A)], c1_right: C1[(Bar | U8)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_left"), type_of("c1_right"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionArmConcreteMismatch)
{
  // Two unions of concrete primitives that are not eqtype: rejected
  // because both sides are typeparam-free, so no reification could
  // relate them. Companion to TypeParamInTypeArgUnionArmMismatchTyped,
  // which exercises the same rejection through the compound branch
  // when a typeparam is present.
  const char* src =
    "primitive P\n"
    "primitive Q\n"
    "primitive R\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z (c1_pq: C1[(P | Q)], c1_pqr: C1[(P | Q | R)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_pq"), type_of("c1_pqr"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_pqr"), type_of("c1_pq"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionArmMismatchTyped)
{
  // Union-vs-union in the compound branch: pattern has an arm no operand
  // arm can supply even under any reification of the operand's
  // typeparams. Neither `Ai` (constrained to (I32 | I8)) nor `Au`
  // (constrained to (U32 | U8)) can reify to `String`, so the pattern
  // arm String has no operand match.
  const char* src =
    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[Ai: (I32 | I8), Au: (U32 | U8)]\n"
    "    (c1_ai_au: C1[(Ai | Au)],\n"
    "     c1_ai_au_string: C1[(Ai | Au | String val)])";

  TEST_COMPILE(src);

  // Pattern arm String has no operand-side match; neither Ai nor Au can
  // reify to String. Reject via compound branch.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_ai_au"), type_of("c1_ai_au_string"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionArmSameTypeParam)
{
  // `(P | A)` vs `(Q | A)` with the same unconstrained `A` on both
  // sides — accepted by the compound branch's mutual arm-covering rule
  // (P<->A, A<->Q both unify). A CAN reify to a union such as (P | Q),
  // making `(P | (P|Q))` and `(Q | (P|Q))` both equal to `(P | Q)`, so
  // the accept is legitimate for that reification. The check does not
  // require every arm-vs-arm choice to share a consistent reification,
  // same as the same-def nominal recursion accepts `Pair[A, A]` vs
  // `Pair[U8, U16]` without cross-position consistency.
  const char* src =
    "primitive P\n"
    "primitive Q\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (c1_pa: C1[(P | A)], c1_qa: C1[(Q | A)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_pa"), type_of("c1_qa"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_qa"), type_of("c1_pa"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionArmOccurs)
{
  // Occurs check propagates through union arms: A vs `(P | Wrap[A])` —
  // A appears wrapped in the union arm, so no reification satisfies
  // A = (P | Wrap[A]) without an infinite type. Reject.
  const char* src =
    "primitive P\n"

    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (c1_a: C1[A], c1_union_wrap_a: C1[(P | Wrap[A] val)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_union_wrap_a"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_union_wrap_a"), type_of("c1_a"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTypeParamVsCompoundConstraintReject)
{
  // Bare typeparam on one side, union on the other. A's constraint (I32)
  // doesn't admit the union, so no reification of A makes the pair
  // equal — reject.
  const char* src =
    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: I32]\n"
    "    (c1_a: C1[A], c1_string_u8: C1[(String val | U8)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_string_u8"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_string_u8"), type_of("c1_a"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionVsNominal)
{
  // Union on one side, non-compound on the other. Every arm of the
  // union must be able to unify with the nominal — for the union to
  // equal the nominal at runtime, every arm must reify to that nominal.
  const char* src =
    "primitive P\n"
    "primitive Q\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share, Aq: (P | Q)]\n"
    "    (c1_p_a: C1[(P | A)], c1_p: C1[P],\n"
    "     c1_q_aq: C1[(Q | Aq)], c1_q: C1[Q])";

  TEST_COMPILE(src);

  // (P | A) vs P: P arm is P, A arm can reify to P. Reification A = P
  // collapses (P | P) to P. Accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_p_a"), type_of("c1_p"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_p"), type_of("c1_p_a"), NULL, &opt));

  // (Q | Aq) vs Q: Q arm is Q, Aq arm — its constraint is (P | Q), so it
  // can reify to Q. Accept.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_q_aq"), type_of("c1_q"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_q"), type_of("c1_q_aq"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionVsNominalDisjoint)
{
  // Compound-vs-non-compound branch reject: even under any reification
  // of the operand's typeparam, one arm can never equal the pattern
  // nominal, so the union can never collapse to the pattern's shape.
  const char* src =
    "primitive P\n"
    "primitive Q\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share] (c1_qa: C1[(Q | A)], c1_p: C1[P])";

  TEST_COMPILE(src);

  // (Q | A) vs P: Q arm can never equal P. Reject even though A could
  // reify to P.
  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_qa"), type_of("c1_p"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionArmStructDenies)
{
  // Struct pattern with a typeparam inside a union arm must still
  // deny_nodesc — a struct has no runtime descriptor, so a type
  // argument containing a typeparam cannot be discriminated at
  // runtime regardless of how the compound arms line up.
  const char* src =
    "primitive P\n"

    "struct val SGen[A: Any #share]\n"
    "  let value: A\n"
    "  new create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (s_p_a: SGen[(P | A)], s_p_u8: SGen[(P | U8)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_DENY_NODESC,
    is_matchtype(type_of("s_p_a"), type_of("s_p_u8"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgMixedCompoundArms)
{
  // Mixed-kind compound arms: one type argument is a union, the other is
  // an intersection. Accepts iff any arm-vs-arm pair could unify — a
  // common subtype supplies a valid reification. See compound_types_
  // could_unify's mixed-kinds rule.
  const char* src =
    "trait val Named\n"
    "  fun name(): String\n"
    "trait val Aged\n"
    "  fun age(): U32\n"

    "class val Both is (Named & Aged)\n"
    "  fun name(): String => \"x\"\n"
    "  fun age(): U32 => 0\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (c1_union: C1[(Both val | A)],\n"
    "     c1_isect: C1[(Named val & Aged val)])";

  TEST_COMPILE(src);

  // Union arm Both val is a subtype of the intersection (Named & Aged),
  // so a reification A = Both val makes the compound arms line up.
  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_union"), type_of("c1_isect"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_isect"), type_of("c1_union"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgMixedCompoundArmsReject)
{
  // Mixed-kind compound arms with no arm-vs-arm pair that could unify.
  // The typeparam `A` is constrained to I32, so it can't reify to
  // anything inhabiting `(Named & Aged)`, and neither primitive
  // provides those traits — no arm-vs-arm pair accepts.
  const char* src =
    "primitive P\n"
    "primitive Q\n"

    "trait val Named\n"
    "  fun name(): String\n"
    "trait val Aged\n"
    "  fun age(): U32\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: I32]\n"
    "    (c1_union: C1[(P | Q | A)],\n"
    "     c1_isect: C1[(Named val & Aged val)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_union"), type_of("c1_isect"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_isect"), type_of("c1_union"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgIsectVsIsect)
{
  // Same-kind intersection distribution. Every arm on each side must
  // have a matching arm on the other. `(Named & Aged & A)` vs
  // `(Named & Aged & Both val)`: Both val is a subtype of Named and of
  // Aged, and A can reify to a type that is a subtype of Both. Accept.
  const char* src =
    "trait val Named\n"
    "  fun name(): String\n"
    "trait val Aged\n"
    "  fun age(): U32\n"

    "class val Both is (Named & Aged)\n"
    "  fun name(): String => \"x\"\n"
    "  fun age(): U32 => 0\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Named val]\n"
    "    (c1_left: C1[(Named val & Aged val & A)],\n"
    "     c1_right: C1[(Named val & Aged val & Both val)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_left"), type_of("c1_right"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_right"), type_of("c1_left"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgIsectVsNominal)
{
  // Intersection on one side, nominal on the other. Every arm of the
  // intersection must be able to unify with the nominal — an
  // over-approximation of "the meet of the arms must equal the
  // nominal." Here `(Named & A)` where `A: Aged val`: Both val is a
  // subtype of Named, and A can reify to Both val (which is Aged).
  const char* src =
    "trait val Named\n"
    "  fun name(): String\n"
    "trait val Aged\n"
    "  fun age(): U32\n"

    "class val Both is (Named & Aged)\n"
    "  fun name(): String => \"x\"\n"
    "  fun age(): U32 => 0\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Aged val]\n"
    "    (c1_isect: C1[(Named val & A)], c1_both: C1[Both val])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_isect"), type_of("c1_both"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_both"), type_of("c1_isect"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgIsectVsNominalReject)
{
  // Intersection-vs-non-compound reject: the nominal isn't a subtype of
  // every arm of the intersection. `(Named & String val)` has no
  // inhabitant that is both Named and String, so it can't reify to
  // `Both val`. String val is not a supertype of Both val, so the arm
  // check fails.
  const char* src =
    "trait val Named\n"
    "  fun name(): String\n"
    "trait val Aged\n"
    "  fun age(): U32\n"

    "class val Both is (Named & Aged)\n"
    "  fun name(): String => \"x\"\n"
    "  fun age(): U32 => 0\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (c1_isect: C1[(Named val & String val & A)],\n"
    "     c1_both: C1[Both val])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_isect"), type_of("c1_both"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgUnionArmMissingArmReject)
{
  // The same-kind branch's second loop covers a specific case: the
  // operand side has an arm no pattern arm can supply, so even after
  // the first loop passes (every operand arm found a match), the
  // reverse check fails. Here operand arm `String val` has no pattern
  // arm to line up with (pattern's `A: I32` can't reify to String, and
  // U8 is not String).
  const char* src =
    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[Astr: String val, Ai: I32]\n"
    "    (c1_left: C1[(Astr | U8)],\n"
    "     c1_right: C1[(Ai | U8)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_left"), type_of("c1_right"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTuple)
{
  // Tuple type argument containing a type parameter: same-arity
  // element-pair recursion. Reifying A to U8 makes the two tuple
  // arguments equal, so the two Cell descriptors coincide at runtime —
  // accept.
  const char* src =
    "primitive P\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (c1_pa: C1[(P, A)], c1_pu8: C1[(P, U8)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_pa"), type_of("c1_pu8"), NULL, &opt));

  ASSERT_EQ(MATCHTYPE_ACCEPT,
    is_matchtype(type_of("c1_pu8"), type_of("c1_pa"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTupleDisjointElement)
{
  // Tuple element pair rejects: `A: I32` can't reify to `String`, so
  // the second element pair can never equal.
  const char* src =
    "primitive P\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: I32]\n"
    "    (c1_pa: C1[(P, A)], c1_pstring: C1[(P, String val)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_pa"), type_of("c1_pstring"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTupleArityMismatch)
{
  // Tuples of different arities can never equal at runtime; the arity
  // is part of the tuple type's descriptor.
  const char* src =
    "primitive P\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (c1_pair: C1[(P, A)], c1_triple: C1[(P, A, U8)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_pair"), type_of("c1_triple"), NULL, &opt));
}


TEST_F(MatchTypeTest, TypeParamInTypeArgTupleOccurs)
{
  // Occurs check propagates through tuple elements: `A` vs `(P, Wrap[A])`
  // — A appears wrapped in the tuple element, so no reification
  // satisfies A = (P, Wrap[A]) without an infinite type.
  const char* src =
    "primitive P\n"

    "class val Wrap[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "class val C1[A: Any #share]\n"
    "  let value: A\n"
    "  new val create(v: A) => value = v\n"

    "interface Test\n"
    "  fun z[A: Any #share]\n"
    "    (c1_a: C1[A], c1_tuple_wrap_a: C1[(P, Wrap[A] val)])";

  TEST_COMPILE(src);

  ASSERT_EQ(MATCHTYPE_REJECT,
    is_matchtype(type_of("c1_a"), type_of("c1_tuple_wrap_a"), NULL, &opt));
}
