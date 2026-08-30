#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "verify"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "verify", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "verify", errs)); }

#define TEST_ERRORS_3(src, err1, err2, err3) \
  { const char* errs[] = {err1, err2, err3, NULL}; \
    DO(test_expected_errors(src, "verify", errs)); }

class ObjectTest : public PassTest
{};

TEST_F(ObjectTest, ObjectProvidesClass)
{
   const char* src =
    "class X1\n"
    "\n"
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    let obj = object is X1 end\n";
  TEST_ERRORS_1(src, "invalid provides type. Can only be interfaces, traits and intersects of those.");
}

TEST_F(ObjectTest, ClassProvidesUnionOfInterfaces)
{
  const char* src =
    "interface I1\n"
    "  fun i1()\n"
    "interface I2\n"
    "  fun i2()\n"
    "\n"
    "class X1 is (I1 | I2)\n"
    "\n"
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    None\n";
  TEST_ERRORS_1(src, "invalid provides type. Can only be interfaces, traits and intersects of those.");
}

TEST_F(ObjectTest, ObjectProvidesUnionInterfaces)
{
  const char* src =
    "interface I1\n"
    "  fun i1()\n"
    "interface I2\n"
    "  fun i2()\n"
    "\n"
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    let obj = object is (I1 | I2) end";
  TEST_ERRORS_1(src, "invalid provides type. Can only be interfaces, traits and intersects of those.");
}

TEST_F(ObjectTest, ClassProvidesTypeAliasUnionInterfaces)
{
  const char* src =
    "interface I1\n"
    "interface I2\n"
    "type T1000 is (I1 | I2)\n"
    "\n"
    "class X1 is T1000\n"
    "\n"
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    None";
  TEST_ERRORS_1(src, "invalid provides type. Can only be interfaces, traits and intersects of those.");
}

TEST_F(ObjectTest, ObjectProvidesTypeAliasUnionInterfaces)
{
  const char* src =
    "interface I1\n"
    "  fun i1()\n"
    "interface I2\n"
    "  fun i2()\n"
    "type T1000 is (I1 | I2)\n"
    "\n"
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    object is T1000 end";
  TEST_ERRORS_1(src, "invalid provides type. Can only be interfaces, traits and intersects of those.");
}

TEST_F(ObjectTest, ObjectProvidesTypeAliasUnionTraits)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "type T3 is (T1 | T2)\n"
    "\n"
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    object is T3 end";
  TEST_ERRORS_1(src, "invalid provides type. Can only be interfaces, traits and intersects of those.");
}


TEST_F(ObjectTest, ObjectProvidesNestedUnionTypeAliasProvides)
{
  const char* src =
    "interface I1\n"
    "  fun i1()\n"
    "interface I2\n"
    "  fun i2()\n"
    "trait TR1\n"
    "type T1 is (I1 | I2)\n"
    "\n"
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    let obj = object is (TR1 & T1)end";
  TEST_ERRORS_1(src, "invalid provides type. Can only be interfaces, traits and intersects of those.");
}

TEST_F(ObjectTest, ObjectProvidesEnclosingTrait)
{
  // From issue #4451: object literal inside a trait method that implements
  // the same trait should compile without crashing.
  const char* src =
    "trait Printer\n"
    "  fun double(): Printer =>\n"
    "    object ref is Printer\n"
    "      fun apply() => None\n"
    "    end\n"
    "  fun apply() => None\n";
  TEST_COMPILE(src);
}

TEST_F(ObjectTest, ObjectProvidesEnclosingTraitWithTypeParams)
{
  // Variant of issue #4451 with type parameters on the trait.
  const char* src =
    "trait Printer[A]\n"
    "  fun double(): Printer[A] =>\n"
    "    object ref is Printer[A]\n"
    "      fun apply() => None\n"
    "    end\n"
    "  fun apply() => None\n";
  TEST_COMPILE(src);
}

TEST_F(ObjectTest, ObjectInMethodWithUnionConstraint)
{
  // From issue #2924: an object literal inside a method whose type parameter
  // has a union constraint with mixed capabilities should compile.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun bar[F: (U32 val | String ref)]() =>\n"
    "    object val end\n";
  TEST_COMPILE(src);
}

TEST_F(ObjectTest, ObjectInMethodWithUnionConstraintShare)
{
  // Union of val and tag derives #share.
  const char* src =
    "actor Tag1\n"
    "  new create() => None\n"
    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun bar[F: (U32 val | Tag1 tag)]() =>\n"
    "    object val end\n";
  TEST_COMPILE(src);
}

TEST_F(ObjectTest, ObjectInMethodWithUnionConstraintSend)
{
  // Union of iso and tag derives #send.
  const char* src =
    "class iso Iso1\n"
    "  new iso create() => None\n"
    "actor Tag2\n"
    "  new create() => None\n"
    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun bar[F: (Iso1 iso | Tag2 tag)]() =>\n"
    "    object val end\n";
  TEST_COMPILE(src);
}

TEST_F(ObjectTest, ObjectInMethodWithUnionConstraintAlias)
{
  // Union of ref and tag derives #alias.
  const char* src =
    "actor Tag3\n"
    "  new create() => None\n"
    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun bar[F: (String ref | Tag3 tag)]() =>\n"
    "    object val end\n";
  TEST_COMPILE(src);
}

TEST_F(ObjectTest, ObjectInMethodWithUnionConstraintAndIftype)
{
  // From issue #2924: iftype inside an object literal that captures type
  // parameters from a method with a union constraint.
  const char* src =
    "interface box FnBox[A, B]\n"
    "  fun apply(a: A): B ?\n"
    "interface ref FnRef[A, B]\n"
    "  fun ref apply(a: A): B ?\n"
    "type Fn[A, B] is (FnBox[A, B] box | FnRef[A, B] ref)\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None\n"
    "  fun bar[A, B, F: Fn[A, B]](f: F) =>\n"
    "    object ref\n"
    "      fun ref foo(a: A) ? =>\n"
    "        iftype F <: FnBox[A, B] box then f(consume a)?\n"
    "        elseif F <: FnRef[A, B] ref then f(consume a)?\n"
    "        else error\n"
    "        end\n"
    "    end\n";
  TEST_COMPILE(src);
}

