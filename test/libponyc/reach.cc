#include <gtest/gtest.h>
#include <platform.h>

#include <reach/reach.h>
#include <type/subtype.h>

#include "util.h"

#include <cstring>
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
  reach_type_t* ab_reach = reach_type(reach, ab_ast, &opt);
  ASSERT_NE(ab_reach, (void*)NULL);

  size_t i = HASHMAP_BEGIN;
  reach_type_t* subtype;

  bool found = false;
  while((subtype = reach_type_cache_next(&ab_reach->subtypes, &i)) != NULL)
  {
    if(subtype->name == stringtab(opt.strtab, "Main"))
    {
      found = true;
      break;
    }
  }

  ASSERT_TRUE(found);
}

// Reaching a method on an intersection type one of whose members is a union
// must compile cleanly. add_rmethod_to_subtypes walks an intersection's
// members and looks the reached method up on each; when a member is a union,
// that lookup runs is_subtype_fun across the union's arms to reconcile their
// signatures, and that reconciliation calls is_bare. The member lookup used to
// pass opt == NULL, but is_bare needs a non-NULL opt (it interns into opt's
// string table), so the reconciliation aborted. `foo` lives in both arms of the
// `(T1 | T2)` member, which is what forces the multi-arm reconciliation; the
// method must reach without crashing.
TEST_F(ReachTest, IsectMemberUnionMethodReached)
{
  const char* src =
    "trait T1\n"
    "  fun foo()\n"

    "trait T2\n"
    "  fun foo()\n"

    "trait Other\n"
    "  fun bar()\n"

    "class C is (T1 & Other)\n"
    "  fun foo() => None\n"
    "  fun bar() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: ((T1 | T2) & Other) = C\n"
    "    x.foo()";

  TEST_COMPILE(src, "reach");

  ast_t* x_ast = type_of("x");
  ASSERT_NE(x_ast, (void*)NULL);

  reach_t* reach = compile->reach;
  reach_type_t* x_reach = reach_type(reach, x_ast, &opt);
  ASSERT_NE(x_reach, (void*)NULL);

  bool found = false;
  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;
  while((n = reach_method_names_next(&x_reach->methods, &i)) != NULL)
  {
    if(n->name == stringtab(opt.strtab, "foo"))
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
  reach_type_t* p_reach = reach_type(reach, p_ast, &opt);
  ASSERT_NE(p_reach, (void*)NULL);

  bool found = false;
  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;
  while((n = reach_method_names_next(&p_reach->methods, &i)) != NULL)
  {
    if(n->name == stringtab(opt.strtab, "example"))
    {
      found = true;
      break;
    }
  }

  ASSERT_TRUE(found);
}

// Return the mangled name of one reached method called `method_name` on the
// reached type `type_name`, or NULL if the type or method wasn't reached.
//
// A `box`/`tag` fun is reached under several receiver caps (box/ref/val), each
// a distinct entry in `r_mangled` with a cap-PREFIXED mangle (`box_f_...` vs
// `val_f_...`) — so the entries do NOT share a full mangled name and the one
// `reach_mangled_next` returns first is an arbitrary pick among them. That is
// fine for the mangling tests below, which only inspect the mangle SUFFIX (the
// `_p` marker, the trailing result-type character) and cross-method
// relationships, both invariant across the cap variants. Do not use this to
// assert a full mangled string.
static const char* any_mangled_name(reach_t* r, const char* type_name,
  const char* method_name, pass_opt_t* opt)
{
  reach_type_t* t = reach_type_name(r, type_name, opt);

  if(t == NULL)
    return NULL;

  reach_method_name_t* n = reach_method_name(t, stringtab(opt->strtab,
    method_name));

  if(n == NULL)
    return NULL;

  size_t i = HASHMAP_BEGIN;
  reach_method_t* m = reach_mangled_next(&n->r_mangled, &i);

  if(m == NULL)
    return NULL;

  return m->mangled_name;
}

static bool ends_with(const char* s, const char* suffix)
{
  size_t s_len = strlen(s);
  size_t suffix_len = strlen(suffix);

  return (s_len >= suffix_len) &&
    (strcmp(s + (s_len - suffix_len), suffix) == 0);
}

// Partiality is part of a method's calling convention (a partial method returns
// `{T, i1}`, a non-partial one returns `T`), so make_mangled_name encodes it: a
// partial method gets a trailing `_p` and therefore mangles distinctly, lands
// in its own vtable slot, and never shares a slot's calling convention with a
// non-partial method. This guards that encoding directly rather than only
// through the dispatch behaviour observed by full-program tests.
TEST_F(ReachTest, PartialAndNonPartialMangleDistinctly)
{
  const char* src =
    "class C1\n"
    "  fun f() ? => error\n"

    "class C2\n"
    "  fun f() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c1: C1 = C1\n"
    "    let c2: C2 = C2\n"
    "    try c1.f()? end\n"
    "    c2.f()";

  TEST_COMPILE(src, "reach");

  reach_t* reach = compile->reach;

  const char* partial = any_mangled_name(reach, "C1", "f", &opt);
  const char* non_partial = any_mangled_name(reach, "C2", "f", &opt);

  ASSERT_NE(partial, (void*)NULL);
  ASSERT_NE(non_partial, (void*)NULL);

  // Same signature, distinct mangle: the whole point of the encoding.
  ASSERT_STRNE(partial, non_partial);

  // The partial method carries the marker; the non-partial one does not.
  ASSERT_TRUE(ends_with(partial, "_p"));
  ASSERT_FALSE(ends_with(non_partial, "_p"));
}

// Bare methods (@-functions) are excluded from the partiality split: a bare
// partial function aborts on error rather than returning `{T, i1}`, so its
// calling convention does not depend on partiality and it must not get the `_p`
// marker. A bare partial method and a bare non-partial method with the same
// signature therefore mangle identically.
TEST_F(ReachTest, BareMethodsNotSplitByPartiality)
{
  const char* src =
    "primitive P1\n"
    "  fun @g() ? => error\n"

    "primitive P2\n"
    "  fun @g() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try P1.g()? end\n"
    "    P2.g()";

  TEST_COMPILE(src, "reach");

  reach_t* reach = compile->reach;

  const char* bare_partial = any_mangled_name(reach, "P1", "g", &opt);
  const char* bare_non_partial = any_mangled_name(reach, "P2", "g", &opt);

  ASSERT_NE(bare_partial, (void*)NULL);
  ASSERT_NE(bare_non_partial, (void*)NULL);

  // Bareness, not partiality, decides the slot: neither carries the marker...
  ASSERT_FALSE(ends_with(bare_partial, "_p"));
  ASSERT_FALSE(ends_with(bare_non_partial, "_p"));

  // ...so the two share a mangled name despite differing in partiality. (Bare
  // methods have a single r_mangled entry with no cap prefix, so a full-string
  // comparison is sound here.)
  ASSERT_STREQ(bare_partial, bare_non_partial);
}

// The `_p` marker is collision-free only because no non-partial method's mangle
// ends in 'p': a non-internal method's mangle ends in its result type's mangle,
// and every type mangle ends in a character from a fixed set that excludes 'p'
// (see the collision-free note in make_mangled_name). This pins that documented
// mapping — each numeric type, Bool, and an object type maps to its expected
// terminal character — so a change that remapped one of these onto 'p' fails
// here. (It does NOT catch a brand-new type whose mangle ends in 'p'; that
// broader guard, over every reached type including recursively-built tuple
// mangles, is NoReachedTypeMangleEndsInMarker below.)
TEST_F(ReachTest, NonPartialMangleNeverEndsInMarker)
{
  const char* src =
    "primitive R\n"
    "  fun i8(): I8 => 0\n"
    "  fun i16(): I16 => 0\n"
    "  fun i32(): I32 => 0\n"
    "  fun i64(): I64 => 0\n"
    "  fun i128(): I128 => 0\n"
    "  fun ilong(): ILong => 0\n"
    "  fun isize(): ISize => 0\n"
    "  fun u8(): U8 => 0\n"
    "  fun u16(): U16 => 0\n"
    "  fun u32(): U32 => 0\n"
    "  fun u64(): U64 => 0\n"
    "  fun u128(): U128 => 0\n"
    "  fun ulong(): ULong => 0\n"
    "  fun usize(): USize => 0\n"
    "  fun f32(): F32 => 0\n"
    "  fun f64(): F64 => 0\n"
    "  fun bool(): Bool => true\n"
    "  fun obj(): R => R\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    R.i8() ; R.i16() ; R.i32() ; R.i64() ; R.i128()\n"
    "    R.ilong() ; R.isize()\n"
    "    R.u8() ; R.u16() ; R.u32() ; R.u64() ; R.u128()\n"
    "    R.ulong() ; R.usize()\n"
    "    R.f32() ; R.f64() ; R.bool() ; R.obj()";

  TEST_COMPILE(src, "reach");

  reach_t* reach = compile->reach;

  struct { const char* method; char terminal; } cases[] = {
    {"i8", 'c'}, {"i16", 's'}, {"i32", 'i'}, {"i64", 'w'}, {"i128", 'q'},
    {"ilong", 'l'}, {"isize", 'z'},
    {"u8", 'C'}, {"u16", 'S'}, {"u32", 'I'}, {"u64", 'W'}, {"u128", 'Q'},
    {"ulong", 'L'}, {"usize", 'Z'},
    {"f32", 'f'}, {"f64", 'd'},
    {"bool", 'b'}, {"obj", 'o'}
  };

  for(size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++)
  {
    const char* mangled = any_mangled_name(reach, "R", cases[i].method, &opt);
    ASSERT_NE(mangled, (void*)NULL) << "method " << cases[i].method;

    size_t len = strlen(mangled);
    ASSERT_GT(len, (size_t)0) << "method " << cases[i].method;

    char terminal = mangled[len - 1];

    // Pins the documented type-mangle character...
    EXPECT_EQ(terminal, cases[i].terminal)
      << "method " << cases[i].method << " mangled as " << mangled;

    // ...which, in particular, is never the marker character.
    EXPECT_NE(terminal, 'p')
      << "method " << cases[i].method << " mangled as " << mangled;
  }
}

// The collision-free guarantee rests on a reciprocal invariant documented at
// the t->mangle assignments in reach.c ("no t->mangle value may be, or end in,
// 'p'"): the partial marker `_p` is appended last, so it can only stay distinct
// from a non-partial mangle if no type mangle — the final segment of a
// non-partial method's mangle — ends in 'p'. NonPartialMangleNeverEndsInMarker
// pins the documented per-type characters but, being enumerative, can't catch a
// newly-added type whose mangle ends in 'p'. This guards the invariant at its
// source: every reached type's mangle must not end in 'p'. The tuple result
// below forces a reached tuple type, exercising the recursively-built tuple
// mangle that the collision-free note calls out explicitly.
TEST_F(ReachTest, NoReachedTypeMangleEndsInMarker)
{
  const char* src =
    "primitive R\n"
    "  fun pair(): (I8, R) => (0, R)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    R.pair()";

  TEST_COMPILE(src, "reach");

  reach_t* reach = compile->reach;

  // A tuple type must actually be reached, else the recursive-mangle path this
  // test exists to cover wouldn't be exercised.
  bool saw_tuple = false;

  size_t i = HASHMAP_BEGIN;
  reach_type_t* t;
  while((t = reach_types_next(&reach->types, &i)) != NULL)
  {
    ASSERT_NE(t->mangle, (void*)NULL) << "type " << t->name;

    size_t len = strlen(t->mangle);

    if(len > 0)
      EXPECT_NE(t->mangle[len - 1], 'p')
        << "type " << t->name << " mangled as " << t->mangle;

    if(t->underlying == TK_TUPLETYPE)
      saw_tuple = true;
  }

  ASSERT_TRUE(saw_tuple);
}

// make_mangled_name notes that 'p' can appear EARLIER in a mangle without
// breaking the marker: trace_kind_append emits 'p' for a primitive parameter
// (constructors and behaviors carry trace-kind characters), always just before
// that parameter's own type mangle and never as the final character. Only the
// terminal character decides partiality, so an interior 'p' is harmless. This
// exercises that rule: a constructor with a primitive parameter mangles with an
// interior 'p', yet the non-partial one does not end in 'p' (so it can't
// collide with a partial mangle) and the partial one is still kept distinct by
// the trailing marker.
TEST_F(ReachTest, InteriorTraceKindMarkerDoesNotCollide)
{
  const char* src =
    "class C\n"
    "  new safe(x: U8) => None\n"
    "  new risky(x: U8) ? => error\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C.safe(0)\n"
    "    try C.risky(0)? end";

  TEST_COMPILE(src, "reach");

  reach_t* reach = compile->reach;

  const char* non_partial = any_mangled_name(reach, "C", "safe", &opt);
  const char* partial = any_mangled_name(reach, "C", "risky", &opt);

  ASSERT_NE(non_partial, (void*)NULL);
  ASSERT_NE(partial, (void*)NULL);

  // trace_kind emitted 'p' for the U8 parameter, sitting before the terminal
  // character (the method names and the U8/result mangles contain no 'p', so
  // this 'p' can only be the trace-kind character).
  const char* np_p = strchr(non_partial, 'p');
  ASSERT_NE(np_p, (void*)NULL);
  ASSERT_LT(np_p, non_partial + (strlen(non_partial) - 1));

  ASSERT_NE(strchr(partial, 'p'), (void*)NULL);

  // The non-partial mangle carries an interior 'p' yet does not end in 'p', so
  // it can never collide with a partial mangle...
  ASSERT_FALSE(ends_with(non_partial, "_p"));

  // ...while the partial one is still marked, despite also carrying the
  // interior trace-kind 'p'.
  ASSERT_TRUE(ends_with(partial, "_p"));
}
