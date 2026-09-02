#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_COMPILE_IR(src) DO(test_compile(src, "ir"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "expr", expect, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

#define TEST_ERROR_WITH_NOTE(src, primary, note) \
  { const char* errs[] = {primary, NULL}; \
    const char* frame_strs[] = {note, NULL}; \
    const char** frames[] = {frame_strs, NULL}; \
    DO(test_expected_error_frames(src, "ir", errs, frames)); }

class TypeParamsTest : public PassTest
{};

TEST_F(TypeParamsTest, DefaultCapInConstraint_ClassGetsRef)
{
  // From issue #5116. A bare class name in a constraint uses the class's
  // default cap (ref), so a ref-receiver method is callable.
  const char* src =
    "class C\n"
    "  fun ref mutate() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A: C](x: A) =>\n"
    "    x.mutate()";

  TEST_COMPILE(src);
}

TEST_F(TypeParamsTest, DefaultCapInConstraint_ValNotSubtypeOfRef)
{
  // From issue #5116. A bare class name in a constraint uses the class's
  // default cap (ref). A val type argument does not satisfy a ref constraint.
  const char* src =
    "class C\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: C val = recover val C end\n"
    "    foo[C val](c)\n"

    "  fun foo[A: C](x: A) => None";

  TEST_ERRORS_1(src, "type argument is outside its constraint");
}

TEST_F(TypeParamsTest, ReifySimultaneously)
{
  // From issue #1870
  const char* src =
    "interface State[S, I, O]\n"
    " fun val apply(state: S, input: I): (S, O)\n"
    " fun val bind[O2](next: State[S, O, O2]): State[S, I, O2]\n";

  TEST_COMPILE(src);
}

TEST_F(TypeParamsTest, DefaultRefersToEarlierParam_FunEquiv)
{
  // From issue #3540. A default naming an earlier parameter is reified.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A, B = A](a: A, b: B): None => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[String](\"a\", \"b\")\n";

  const char* expect =
    "primitive Bar\n"
    "  fun foo[A, B = A](a: A, b: B): None => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[String, String](\"a\", \"b\")\n";

  TEST_EQUIV(src, expect);
}

TEST_F(TypeParamsTest, DefaultRefersToEarlierParam_ClassAnnotation)
{
  const char* src =
    "class Foo[A: Any val, B: Any val = A]\n"
    "  let x: A\n"
    "  let y: B\n"
    "  new create(x': A, y': B) =>\n"
    "    x = x'\n"
    "    y = y'\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f: Foo[String] = Foo[String](\"hello\", \"world\")\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DefaultRefersToEarlierParam_Alias)
{
  const char* src =
    "class Foo[A: Any val, B: Any val = A]\n"
    "  let x: A\n"
    "  let y: B\n"
    "  new create(x': A, y': B) =>\n"
    "    x = x'\n"
    "    y = y'\n"

    "type Bar[A: Any val, B: Any val = A] is Foo[A, B]\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let b: Bar[U8] = Foo[U8](U8(1), U8(2))\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DefaultRefersToEarlierParam_Recursive)
{
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val, B: Any val = A](a: A, again: Bool): None =>\n"
    "    if again then\n"
    "      foo[A](a, false)\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[String](\"hello\", true)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DefaultRefersToEarlierParam_ArrowRegression)
{
  const char* src =
    "class Foo[A: Any val]\n"
    "  fun f[B: Any #read = this->A](x: B) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[U8].f(U8(1))\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DefaultRefersToEarlierParam_IterIssue3540)
{
  // The original program from issue #3540.
  const char* src =
    "class Iter[A, I: Iterator[A] ref = Iterator[A] ref] is Iterator[A]\n"
    "  let _inner: I\n"
    "  new create(inner: I) =>\n"
    "    _inner = inner\n"
    "  fun ref has_next(): Bool =>\n"
    "    _inner.has_next()\n"
    "  fun ref next(): A ? =>\n"
    "    _inner.next()?\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let iter = Iter[U8]([as U8: 0].values())\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DefaultRefersToEarlierParam_ThreeLevelChain)
{
  // Three-level default chain: C defaults to B, B defaults to A.
  const char* src =
    "class Foo[A: Any val, B: Any val = A, C: Any val = B]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  let _c: C\n"
    "  new create(a: A, b: B, c: C) =>\n"
    "    _a = a\n"
    "    _b = b\n"
    "    _c = c\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[U8](U8(1), U8(2), U8(3))\n";

  TEST_COMPILE_IR(src);
}


// Issue #5962: lambda type as a type parameter default.

TEST_F(TypeParamsTest, LambdaDefault_MethodForm)
{
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val = {(U8): U8} val](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {(x: U8): U8 => x} val\n"
    "    Bar.foo(f)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_ClassForm)
{
  const char* src =
    "class Foo[A: Any val = {(U8): U8} val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {(x: U8): U8 => x} val\n"
    "    Foo(f)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_ClassAnnotation)
{
  const char* src =
    "class Foo[A: Any val = {(U8): U8} val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {(x: U8): U8 => x} val\n"
    "    let x: Foo = Foo(f)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_PartialExplicit_Class)
{
  const char* src =
    "class Foo[A: Any val, B: Any val = {(U8): U8} val]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  new create(a: A, b: B) =>\n"
    "    _a = a\n"
    "    _b = b\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {(x: U8): U8 => x} val\n"
    "    Foo[U8](U8(1), f)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_PartialExplicit_Method)
{
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val, B: Any val = {(U8): U8} val](a: A, b: B)"
    "  => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {(x: U8): U8 => x} val\n"
    "    Bar.foo[U8](U8(1), f)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_OwnTypeParams_Single)
{
  // {[X: Any val](X): X} val as a default on the only type param.
  // The interface ends up with no type params (only the defaulted one,
  // which is dropped).
  const char* src =
    "class Foo[A: Any val = {[X: Any val](X): X} val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {[X: Any val](x: X): X => x} val\n"
    "    Foo(f)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_OwnTypeParams_WithEarlierSibling)
{
  // {[X: Any val](X): X} val as a default with an earlier sibling.
  // The interface keeps A (the earlier sibling).
  const char* src =
    "class Foo[A: Any val, B: Any val = {[X: Any val](X): X} val]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  new create(a: A, b: B) =>\n"
    "    _a = a\n"
    "    _b = b\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {[X: Any val](x: X): X => x} val\n"
    "    Foo[U8](U8(1), f)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_AliasWrittenStillWorks)
{
  const char* src =
    "type Fn is {(U8): U8} val\n"

    "primitive Bar\n"
    "  fun foo[A: Any val = Fn](a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {(x: U8): U8 => x} val\n"
    "    Bar.foo(f)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_SelfRefWithWrittenArg)
{
  // [A = {(A): A} val] used as Foo[U8] — the type argument is written,
  // so the default is not used and the self-reference is harmless.
  const char* src =
    "class Foo[A: Any val = {(A): A} val]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[U8](U8(1))\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, LambdaDefault_ConstraintKept_WrittenArgs)
{
  // B is kept because A's constraint names B.
  const char* src =
    "class Foo[A: (B | None), B: Any val = {(U8): U8} val]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  new create(a: A, b: B) =>\n"
    "    _a = a\n"
    "    _b = b\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f = {(x: U8): U8 => x} val\n"
    "    Foo[None, {(U8): U8} val](None, f)\n";

  TEST_COMPILE_IR(src);
}


// Issue #5957: default naming itself or a later sibling.

TEST_F(TypeParamsTest, DanglingDefault_ClassAnnotation)
{
  const char* src =
    "class Foo[A: Any val = B, B: Any val = U8]\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Foo = Foo\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter B");
}

TEST_F(TypeParamsTest, DanglingDefault_MethodCall)
{
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val = B, B: Any val = U8](x: U8) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1))\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter B");
}

TEST_F(TypeParamsTest, DanglingDefault_PartialExplicit_Class)
{
  const char* src =
    "class Foo[A: Any val, B: Any val = C, C: Any val = U8]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[U8](U8(1))\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter C");
}

TEST_F(TypeParamsTest, DanglingDefault_PartialExplicit_Method)
{
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val, B: Any val = C, C: Any val = U8]"
    "(x: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[U8](U8(1))\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter C");
}

TEST_F(TypeParamsTest, DanglingDefault_SelfRef_Class)
{
  const char* src =
    "class Foo[A: Any val = A]\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Foo = Foo\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter A");
}

TEST_F(TypeParamsTest, DanglingDefault_SelfRef_Method)
{
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val = A](x: U8) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1))\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter A");
}

TEST_F(TypeParamsTest, DanglingDefault_Nested)
{
  const char* src =
    "class Foo[A: Any val = Array[B] val, B: Any val = U8]\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Foo = Foo\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter B");
}

TEST_F(TypeParamsTest, DanglingDefault_Alias)
{
  const char* src =
    "class Foo[A: Any val = B, B: Any val = U8]\n"
    "  new create() => None\n"

    "type Bar is Foo\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Bar = Foo\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter B");
}

TEST_F(TypeParamsTest, DanglingDefault_LambdaSelfRef_Unused)
{
  // A self-referencing lambda default at an unreached annotation site.
  // The default has no meaning (it refers to itself) and is rejected.
  const char* src =
    "class Foo[A: Any val = {(A): A} val]\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Foo = Foo\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter A");
}

TEST_F(TypeParamsTest, DanglingDefault_LambdaConstraintKeepsB)
{
  // A defaults to B, B defaults to a lambda. The dangling check fires
  // on A's default because it names B (a later sibling).
  const char* src =
    "class Foo[A: Any val = B, B: Any val = {(U8): U8} val]\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Foo = Foo\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter B");
}

TEST_F(TypeParamsTest, DanglingDefault_LambdaOwnParam_NamesLaterSibling)
{
  // {[X: B](X): X} val as a default on A, where B is a later sibling.
  const char* src =
    "class Foo[A: Any val = {[X: B](X): X} val, B: Any val = U8]\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Foo = Foo\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter B");
}

TEST_F(TypeParamsTest, DanglingDefault_LambdaConstraintNamesB)
{
  // B's default {(U8): U8} val doesn't name B, but A's constraint
  // (B | None) does. The constraint dependency keeps B in the
  // sugared interface, making the nominal $0[A, B] reference B.
  const char* src =
    "class Foo[A: (B | None), B: Any val = {(U8): U8} val]\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Foo[None] = Foo[None]\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter B");
}

TEST_F(TypeParamsTest, DanglingDefault_LambdaOwnParamDefaultedByLater)
{
  // {[X: Any val = B](X): X} val where B is a later sibling. The
  // lambda's own type parameter X has a default naming B.
  const char* src =
    "class Foo[A: Any val = {[X: Any val = B](X): X} val,"
    " B: Any val = U8]\n"
    "  new create() => None\n"

    "primitive Bar\n"
    "  fun foo[A: Any val = {[X: Any val = B](X): X} val,"
    " B: Any val = U8](x: U8) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(U8(1))\n";

  TEST_ERROR_WITH_NOTE(src, "not enough type arguments",
    "default refers to type parameter B");
}

// Green pins for #5957: these must keep compiling.

TEST_F(TypeParamsTest, DanglingDefault_ConstraintOnly_Green)
{
  // A constraint naming a later sibling is legal.
  const char* src =
    "class Foo[A: (B | None), B: Any val = U8]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  new create(a: A, b: B) =>\n"
    "    _a = a\n"
    "    _b = b\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[U8](U8(1), U8(2))\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DanglingDefault_WrittenArgs_Green)
{
  // Dangling default, but all type arguments are written, so the default
  // is never used.
  const char* src =
    "class Foo[A: Any val = B, B: Any val = U8]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  new create(a: A, b: B) =>\n"
    "    _a = a\n"
    "    _b = b\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo[U8, U8](U8(1), U8(2))\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DanglingDefault_DefinitionOnly_Green)
{
  // A definition with a dangling default that is only ever used with
  // written type arguments keeps compiling.
  const char* src =
    "class Foo[A: Any val = B, B: Any val = U8]\n"
    "  let _a: A\n"
    "  let _b: B\n"
    "  new create(a: A, b: B) =>\n"
    "    _a = a\n"
    "    _b = b\n"

    "primitive Bar\n"
    "  fun foo(x: Foo[U8, U8]) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo(Foo[U8, U8](U8(1), U8(2)))\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DanglingDefault_RecursiveMethod_Green)
{
  // A method that calls itself with [B] — B is the method's own type
  // parameter whose default is A (an earlier sibling). The call supplies
  // B from the caller's scope, so no dangling reference. The recursive
  // call passes b (type B) as both arguments; in the recursive frame
  // A=B and B=A (from the default), so both parameters accept B.
  const char* src =
    "primitive Bar\n"
    "  fun foo[A: Any val, B: Any val = A](a: A, b: B,"
    " again: Bool): None =>\n"
    "    if again then\n"
    "      foo[B](b, b, false)\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Bar.foo[String](\"hello\", \"world\", true)\n";

  TEST_COMPILE_IR(src);
}

TEST_F(TypeParamsTest, DanglingDefault_ReturnUsesLaterParam_Green)
{
  // fun g(): (Foo[B] | None) with both arguments written at the call.
  const char* src =
    "class Foo[A: Any val, B: Any val = A]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"
    "  fun g(): (Foo[B] val | None) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f: Foo[U8, U8] val = Foo[U8](U8(1))\n"
    "    f.g()\n";

  TEST_COMPILE_IR(src);
}
