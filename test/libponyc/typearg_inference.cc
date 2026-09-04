#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_COMPILE_IR(src) DO(test_compile(src, "ir"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "expr", expect, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }


class TypeArgInferenceTest : public PassTest
{};


// --- Simple inference from a single argument ---

TEST_F(TypeArgInferenceTest, SingleParam_DirectPosition)
{
  // foo(U8(1)) should infer A = U8
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1))\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1))\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, TwoParams_DirectPosition)
{
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val, B: Any val](a: A, b: B) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1), \"hello\")\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val, B: Any val](a: A, b: B) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8, String val](U8(1), \"hello\")\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- Default-first rule ---

TEST_F(TypeArgInferenceTest, DefaultKept_WhenArgFits)
{
  // A has default U8; argument is U8 — default is kept.
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val = U8](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1))\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val = U8](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1))\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, DefaultOverridden_WhenArgDoesNotFit)
{
  // A has default U8; argument is String — inferred type overrides default.
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val = U8](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(\"hello\")\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val = U8](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[String val](\"hello\")\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- Multiple arguments for the same type parameter ---

TEST_F(TypeArgInferenceTest, TwoArgs_SameTypeParam_SameType)
{
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1), U8(2))\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1), U8(2))\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- Nested position inference ---

TEST_F(TypeArgInferenceTest, NestedPosition_ArrayElement)
{
  // fun foo[A: Any val](a: Array[A] val) => None
  // foo(recover val [as String val: "hello"] end) — A inferred from Array
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: Array[A] val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(recover val [as String val: \"hello\"] end)\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: Array[A] val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[String val](recover val [as String val: \"hello\"] end)\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- M1 errors (no evidence) ---

TEST_F(TypeArgInferenceTest, M1_NoPosition)
{
  // Type parameter not mentioned in any parameter type.
  // Falls through to defaults-only path since no param mentions A.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val](x: U8) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1))\n";

  TEST_ERRORS_1(src, "not enough type arguments");
}

TEST_F(TypeArgInferenceTest, M1_LiteralOnly)
{
  // All arguments at bindable positions are literals — no evidence.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(1)\n";

  TEST_ERRORS_1(src, "could not infer the type arguments");
}


// --- M2 error (conflict) ---

TEST_F(TypeArgInferenceTest, M2_Conflict)
{
  // Two arguments for the same type parameter with incompatible types.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1), \"hello\")\n";

  TEST_ERRORS_1(src, "conflicting types for type parameter");
}


// --- Self-call equivalence ---

TEST_F(TypeArgInferenceTest, SelfCall_Equiv)
{
  // fun foo[A: Any val](a: A, b: A) calling foo(a, b)
  // should be equivalent to foo[A](a, b)
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: A): A =>\n"
    "    foo(a, b)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1), U8(2))\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: A): A =>\n"
    "    foo[A](a, b)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1), U8(2))\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- Same-name caller equivalence ---

TEST_F(TypeArgInferenceTest, SameNameCaller_Equiv)
{
  // fun bar[A: Any val](a: A) => Baz.foo(a)
  // should be equivalent to Baz.foo[A](a)
  const char* inferred =
    "primitive Baz\n"
    "  fun foo[A: Any val](a: A): A => a\n"

    "primitive Bar\n"
    "  fun bar[A: Any val](a: A): A =>\n"
    "    Baz.foo(a)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.bar[U8](U8(1))\n";

  const char* explicit_form =
    "primitive Baz\n"
    "  fun foo[A: Any val](a: A): A => a\n"

    "primitive Bar\n"
    "  fun bar[A: Any val](a: A): A =>\n"
    "    Baz.foo[A](a)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.bar[U8](U8(1))\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- Pre-order hook: antecedent-dependent arguments ---

TEST_F(TypeArgInferenceTest, PreOrder_LambdaGetsReifiedType)
{
  // The lambda's parameter type depends on the inferred type parameter.
  // The pre-order hook infers A=U8 from the first argument, reifies
  // the parameter types, then the normal pass visits the lambda against
  // the reified type.
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, f: {(A): A} val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1), {(x: U8): U8 => x})\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, f: {(A): A} val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1), {(x: U8): U8 => x})\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, PreOrder_LambdaUntypedParams)
{
  // Regression test for issue #5981: lambda parameters without explicit
  // types should be inferred from the reified parameter type after type
  // argument inference.
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, f: {(A): A} val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1), {(x) => x})\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, f: {(A): A} val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1), {(x) => x})\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, PreOrder_LambdaInRecover)
{
  // Untyped lambda inside a recover block at a type-parameter-dependent
  // position.
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, f: {(A): A} val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1), recover val {(x) => x} end)\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, f: {(A): A} val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1), recover val {(x) => x} end)\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- Constructor-path inference ---

TEST_F(TypeArgInferenceTest, ConstructorPath_BareTyperef)
{
  // Foo(x) -> Foo[String val](x)
  const char* inferred =
    "class Foo[A: Any val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo(\"hello\")\n";

  const char* explicit_form =
    "class Foo[A: Any val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[String val](\"hello\")\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, ConstructorPath_DotCreate)
{
  // Foo.create(x) -> Foo[String val].create(x)
  const char* inferred =
    "class Foo[A: Any val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo.create(\"hello\")\n";

  const char* explicit_form =
    "class Foo[A: Any val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[String val].create(\"hello\")\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, ConstructorPath_LambdaUntypedParams)
{
  // Constructor path: lambda with untyped parameters at a position whose
  // type mentions the class type parameter.
  const char* inferred =
    "class Foo[A: Any val]\n"
    "  let _a: A\n"
    "  let _f: {(A): A} val\n"
    "  new create(a: A, f: {(A): A} val) => _a = a; _f = f\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo(U8(1), {(x) => x})\n";

  const char* explicit_form =
    "class Foo[A: Any val]\n"
    "  let _a: A\n"
    "  let _f: {(A): A} val\n"
    "  new create(a: A, f: {(A): A} val) => _a = a; _f = f\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[U8](U8(1), {(x) => x})\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, ConstructorPath_TwoParams)
{
  // Two class type params inferred from constructor args.
  const char* inferred =
    "class Pair[A: Any val, B: Any val]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  new create(a: A, b: B) => _a = a; _b = b\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Pair(U8(1), \"hello\")\n";

  const char* explicit_form =
    "class Pair[A: Any val, B: Any val]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  new create(a: A, b: B) => _a = a; _b = b\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Pair[U8, String val](U8(1), \"hello\")\n";

  TEST_EQUIV(inferred, explicit_form);
}


TEST_F(TypeArgInferenceTest, M4_MethodCall_NotConstructor)
{
  // Foo.some_fun(x) — type args not inferred on non-constructor calls.
  const char* src =
    "class Foo[A: Any val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"
    "  fun get(): A => _a\n"

    "primitive Bar\n"
    "  fun apply() =>\n"
    "    Foo.get()\n";

  TEST_ERRORS_1(src, "not enough type arguments");
}


// --- Codegen survival: inferred type args must survive through IR ---

TEST_F(TypeArgInferenceTest, Codegen_SimpleInference)
{
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A): A => a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U8 = Bar.foo(U8(42))\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeArgInferenceTest, Codegen_TwoParamInference)
{
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val, B: Any val](a: A, b: B): A => a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U8 = Bar.foo(U8(42), \"hello\")\n";

  TEST_COMPILE_IR(src);
}


// --- H1: unbound reasons that had no tests ---

TEST_F(TypeArgInferenceTest, M1_NoArgument)
{
  // A is mentioned at a bindable position but no argument is given there.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: U8 = 0) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(where b = U8(1))\n";

  TEST_ERRORS_1(src, "could not infer the type arguments");
}

TEST_F(TypeArgInferenceTest, M1_Dependent)
{
  // The only argument mentioning A is a lambda — antecedent-dependent,
  // so inference has no evidence for A.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val](f: {(A): A} val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo({(x: U8): U8 => x})\n";

  TEST_ERRORS_1(src, "could not infer the type arguments");
}

TEST_F(TypeArgInferenceTest, M1_DefaultUndecided)
{
  // A's default is B, but B cannot be determined from the arguments.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val = B, B: Any val](b: B) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1))\n";

  TEST_ERRORS_1(src, "could not infer the type arguments");
}


// --- M3: state transitions ---

TEST_F(TypeArgInferenceTest, SupertypeMerge)
{
  // Two arguments with incompatible types for the same type parameter.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1), USize(2))\n";

  TEST_ERRORS_1(src, "conflicting types for type parameter");
}

TEST_F(TypeArgInferenceTest, BoundToPinned_NestedOverridesDirect)
{
  // First argument at direct position → Bound. Second at nested → Pinned.
  // Pinned wins over direct.
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: Array[A] val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1), recover val [as U8: U8(2)] end)\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: Array[A] val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1), recover val [as U8: U8(2)] end)\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, PinnedConflict_TwoNestedDisagree)
{
  // Two nested positions give different exact types → Pinned then Conflict.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: Array[A] val, b: Array[A] val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(recover val [as U8: U8(1)] end, "
    "            recover val [as String val: \"hello\"] end)\n";

  TEST_ERRORS_1(src, "conflicting types for type parameter");
}


// --- M5: provides_walk ---

TEST_F(TypeArgInferenceTest, ProvidesWalk_InterfaceArg)
{
  // Argument type implements an interface matching the parameter type.
  // provides_walk finds the match through the implements chain.
  const char* inferred =
    "interface Gettable[A: Any val]\n"
    "  fun get(): A\n"

    "primitive GetU8 is Gettable[U8]\n"
    "  fun get(): U8 => U8(0)\n"

    "primitive Bar\n"
    "  fun foo[A: Any val](s: Gettable[A] val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(GetU8)\n";

  const char* explicit_form =
    "interface Gettable[A: Any val]\n"
    "  fun get(): A\n"

    "primitive GetU8 is Gettable[U8]\n"
    "  fun get(): U8 => U8(0)\n"

    "primitive Bar\n"
    "  fun foo[A: Any val](s: Gettable[A] val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](GetU8)\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- M6: two-level inference (class + method type params) ---

TEST_F(TypeArgInferenceTest, TwoLevel_ClassAndMethodTypeParams)
{
  // Constructor path infers the class type parameter; then method-level
  // inference infers B from the second argument.
  const char* inferred =
    "class Box2[A: Any val]\n"
    "  let _a: A\n"
    "  new create[B: Any val](a: A, b: B) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Box2(U8(1), \"hello\")\n";

  const char* explicit_form =
    "class Box2[A: Any val]\n"
    "  let _a: A\n"
    "  new create[B: Any val](a: A, b: B) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Box2[U8].create[String val](U8(1), \"hello\")\n";

  TEST_EQUIV(inferred, explicit_form);
}


// --- L12: named constructor, array skip, conflict continuation text ---

TEST_F(TypeArgInferenceTest, ConstructorPath_NamedConstructor)
{
  // Foo.from_u8(x) — a named constructor, not `create`.
  const char* inferred =
    "class Wrap[A: Any val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"
    "  new from_u8(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Wrap.from_u8(U8(42))\n";

  const char* explicit_form =
    "class Wrap[A: Any val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"
    "  new from_u8(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Wrap[U8].from_u8(U8(42))\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, PreOrder_ArraySkippedAtBindablePosition)
{
  // Array literal at a bindable position is skipped during inference;
  // the second argument determines A, and the array is typed afterward.
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: Array[A] val, b: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(recover val [as U8: U8(1)] end, U8(2))\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: Array[A] val, b: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](recover val [as U8: U8(1)] end, U8(2))\n";

  TEST_EQUIV(inferred, explicit_form);
}

TEST_F(TypeArgInferenceTest, SubtypeMerge_WidensToSupertype)
{
  // Two arguments where one is a subtype of the other: the supertype wins.
  // U8 <: (U8 | String val), so passing U8 and (U8 | String val) should
  // resolve to (U8 | String val).
  const char* inferred =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: (U8 | String val) = U8(1)\n"
    "    Bar.foo(U8(2), x)\n";

  const char* explicit_form =
    "primitive Bar\n"
    "  fun foo[A: Any val](a: A, b: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: (U8 | String val) = U8(1)\n"
    "    Bar.foo[(U8 | String val)](U8(2), x)\n";

  TEST_EQUIV(inferred, explicit_form);
}
