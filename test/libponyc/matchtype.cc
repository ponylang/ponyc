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
