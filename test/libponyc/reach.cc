#include <gtest/gtest.h>
#include <platform.h>

#include <reach/reach.h>
#include <type/subtype.h>

#include "util.h"

#include <memory>


#define TEST_COMPILE(src, pass) DO(test_compile(src, pass))


class ReachTest : public PassTest
{};


TEST_F(ReachTest, IsectHasSubtypes)
{
  const char* src =
    "trait TA\n"
    "  fun a()\n"

    "trait TB\n"
    "  fun b()\n"

    "actor Main is (TA & TB)\n"
    "  new create(env: Env) =>\n"
    "    let ab: (TA & TB) = this\n"
    "    ab.a()\n"
    "    ab.b()\n"

    "  fun a() => None\n"
    "  fun b() => None";

  TEST_COMPILE(src, "reach");

  ast_t* ab_ast = type_of("ab");
  ASSERT_NE(ab_ast, (void*)NULL);

  reach_t* reach = compile->reach;
  reach_type_t* ab_reach = reach_type(reach, ab_ast);
  ASSERT_NE(ab_reach, (void*)NULL);

  size_t i = HASHMAP_BEGIN;
  reach_type_t* subtype;

  bool found = false;
  while((subtype = reach_type_cache_next(&ab_reach->subtypes, &i)) != NULL)
  {
    if(subtype->name == stringtab("Main"))
    {
      found = true;
      break;
    }
  }

  ASSERT_TRUE(found);
}

// A generic function that instantiates itself with an ever-deeper type
// argument (Bar.apply[A] calls Bar.apply[Pair[A]]) requires an unbounded number
// of reachable types/methods. Reachability used to chase this until it ran out
// of memory; it must now stop at the instantiation-depth limit and report an
// error rather than diverging. With the limit in place the compile aborts at a
// bounded depth — fast — so this test is safe to run in CI.
TEST_F(ReachTest, InfinitelyRecursiveGenericInstantiation)
{
  const char* src =
    "interface IFoo\n"
    "  new create()\n"
    "  fun depth(): USize\n"

    "class Foo is IFoo\n"
    "  new create() => None\n"
    "  fun depth(): USize => 0\n"

    "class Pair[A: IFoo] is IFoo\n"
    "  new create() => None\n"
    "  fun depth(): USize => A.depth() + 1\n"

    "primitive Bar\n"
    "  fun apply[A: IFoo](n: USize): IFoo =>\n"
    "    if n == 0 then\n"
    "      A\n"
    "    else\n"
    "      Bar.apply[Pair[A]](n - 1)\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(Bar.apply[Foo](0).depth().string())";

  // Needs the real builtin: the recursion runs through USize arithmetic, and
  // the divergence only happens once reachability traces the method bodies.
  set_builtin(nullptr);

  // Assert the depth-specific message: this linear shape trips the depth limit.
  // Asserting the distinguishing text (not the shared tail) means a regression
  // that broke only the depth check would fail here instead of being masked by
  // the size check catching the same runaway.
  const char* errs[] = {"type instantiation depth", NULL};
  DO(test_expected_errors(src, "reach", errs));
}

// A generic type whose field is an ever-larger instantiation of itself
// (Wrap[A] has a field of type Wrap[Wrap[A]]) makes an unbounded number of
// types reachable through field expansion alone — no recursive method call
// needed. This diverges synchronously inside add_type rather than through the
// method worklist, exercising the add_nominal depth check (the error points at
// the type definition, not a method). It too must abort at the depth limit.
TEST_F(ReachTest, InfinitelyRecursiveGenericType)
{
  const char* src =
    "class Wrap[A]\n"
    "  let next: (Wrap[Wrap[A]] | None) = None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Wrap[U8]";

  const char* errs[] = {"type instantiation depth", NULL};
  DO(test_expected_errors(src, "reach", errs));
}

// A generic type whose fields fan out into several distinct, ever-larger
// instantiations of itself (Wrap[A] has fields of type Wrap[L[A]] and
// Wrap[R[A]]) makes an exponential number of types reachable. The depth check
// trips on the first over-deep chain, but detection must also HALT the
// remaining sibling branches — otherwise every branch that hasn't yet reached
// the limit keeps expanding, an exponential blow-up that still exhausts memory.
// This guards that the limit flag stops all type-creation recursion, not just
// the chain that tripped it.
TEST_F(ReachTest, InfinitelyRecursiveGenericTypeFanOut)
{
  const char* src =
    "class L[A]\n"
    "class R[A]\n"
    "class Wrap[A]\n"
    "  let a: (Wrap[L[A]] | None) = None\n"
    "  let b: (Wrap[R[A]] | None) = None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Wrap[U8]";

  const char* errs[] = {"type instantiation depth", NULL};
  DO(test_expected_errors(src, "reach", errs));
}

// A generic function whose type argument *doubles* in size each step
// (Bar.apply[A] calls Bar.apply[(A, A)]) grows the type's node count
// exponentially while its nesting depth grows only linearly. A nesting-depth
// limit would let this consume ~2^depth memory before tripping; the node-count
// (size) limit catches it while the type is still small. This is the shape that
// a depth-based check could not catch.
TEST_F(ReachTest, InfinitelyRecursiveGenericTypeArgumentDoubles)
{
  const char* src =
    "primitive Bar\n"
    "  fun apply[A: Any val](n: USize): None =>\n"
    "    if n == 0 then None else Bar.apply[(A, A)](n - 1) end\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.apply[U8](4)";

  set_builtin(nullptr);

  // Assert the size-specific message: this branching shape trips the size limit
  // (not depth), which is the case a depth-only check could not catch.
  const char* errs[] = {"grew too large", NULL};
  DO(test_expected_errors(src, "reach", errs));
}

// A recursive type alias is finite and legal — `Tree` is `None` or an array of
// `Tree`, which bottoms out. reach_type_size/reach_type_depth deliberately do
// not unfold aliases, so this must still compile cleanly through reachability
// rather than tripping a limit. Guards that the checks don't false-positive on
// legitimate recursive aliases.
TEST_F(ReachTest, RecursiveTypeAliasCompiles)
{
  const char* src =
    "use \"collections\"\n"
    "type Tree is (None | Array[Tree])\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let t: Tree = None\n"
    "    env.out.print(\"ok\")";

  set_builtin(nullptr);

  TEST_COMPILE(src, "reach");
}

struct reach_deleter
{
  void operator()(reach_t* r)
  {
    reach_free(r);
  }
};

TEST_F(ReachTest, Determinism)
{
  const char* src =
    "interface I1\n"
    "  fun f1()\n"
    "interface I2\n"
    "  fun f2()\n"
    "interface I3\n"
    "  fun f3()\n"
    "interface I4\n"
    "  fun f4()\n"

    "class C1 is (I1 & I2)\n"
    "  fun f1() => None\n"
    "  fun f2() => None\n"
    "class C2 is (I2 & I3)\n"
    "  fun f2() => None\n"
    "  fun f3() => None\n"
    "class C3 is (I3 & I4)\n"
    "  fun f3() => None\n"
    "  fun f4() => None\n"
    "class C4 is (I4 & I1)\n"
    "  fun f4() => None\n"
    "  fun f1() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(\"\")\n"

    "    U8(0) ; I8(0) ; U16(0) ; I16(0) ; U32(0) ; I32(0)\n"

    "    (C1, C2)\n"
    "    (C2, C3)\n"
    "    (C3, C4)\n"
    "    (C4, C1)\n"

    "    let i1: I1 = C1\n"
    "    let i2: I2 = C2\n"
    "    let i3: I3 = C3\n"
    "    let i4: I4 = C4\n"

    "    i1.f1()\n"
    "    i2.f2()\n"
    "    i3.f3()\n"
    "    i4.f4()";

  set_builtin(nullptr);

  TEST_COMPILE(src, "paint");

  std::unique_ptr<reach_t, reach_deleter> reach_guard{compile->reach};
  compile->reach = nullptr;

  TEST_COMPILE(src, "paint");

  reach_t* r1 = reach_guard.get();
  reach_t* r2 = compile->reach;

  ASSERT_EQ(r1->object_type_count, r2->object_type_count);
  ASSERT_EQ(r1->numeric_type_count, r2->numeric_type_count);
  ASSERT_EQ(r1->tuple_type_count, r2->tuple_type_count);
  ASSERT_EQ(r1->total_type_count, r2->total_type_count);
  ASSERT_EQ(r1->trait_type_count, r2->trait_type_count);

  reach_type_t* t1;
  size_t i = HASHMAP_BEGIN;

  while((t1 = reach_types_next(&r1->types, &i)) != nullptr)
  {
    size_t j = HASHMAP_UNKNOWN;
    reach_type_t* t2 = reach_types_get(&r2->types, t1, &j);

    ASSERT_NE(t2, nullptr);
    ASSERT_EQ(t1->type_id, t2->type_id);

    reach_method_name_t* n1;
    j = HASHMAP_BEGIN;

    while((n1 = reach_method_names_next(&t1->methods, &j)) != nullptr)
    {
      size_t k = HASHMAP_UNKNOWN;
      reach_method_name_t* n2 = reach_method_names_get(&t2->methods, n1, &j);

      ASSERT_NE(n2, nullptr);

      reach_method_t* m1;
      k = HASHMAP_BEGIN;

      while((m1 = reach_mangled_next(&n1->r_mangled, &k)) != nullptr)
      {
        size_t l = HASHMAP_UNKNOWN;
        reach_method_t* m2 = reach_mangled_get(&n2->r_mangled, m1, &l);

        ASSERT_NE(m2, nullptr);
        ASSERT_EQ(m1->vtable_index, m2->vtable_index);
      }
    }
  }
}

TEST_F(ReachTest, UnionReachedMethod)
{
  const char* src =
    "trait val TA\n"
    "  fun example()\n"

    "trait val TB\n"
    "  fun example()\n"

    "primitive P is TA"
    "  fun example() => None\n"

    "primitive Indirection\n"
    "  fun apply(): (TA | TB) => P\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Indirection().example()\n"
    "    let p = P";

  TEST_COMPILE(src, "reach");

  ast_t* p_ast = type_of("p");
  ASSERT_NE(p_ast, (void*)NULL);

  reach_t* reach = compile->reach;
  reach_type_t* p_reach = reach_type(reach, p_ast);
  ASSERT_NE(p_reach, (void*)NULL);

  bool found = false;
  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;
  while((n = reach_method_names_next(&p_reach->methods, &i)) != NULL)
  {
    if(n->name == stringtab("example"))
    {
      found = true;
      break;
    }
  }

  ASSERT_TRUE(found);
}
