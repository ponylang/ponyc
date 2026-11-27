#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/id.h>

#include <string>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

// How many classes object literals and lambdas make: a literal written in a
// trait makes one class in the trait's own check, which the types that
// inherit the method share when the literal captures nothing and its types
// come from the trait's text; otherwise each inheriting type's copy makes its
// own. "C's first-level class" is the class C's copy of a trait method makes
// for the trait's literal.
class InheritedLiteralTest : public PassTest
{
protected:
  virtual void SetUp()
  {
    PassTest::SetUp();
    set_builtin(NULL);
  }

  // The anonymous classes (not lambda-type interfaces) in the test package.
  size_t anonymous_classes()
  {
    size_t count = 0;

    for(ast_t* f = ast_child(module); f != NULL; f = ast_sibling(f))
    {
      for(ast_t* e = ast_child(f); e != NULL; e = ast_sibling(e))
      {
        switch(ast_id(e))
        {
          case TK_CLASS:
          case TK_PRIMITIVE:
          case TK_ACTOR:
            if(ast_name(ast_child(e))[0] == '$')
              count++;
            break;

          default: {}
        }
      }
    }

    return count;
  }

  // A chain of `depth` traits, each with two methods whose object literals
  // provide the next trait and capture the receiver, and a class that
  // inherits from the first.
  std::string trait_chain(size_t depth)
  {
    std::string src;

    for(size_t i = 0; i < depth; i++)
    {
      std::string t = "T" + std::to_string(i);
      std::string next = "T" + std::to_string(i + 1);
      src += "trait " + t + "\n";
      src += "  fun name(): String => \"" + t + "\"\n";

      for(const char* m : {"a", "b"})
      {
        src += std::string("  fun val f") + m + "(): " + next + " val =>\n";
        src += "    let me = this\n";
        src += "    object val is " + next + "\n";
        src += std::string("      fun name(): String => \"") + m +
          "\" + me.name()\n";
        src += "    end\n";
      }
    }

    src += "trait T" + std::to_string(depth) + "\n";
    src += "  fun name(): String => \"end\"\n";
    src += "class val C is T0\n";
    src += "  new val create() => None\n";
    src += "actor Main\n";
    src += "  new create(env: Env) =>\n";
    src += "    env.out.print(C.fa().fb().name())\n";
    return src;
  }
};

TEST_F(InheritedLiteralTest, TraitChainOfFour)
{
  // Two literals in each of four traits, and C's two first-level classes,
  // not one class per path through the chain.
  std::string src = trait_chain(4);
  TEST_COMPILE(src.c_str());
  ASSERT_EQ(10U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, TraitChainOfSixteen)
{
  // As TraitChainOfFour, with sixteen traits: 32 trait classes and C's two.
  std::string src = trait_chain(16);
  TEST_COMPILE(src.c_str());
  ASSERT_EQ(34U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, SelfReferenceSharesTraitClass)
{
  // C's first-level class is the trait's: no type arguments, no captures.
  const char* src =
    "trait T\n"
    "  fun name(): String => \"T\"\n"
    "  fun m(): T val => object val is T end\n"
    "class C is T\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = C.m()\n"
    "    let b = a.m()\n"
    "    let c = b.m()\n"
    "    env.out.print((a is b).string() + \" \" + (b is c).string())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(1U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, TwoHostsShareTraitClass)
{
  // C1 and C2 share the trait's class.
  const char* src =
    "trait T\n"
    "  fun n(): String => \"n\"\n"
    "  fun m(): T val => object val is T end\n"
    "class C1 is T\n"
    "  new create() => None\n"
    "class C2 is T\n"
    "  new create() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print((C1.m() is C2.m()).string() + \" \" + (C1.m() is C1.m()).string() + \" \" + (C1.m().m() is C2.m().m()).string() + \" \" + (C1.m() is C1.m().m()).string())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(1U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, SelfCaptureFirstLevel)
{
  // The trait's class (me: T val) and C's first-level class (me: C val).
  const char* src =
    "trait T\n"
    "  fun name(): String => \"T\"\n"
    "  fun val m(): T val =>\n"
    "    let me = this\n"
    "    object val is T\n"
    "      fun name(): String => \"inner of \" + me.name()\n"
    "    end\n"
    "primitive C is T\n"
    "  fun name(): String => \"C\"\n"
    "actor Main\n"
    "  new create(env: Env) => env.out.print(C.m().name()); env.out.print(C.m().m().name())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(2U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, ArrayCaptureFirstLevel)
{
  // The trait classes for T0.fa and T1.fa, and C's first-level class
  // (me: Array[C val] val).
  const char* src =
    "trait T0\n"
    "  fun name(): String => \"t0\"\n"
    "  fun val fa(): T1 val =>\n"
    "    let me = recover val [this] end\n"
    "    object val is T1\n"
    "      fun name(): String => \"a\" + try me(0)?.name() else \"\" end\n"
    "    end\n"
    "trait T1\n"
    "  fun name(): String => \"t1\"\n"
    "  fun val fa(): T2 val =>\n"
    "    let me = recover val [this] end\n"
    "    object val is T2\n"
    "      fun name(): String => \"a\" + try me(0)?.name() else \"\" end\n"
    "    end\n"
    "trait T2\n"
    "  fun name(): String => \"t2\"\n"
    "class val C is T0\n"
    "  new val create() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(C.fa().fa().name())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(3U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, SubTraitCopyBuildsOwnClass)
{
  // T0's class, S's own class for its copy of fa, and C's first-level class.
  const char* src =
    "trait T0\n"
    "  fun name(): String => \"t0\"\n"
    "  fun val fa(): T2 val =>\n"
    "    let me = recover val [this] end\n"
    "    object val is T2\n"
    "      fun name(): String => \"a\" + try me(0)?.name() else \"\" end\n"
    "    end\n"
    "trait S is T0\n"
    "  fun name(): String => \"s\"\n"
    "trait T2\n"
    "  fun name(): String => \"t2\"\n"
    "class val C is S\n"
    "  new val create() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(C.fa().name())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(3U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, ThreeLiteralsThreeFirstLevel)
{
  // Three trait classes and C's three first-level classes.
  const char* src =
    "trait P\n"
    "  fun name(): String => \"p\"\n"
    "  fun val c0(): P val =>\n"
    "    let me = this\n"
    "    object val is P\n"
    "      fun name(): String => \"c0(\" + me.name() + \")\"\n"
    "    end\n"
    "  fun val c1(): P val =>\n"
    "    let me = this\n"
    "    object val is P\n"
    "      fun name(): String => \"c1(\" + me.name() + \")\"\n"
    "    end\n"
    "  fun val c2(): P val =>\n"
    "    let me = this\n"
    "    object val is P\n"
    "      fun name(): String => \"c2(\" + me.name() + \")\"\n"
    "    end\n"
    "class val C is P\n"
    "  new val create() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(C.c0().c1().c0().name())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(6U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, CaptureFreeCopiesShareTraitClasses)
{
  // 32 trait literals and K's literal; K's copies have no captures and share
  // the trait classes.
  const char* src =
    "trait T16[A: Any val]\n"
    "  fun z(): String => \"z\"\n"
    "trait T15[A: Any val]\n"
    "  fun a15(): T16[Array[A] val] val => object val is T16[Array[A] val] end\n"
    "  fun b15(): T16[(A, U8)] val => object val is T16[(A, U8)] end\n"
    "trait T14[A: Any val]\n"
    "  fun a14(): T15[Array[A] val] val => object val is T15[Array[A] val] end\n"
    "  fun b14(): T15[(A, U8)] val => object val is T15[(A, U8)] end\n"
    "trait T13[A: Any val]\n"
    "  fun a13(): T14[Array[A] val] val => object val is T14[Array[A] val] end\n"
    "  fun b13(): T14[(A, U8)] val => object val is T14[(A, U8)] end\n"
    "trait T12[A: Any val]\n"
    "  fun a12(): T13[Array[A] val] val => object val is T13[Array[A] val] end\n"
    "  fun b12(): T13[(A, U8)] val => object val is T13[(A, U8)] end\n"
    "trait T11[A: Any val]\n"
    "  fun a11(): T12[Array[A] val] val => object val is T12[Array[A] val] end\n"
    "  fun b11(): T12[(A, U8)] val => object val is T12[(A, U8)] end\n"
    "trait T10[A: Any val]\n"
    "  fun a10(): T11[Array[A] val] val => object val is T11[Array[A] val] end\n"
    "  fun b10(): T11[(A, U8)] val => object val is T11[(A, U8)] end\n"
    "trait T9[A: Any val]\n"
    "  fun a9(): T10[Array[A] val] val => object val is T10[Array[A] val] end\n"
    "  fun b9(): T10[(A, U8)] val => object val is T10[(A, U8)] end\n"
    "trait T8[A: Any val]\n"
    "  fun a8(): T9[Array[A] val] val => object val is T9[Array[A] val] end\n"
    "  fun b8(): T9[(A, U8)] val => object val is T9[(A, U8)] end\n"
    "trait T7[A: Any val]\n"
    "  fun a7(): T8[Array[A] val] val => object val is T8[Array[A] val] end\n"
    "  fun b7(): T8[(A, U8)] val => object val is T8[(A, U8)] end\n"
    "trait T6[A: Any val]\n"
    "  fun a6(): T7[Array[A] val] val => object val is T7[Array[A] val] end\n"
    "  fun b6(): T7[(A, U8)] val => object val is T7[(A, U8)] end\n"
    "trait T5[A: Any val]\n"
    "  fun a5(): T6[Array[A] val] val => object val is T6[Array[A] val] end\n"
    "  fun b5(): T6[(A, U8)] val => object val is T6[(A, U8)] end\n"
    "trait T4[A: Any val]\n"
    "  fun a4(): T5[Array[A] val] val => object val is T5[Array[A] val] end\n"
    "  fun b4(): T5[(A, U8)] val => object val is T5[(A, U8)] end\n"
    "trait T3[A: Any val]\n"
    "  fun a3(): T4[Array[A] val] val => object val is T4[Array[A] val] end\n"
    "  fun b3(): T4[(A, U8)] val => object val is T4[(A, U8)] end\n"
    "trait T2[A: Any val]\n"
    "  fun a2(): T3[Array[A] val] val => object val is T3[Array[A] val] end\n"
    "  fun b2(): T3[(A, U8)] val => object val is T3[(A, U8)] end\n"
    "trait T1[A: Any val]\n"
    "  fun a1(): T2[Array[A] val] val => object val is T2[Array[A] val] end\n"
    "  fun b1(): T2[(A, U8)] val => object val is T2[(A, U8)] end\n"
    "trait T0[A: Any val]\n"
    "  fun a0(): T1[Array[A] val] val => object val is T1[Array[A] val] end\n"
    "  fun b0(): T1[(A, U8)] val => object val is T1[(A, U8)] end\n"
    "primitive K\n"
    "  fun mk(): T0[U8] val => object val is T0[U8] end\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(\"ok\")\n"
    "    K.mk().a0(); K.mk().b0()\n";

  TEST_COMPILE(src);
  ASSERT_EQ(33U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, FirstLevelSharesGenericTraitClass)
{
  // Two classes, for the literals in f1 and f2. The literal in C's copy of f1
  // uses f1's class at [U8], so it adds none.
  const char* src =
    "trait T1[A: Any val]\n"
    "  fun n(): String => \"T1\"\n"
    "  fun f1(): T2[A] val => object val is T2[A] end\n"
    "trait T2[A: Any val]\n"
    "  fun n(): String => \"T2\"\n"
    "  fun f2(): T1[A] val => object val is T1[A] end\n"
    "class val C is T1[U8]\n"
    "  new val create() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print((C.f1() is C.f1().f2().f1()).string() + \" \" + (C.f1().f2() is C.f1().f2().f1().f2()).string())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(2U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, LambdaWithoutTypeParameters)
{
  // The lambda mentions no type parameter, so its class takes none.
  const char* src =
    "primitive P\n"
    "  fun f[A: Any val](): {(): U8} val => {(): U8 => 1}\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = P.f[U8]()\n"
    "    let b = P.f[String]()\n"
    "    env.out.print((a is b).string())\n"
    "    env.out.print(((digestof a) == (digestof b)).string())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(1U, anonymous_classes());

  size_t checked = 0;

  for(ast_t* f = ast_child(module); f != NULL; f = ast_sibling(f))
  {
    for(ast_t* e = ast_child(f); e != NULL; e = ast_sibling(e))
    {
      token_id id = ast_id(e);

      if(((id == TK_CLASS) || (id == TK_PRIMITIVE) || (id == TK_ACTOR)) &&
        (ast_name(ast_child(e))[0] == '$'))
      {
        ASSERT_EQ(TK_NONE, ast_id(ast_childidx(e, 1)));
        checked++;
      }
    }
  }

  ASSERT_EQ(1U, checked);
}

TEST_F(InheritedLiteralTest, CaptureFreeLiteralOneClass)
{
  // One class for the capture-free literal. The literal in C's and D's copies
  // of m uses it, so they add none.
  const char* src =
    "trait T\n"
    "  fun val m(): T val => object val is T end\n"
    "class val C is T\n"
    "  new val create() => None\n"
    "class val D is T\n"
    "  new val create() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = C.m(); let b = a.m(); let d = b.m()\n"
    "    let e = D.m(); let f = e.m()\n"
    "    env.out.print((a is b).string() + \" \" + (b is d).string() + \" \" + (a is e).string() + \" \" + (b is f).string())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(1U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, SubTraitCopyWithCapture)
{
  // T0's class for mk, S's own class for its copy of mk (it captures
  // v: U64), and the class for M's capture-free literal, which is C's
  // first-level class.
  const char* src =
    "primitive Name\n"
    "  fun apply[X](x: X): String =>\n"
    "    iftype X <: U64 then \"u64\" else \"other\" end\n"
    "trait T0\n"
    "  fun get(): (U64 | None) => None\n"
    "  fun lab(): String => \"t0\"\n"
    "  fun mk(): T0 val =>\n"
    "    let v = get()\n"
    "    object val is T0\n"
    "      fun lab(): String => Name(v)\n"
    "    end\n"
    "trait S is T0\n"
    "  fun get(): U64 => 1\n"
    "  fun mk2(): T0 val => mk()\n"
    "trait M\n"
    "  fun mm(): S val => object val is S end\n"
    "class val C is M\n"
    "  new val create() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(C.mm().mk().lab() + \"|\" + C.mm().mk2().lab())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(3U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, TypedLambdaReturnShared)
{
  // One class for the lambda, shared by C1 and C2.
  const char* src =
    "trait T\n"
    "  fun g(): {(U64): U64} val => {(x) => x}\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print((C1.g() is C2.g()).string() + \" \" + (C1.g() is C1.g()).string())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(1U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, TypedLambdaLetShared)
{
  // One class for the lambda, shared by C1 and C2.
  const char* src =
    "trait T\n"
    "  fun g(): {(U64): U64} val =>\n"
    "    let f: {(U64): U64} val = {(x) => x}\n"
    "    f\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print((C1.g() is C2.g()).string() + \" \" + (C1.g() is C1.g()).string())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(1U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, TypedLambdaArrayShared)
{
  // One class for the lambda, shared by C1 and C2.
  const char* src =
    "trait T\n"
    "  fun g(): {(U64): U64} val ? =>\n"
    "    let a = [as {(U64): U64} val: {(x) => x}]\n"
    "    a(0)?\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try env.out.print((C1.g()? is C2.g()?).string() + \" \" + (C1.g()? is C1.g()?).string()) end\n";

  TEST_COMPILE(src);
  ASSERT_EQ(1U, anonymous_classes());
}

TEST_F(InheritedLiteralTest, TypedLambdaTupleShared)
{
  // One class for the lambda, shared by C1 and C2.
  const char* src =
    "trait T\n"
    "  fun g(): {(U64): U64} val =>\n"
    "    let t: ({(U64): U64} val, U8) = ({(x) => x}, 1)\n"
    "    t._1\n"
    "class C1 is T\n"
    "class C2 is T\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print((C1.g() is C2.g()).string() + \" \" + (C1.g() is C1.g()).string())\n";

  TEST_COMPILE(src);
  ASSERT_EQ(1U, anonymous_classes());
}
