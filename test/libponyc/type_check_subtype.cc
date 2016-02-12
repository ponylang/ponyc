#include <gtest/gtest.h>
#include <platform.h>
#include <type/subtype.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

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

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("t1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("t2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("t3"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("t1"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("t2"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("t3"), NULL));
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

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("i1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("i2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("i3"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("i1"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("i2"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("i3"), NULL));
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

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("c1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("c2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c2"), type_of("c1"), NULL));
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

  ASSERT_TRUE(is_subtype(type_of("t1"), type_of("t1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t3"), NULL));
  ASSERT_TRUE(is_subtype(type_of("t3"), type_of("t1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1b"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1b"), type_of("t1"), NULL));
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

  ASSERT_TRUE(is_subtype(type_of("i1"), type_of("i1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("i1"), type_of("i3"), NULL));
  ASSERT_TRUE(is_subtype(type_of("i3"), type_of("i1"), NULL));
  ASSERT_TRUE(is_subtype(type_of("i1"), type_of("i1b"), NULL));
  ASSERT_TRUE(is_subtype(type_of("i1b"), type_of("i1"), NULL));
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

  ASSERT_TRUE(is_subtype(type_of("t1"), type_of("i1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("i2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("i1"), type_of("t1"), NULL));
}


TEST_F(SubTypeTest, IsSubTypePrimitivePrimitive)
{
  const char* src =
    "primitive P1\n"
    "primitive P2\n"

    "interface Test\n"
    "  fun z(p1: P1, p2: P2)";

  TEST_COMPILE(src);

  ASSERT_TRUE(is_subtype(type_of("p1"), type_of("p1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("p1"), type_of("p2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("p2"), type_of("p1"), NULL));
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

  ASSERT_FALSE(is_subtype(type_of("p1"), type_of("t1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("p1"), type_of("t2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("p2"), type_of("t1"), NULL));
  ASSERT_TRUE(is_subtype(type_of("p2"), type_of("t2"), NULL));
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

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("c1or2"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c2"), type_of("c1or2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c3"), type_of("c1or2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1or2"), type_of("c1"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c1or2"), type_of("c1or2"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("t1or2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1or2"), type_of("c1"), NULL));
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
    "    t3trn: T3 trn,\n"
    "    t1val: T1 val,\n"
    "    t1and2val: (T1 ref & T2 val))";

  TEST_COMPILE(src);

  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1and2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("t1and2"), NULL));
  ASSERT_TRUE(is_subtype(type_of("t3"), type_of("t1and2"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c3"), type_of("t1and2"), NULL));
  ASSERT_TRUE(is_subtype(type_of("t1and2"), type_of("t1and2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1and2"), type_of("c3"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1val"), type_of("t1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1val"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1and2val"), NULL));
  ASSERT_TRUE(is_subtype(type_of("t3trn"), type_of("t1and2val"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1val"), type_of("t1and2val"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1and2val"), type_of("t1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1and2val"), type_of("t1val"), NULL));
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

  // TODO: Fix this, intersect of non-independent and combinable types
  //ASSERT_TRUE(is_subtype(type_of("i1and2"), type_of("i3")));
  ASSERT_TRUE(is_subtype(type_of("i3"), type_of("i1and2"), NULL));
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

  ASSERT_TRUE(is_subtype(type_of("c1"), type_of("iofu"), NULL));
  // TODO: Fix this, intersect of non-independent and combinable types
  //ASSERT_TRUE(is_subtype(type_of("iofu"), type_of("c1")));
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

  ASSERT_FALSE(is_subtype(type_of("t1"), type_of("t1t1"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1t1"), type_of("t1"), NULL));
  ASSERT_TRUE(is_subtype(type_of("c1c2"), type_of("t1t2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1c2"), type_of("t2t1"), NULL));
  ASSERT_TRUE(is_subtype(type_of("t1t2"), type_of("t1t2"), NULL));
  ASSERT_FALSE(is_subtype(type_of("t1t2"), type_of("c1c2"), NULL));
}


TEST_F(SubTypeTest, IsSubTypeUnionOfTuple)
{
  const char* src =
    "class C1\n"
    "class C2\n"

    "interface Test\n"
    "  fun z(c1: C1, c2: C2,\n"
    "    uoft: ((C1, C1) | (C1, C2) | (C2, C1) | (C2, C2)),\n"
    "    tofu: ((C1 | C2), (C1 | C2)))";

  TEST_COMPILE(src);

  ASSERT_FALSE(is_subtype(type_of("c1"), type_of("tofu"), NULL));
  ASSERT_TRUE(is_subtype(type_of("uoft"), type_of("tofu"), NULL));

  // TODO: enable this test
  // ASSERT_TRUE(is_subtype(type_of("tofu"), type_of("uoft"), NULL));
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

  ASSERT_TRUE (is_subtype(type_of("c1iso"), type_of("t1iso"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1ref"), type_of("t1iso"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1val"), type_of("t1iso"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1box"), type_of("t1iso"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1tag"), type_of("t1iso"), NULL));

  ASSERT_TRUE (is_subtype(type_of("c1iso"), type_of("t1ref"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1ref"), type_of("t1ref"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1val"), type_of("t1ref"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1box"), type_of("t1ref"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1tag"), type_of("t1ref"), NULL));

  ASSERT_TRUE (is_subtype(type_of("c1iso"), type_of("t1val"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1ref"), type_of("t1val"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1val"), type_of("t1val"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1box"), type_of("t1val"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1tag"), type_of("t1val"), NULL));

  ASSERT_TRUE (is_subtype(type_of("c1iso"), type_of("t1box"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1ref"), type_of("t1box"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1val"), type_of("t1box"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1box"), type_of("t1box"), NULL));
  ASSERT_FALSE(is_subtype(type_of("c1tag"), type_of("t1box"), NULL));

  ASSERT_TRUE (is_subtype(type_of("c1iso"), type_of("t1tag"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1ref"), type_of("t1tag"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1val"), type_of("t1tag"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1box"), type_of("t1tag"), NULL));
  ASSERT_TRUE (is_subtype(type_of("c1tag"), type_of("t1tag"), NULL));
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

  ASSERT_TRUE(is_eqtype(type_of("c1"), type_of("c1"), NULL));
  ASSERT_TRUE(is_eqtype(type_of("c2"), type_of("c2"), NULL));
  ASSERT_TRUE(is_eqtype(type_of("i1"), type_of("i1"), NULL));
  ASSERT_TRUE(is_eqtype(type_of("i2"), type_of("i2"), NULL));
  ASSERT_TRUE(is_eqtype(type_of("t1"), type_of("t1"), NULL));
  ASSERT_TRUE(is_eqtype(type_of("t2"), type_of("t2"), NULL));

  ASSERT_FALSE(is_eqtype(type_of("c1"), type_of("c2"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c2"), type_of("c1"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("i1"), type_of("i2"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("i2"), type_of("i1"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("t1"), type_of("t2"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("t2"), type_of("t1"), NULL));

  ASSERT_FALSE(is_eqtype(type_of("c1"), type_of("i1"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1"), type_of("t1"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("i1"), type_of("c1"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("i1"), type_of("t1"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("t1"), type_of("c1"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("t1"), type_of("i1"), NULL));

  ASSERT_TRUE(is_eqtype(type_of("i1"), type_of("i1b"), NULL));
}


TEST_F(SubTypeTest, IsEqTypeCap)
{
  const char* src =
    "class C1\n"

    "interface Test\n"
    "  fun z(c1iso: C1 iso, c1ref: C1 ref, c1val: C1 val,\n"
    "    c1box: C1 box, c1tag: C1 tag)";

  TEST_COMPILE(src);

  ASSERT_TRUE (is_eqtype(type_of("c1iso"), type_of("c1iso"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1ref"), type_of("c1iso"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1val"), type_of("c1iso"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1box"), type_of("c1iso"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1tag"), type_of("c1iso"), NULL));

  ASSERT_FALSE(is_eqtype(type_of("c1iso"), type_of("c1ref"), NULL));
  ASSERT_TRUE (is_eqtype(type_of("c1ref"), type_of("c1ref"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1val"), type_of("c1ref"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1box"), type_of("c1ref"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1tag"), type_of("c1ref"), NULL));

  ASSERT_FALSE(is_eqtype(type_of("c1iso"), type_of("c1val"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1ref"), type_of("c1val"), NULL));
  ASSERT_TRUE (is_eqtype(type_of("c1val"), type_of("c1val"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1box"), type_of("c1val"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1tag"), type_of("c1val"), NULL));

  ASSERT_FALSE(is_eqtype(type_of("c1iso"), type_of("c1box"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1ref"), type_of("c1box"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1val"), type_of("c1box"), NULL));
  ASSERT_TRUE (is_eqtype(type_of("c1box"), type_of("c1box"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1tag"), type_of("c1box"), NULL));

  ASSERT_FALSE(is_eqtype(type_of("c1iso"), type_of("c1tag"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1ref"), type_of("c1tag"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1val"), type_of("c1tag"), NULL));
  ASSERT_FALSE(is_eqtype(type_of("c1box"), type_of("c1tag"), NULL));
  ASSERT_TRUE (is_eqtype(type_of("c1tag"), type_of("c1tag"), NULL));
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
    "    t1ornone: (T1 | None), t1andnone: (T1 & None),\n"
    "    c1ort1: (C1 | T1), c1andt1: (C1 & T1))";

  TEST_COMPILE(src);

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

  ASSERT_TRUE(is_subtype(type_of("a"), type_of("a"), NULL));
  ASSERT_FALSE(is_subtype(type_of("a"), type_of("this_a"), NULL));
  ASSERT_FALSE(is_subtype(type_of("a"), type_of("b_a"), NULL));
  ASSERT_FALSE(is_subtype(type_of("a"), type_of("b_b_a"), NULL));

  ASSERT_FALSE(is_subtype(type_of("this_a"), type_of("a"), NULL));
  ASSERT_TRUE(is_subtype(type_of("this_a"), type_of("this_a"), NULL));
  ASSERT_FALSE(is_subtype(type_of("this_a"), type_of("b_a"), NULL));
  ASSERT_FALSE(is_subtype(type_of("this_a"), type_of("b_b_a"), NULL));

  ASSERT_FALSE(is_subtype(type_of("b_a"), type_of("a"), NULL));
  ASSERT_FALSE(is_subtype(type_of("b_a"), type_of("this_a"), NULL));
  ASSERT_TRUE(is_subtype(type_of("b_a"), type_of("b_a"), NULL));
  ASSERT_TRUE(is_subtype(type_of("b_a"), type_of("b_b_a"), NULL));

  ASSERT_FALSE(is_subtype(type_of("b_b_a"), type_of("a"), NULL));
  ASSERT_FALSE(is_subtype(type_of("b_b_a"), type_of("this_a"), NULL));
  ASSERT_TRUE(is_subtype(type_of("b_b_a"), type_of("b_a"), NULL));
  ASSERT_TRUE(is_subtype(type_of("b_b_a"), type_of("b_b_a"), NULL));

  ASSERT_TRUE(is_subtype(type_of("val_a"), type_of("box_a"), NULL));
}
