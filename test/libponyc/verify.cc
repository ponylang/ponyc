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


class VerifyTest : public PassTest
{};


TEST_F(VerifyTest, MainActorFunCreate)
{
  // From issue #906
  const char* src =
    "actor Main\n"
    "  fun create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "the create method of the Main actor must be a constructor");
}

TEST_F(VerifyTest, MainActorNewCreateTypeParams)
{
  const char* src =
    "actor Main\n"
    "  new create[A: Number](env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "the create constructor of the Main actor must not take type parameters");
}

TEST_F(VerifyTest, MainActorNewCreateTwoParams)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env, bogus: Bool) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "the create constructor of the Main actor must take only a single Env "
    "parameter");
}

TEST_F(VerifyTest, MainActorNewCreateParamWrongType)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Bool) =>\n"
    "    None";

  TEST_ERRORS_1(src, "must be of type Env");
}

TEST_F(VerifyTest, MainActorNoConstructor)
{
  // From issue #1259
  const char* src =
    "actor Main\n";

  TEST_ERRORS_1(src,
    "The Main actor must have a create constructor which takes only a single "
    "Env parameter");
}

TEST_F(VerifyTest, PrimitiveWithTypeParamsInit)
{
  const char* src =
    "primitive Foo[A: Number]\n"
    "  fun _init() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive with type parameters cannot have an _init method");
}

TEST_F(VerifyTest, PrimitiveNewInit)
{
  const char* src =
    "primitive Foo\n"
    "  new _init() =>\n"
    "    None";

  TEST_ERRORS_3(src,
    "a primitive _init method must be a function",
    "a primitive _init method must use box as the receiver capability",
    "a primitive _init method must return None");
}

TEST_F(VerifyTest, PrimitiveFunRefInit)
{
  const char* src =
    "primitive Foo\n"
    "  fun ref _init() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive _init method must use box as the receiver capability");
}

TEST_F(VerifyTest, PrimitiveInitWithTypeParams)
{
  const char* src =
    "primitive Foo\n"
    "  fun _init[A: Number]() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive _init method must not take type parameters");
}

TEST_F(VerifyTest, PrimitiveInitWithParams)
{
  const char* src =
    "primitive Foo\n"
    "  fun _init(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive _init method must take no parameters");
}

TEST_F(VerifyTest, PrimitiveInitReturnBool)
{
  const char* src =
    "primitive Foo\n"
    "  fun _init(): Bool =>\n"
    "    true";

  TEST_ERRORS_1(src,
    "a primitive _init method must return None");
}

TEST_F(VerifyTest, PrimitiveInitPartial)
{
  const char* src =
    "primitive Foo\n"
    "  fun _init() ? =>\n"
    "    error";

  TEST_ERRORS_1(src,
    "a primitive _init method cannot be a partial function");
}

TEST_F(VerifyTest, StructFinal)
{
  const char* src =
    "struct Foo\n"
    "  fun _final() =>\n"
    "    None";

  TEST_ERRORS_1(src, "a struct cannot have a _final method");
}

TEST_F(VerifyTest, StructEmbedFieldFinal)
{
  const char* src =
    "class Foo\n"
    "  fun _final() =>\n"
    "    None\n"

    "struct Bar\n"
    "  embed f: Foo = Foo";

  TEST_ERRORS_1(src, "a struct cannot have a field with a _final method");
}

TEST_F(VerifyTest, PrimitiveWithTypeParamsFinal)
{
  const char* src =
    "primitive Foo[A: Number]\n"
    "  fun _final() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive with type parameters cannot have a _final method");
}

TEST_F(VerifyTest, ClassWithTypeParamsFinal)
{
  const char* src =
    "class Foo[A: Number]\n"
    "  fun _final() =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ActorBeFinal)
{
  const char* src =
    "actor Foo\n"
    "  be _final() =>\n"
    "    None";

  TEST_ERRORS_2(src,
    "a _final method must be a function",
    "a _final method must use box as the receiver capability");
}

TEST_F(VerifyTest, ClassFunRefFinal)
{
  const char* src =
    "class Foo\n"
    "  fun ref _final() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a _final method must use box as the receiver capability");
}

TEST_F(VerifyTest, ClassFinalWithTypeParams)
{
  const char* src =
    "class Foo\n"
    "  fun _final[A: Number]() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a _final method must not take type parameters");
}

TEST_F(VerifyTest, ClassFinalWithParams)
{
  const char* src =
    "class Foo\n"
    "  fun _final(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a _final method must take no parameters");
}

TEST_F(VerifyTest, ClassFinalReturnBool)
{
  const char* src =
    "class Foo\n"
    "  fun _final(): Bool =>\n"
    "    true";

  TEST_ERRORS_1(src,
    "a _final method must return None");
}

TEST_F(VerifyTest, ClassFinalPartial)
{
  const char* src =
    "class Foo\n"
    "  fun _final() ? =>\n"
    "    error";

  TEST_ERRORS_1(src,
    "a _final method cannot be a partial function");
}

TEST_F(VerifyTest, TryNoError)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply() =>\n"
    "    try None end";

  TEST_ERRORS_1(src, "try expression never results in an error");
}

TEST_F(VerifyTest, TryError)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply() =>\n"
    "    try error end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, TryTryElseError)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply() =>\n"
    "    try try error else error end end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, TryBranchedError)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply() =>\n"
    "    try if true then error else None end end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, TryCallPartialFunction)
{
  const char* src =
    "primitive Foo\n"
    "  fun partial() ? => error\n"
    "  fun apply() =>\n"
    "    try partial()? end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, TryCallCurriedPartialFunction)
{
  const char* src =
    "primitive Foo\n"
    "  fun partial() ? => error\n"
    "  fun apply() =>\n"
    "    let fn = this~partial()\n"
    "    try fn()? end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, TryCallChainedPartialFunction)
{
  const char* src =
    "primitive Foo\n"
    "  fun partial() ? => error\n"
    "  fun apply() =>\n"
    "    try this.>partial()? end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialFunctionNoError)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply() ? =>\n"
    "    None";

  TEST_ERRORS_1(src, "function signature is marked as partial but the function "
    "body cannot raise an error");
}

TEST_F(VerifyTest, PartialFunctionError)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply() ? =>\n"
    "    error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialFunctionBranchedError)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply() ? =>\n"
    "    if true then error else None end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialFunctionCallPartialFunction)
{
  const char* src =
    "primitive Foo\n"
    "  fun partial() ? => error\n"
    "  fun apply() ? =>\n"
    "    partial()?";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialFunctionCallCurriedPartialFunction)
{
  const char* src =
    "primitive Foo\n"
    "  fun partial() ? => error\n"
    "  fun apply() ? =>\n"
    "    let fn = this~partial()\n"
    "    fn()?";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialFunctionCallNonPartialFunction)
{
  const char* src =
    "primitive Foo\n"
    "  fun non_partial() => None\n"
    "  fun apply() =>\n"
    "    non_partial()?";

  TEST_ERRORS_1(src, "call is partial but the method is not");
}

TEST_F(VerifyTest, NonPartialFunctionCallPartialFunction)
{
  const char* src =
    "primitive Foo\n"
    "  fun partial() ? => error\n"
    "  fun apply() ? =>\n"
    "    partial()";

  TEST_ERRORS_1(src, "call is not partial but the method is");
}

TEST_F(VerifyTest, NonPartialFunctionError)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply() =>\n"
    "    error";

  TEST_ERRORS_1(src, "function signature is not marked as partial but the "
    "function body can raise an error");
}

TEST_F(VerifyTest, ErroringBehaviourError)
{
  const char* src =
    "actor Foo\n"
    "  be apply() =>\n"
    "    error";

  TEST_ERRORS_1(src, "a behaviour must handle any potential error");
}

TEST_F(VerifyTest, ErroringActorConstructorError)
{
  const char* src =
    "actor Foo\n"
    "  new create() =>\n"
    "    error";

  TEST_ERRORS_1(src, "an actor constructor must handle any potential error");
}

TEST_F(VerifyTest, TraitPartialFunctionNoError)
{
  const char* src =
    "trait Foo\n"
    "  fun apply() ? =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, TraitNonPartialFunctionError)
{
  const char* src =
    "trait Foo\n"
    "  fun apply() =>\n"
    "    error";

  TEST_ERRORS_1(src, "function signature is not marked as partial but the "
    "function body can raise an error");
}

TEST_F(VerifyTest, InterfacePartialFunctionNoError)
{
  const char* src =
    "interface Foo\n"
    "  fun apply() ? =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, InterfaceNonPartialFunctionError)
{
  const char* src =
    "interface Foo\n"
    "  fun apply() =>\n"
    "    error";

  TEST_ERRORS_1(src, "function signature is not marked as partial but the "
    "function body can raise an error");
}

TEST_F(VerifyTest, IfTypeError)
{
  const char* src =
    "primitive A\n"
    "primitive B\n"

    "primitive Foo\n"
    "  fun apply[X: (A|B)]() =>\n"
    "    iftype X <: A then\n"
    "      error\n"
    "    end\n";

  TEST_ERRORS_1(src, "function signature is not marked as partial but the "
    "function body can raise an error");
}

TEST_F(VerifyTest, NonPartialSugaredApplyCall)
{
  const char* src =
    "primitive Bar\n"
    "  new create() => None\n"
    "  fun apply() => None\n"

    "primitive Foo\n"
    "  fun apply() =>\n"
    "    let bar = Bar\n"
    "    bar()";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialSugaredApplyCall)
{
  const char* src =
    "primitive Bar\n"
    "  new create() => None\n"
    "  fun apply() ? => error\n"

    "primitive Foo\n"
    "  fun apply() ? =>\n"
    "    let bar = Bar\n"
    "    bar()?";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, NonPartialSugaredApplyCallAfterSugaredConstructor)
{
  const char* src =
    "primitive Bar\n"
    "  new create() => None\n"
    "  fun apply() => None\n"

    "primitive Foo\n"
    "  fun apply() =>\n"
    "    Bar()";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialSugaredApplyCallAfterSugaredConstructor)
{
  const char* src =
    "primitive Bar\n"
    "  new create() => None\n"
    "  fun apply() ? => error\n"

    "primitive Foo\n"
    "  fun apply() ? =>\n"
    "    Bar()?";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, FFICallPartial)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply(): U64 ? =>\n"
    "    @foo[U64]()?";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, FFICallPartialWithPartialDeclaration)
{
  const char* src =
    "use @foo[U64]() ?\n"

    "primitive Foo\n"
    "  fun apply(): U64 ? =>\n"
    "    @foo[U64]()?";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, FFICallWithPartialDeclaration)
{
  const char* src =
    "use @foo[U64]() ?\n"

    "primitive Foo\n"
    "  fun apply(): U64 ? =>\n"
    "    @foo[U64]()?";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, FFICallPartialWithNonPartialDeclaration)
{
  const char* src =
    "use @foo[U64]()\n"

    "primitive Foo\n"
    "  fun apply(): U64 =>\n"
    "    @foo[U64]() ?";

  TEST_ERRORS_1(src, "call is partial but the declaration is not");
}

TEST_F(VerifyTest, FFICallNonPartialWithPartialDeclaration)
{
  const char* src =
    "use @foo[U64]() ?\n"

    "primitive Foo\n"
    "  fun apply(): U64 ? =>\n"
    "    @foo[U64]()";

  TEST_ERRORS_1(src, "call is not partial but the declaration is");
}

TEST_F(VerifyTest, NonPartialSugaredBinaryOperatorCall)
{
  const char* src =
    "class val Bar\n"
    "  new val create() => None\n"
    "  fun val add(other: Bar): Bar => this\n"

    "primitive Foo\n"
    "  fun apply(): Bar =>\n"
    "    Bar + Bar";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialSugaredBinaryOperatorCall)
{
  const char* src =
    "class val Bar\n"
    "  new val create() => None\n"
    "  fun val add_partial(other: Bar): Bar ? => error\n"

    "primitive Foo\n"
    "  fun apply(): Bar ? =>\n"
    "    Bar +? Bar";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialTraitCall)
{
  // From issue #2228
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let b = B\n"

    "trait A\n"
    "  fun a1() ?\n"
    "  fun a2() ? => a1()?\n"

    "class B is A\n"
    "  fun a1() =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, LambdaTypeGenericAliased)
{
  const char* src =
    "type Foo[T] is {(T): T}";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, PartialFunCallInTryBlock)
{
  // From issue #2146
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try partial()? else partial()? end\n"
    "fun partial() ? => error";

  {
      const char* errs[] = {"an actor constructor must handle any potential error", NULL};
      const char* frame1[] = {"an error can be raised here", NULL};
      const char** frames[] = {frame1, NULL};
      DO(test_expected_error_frames(src, "verify", errs, frames));
  }
}

TEST_F(VerifyTest, ConsumeVarReassignTrySameExpressionPartial)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C): C iso^ ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c = P(consume c) ?\n"
        "end\n";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed identifier in a try expression if there is a partial call involved");
}

TEST_F(VerifyTest, ConsumeVarReassignTrySameExpression)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c = consume c\n"
        "  error\n"
        "end\n";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarReassignTrySameExpressionTryElse)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c = try error else consume c end\n"
        "  error\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarReassignTrySameExpressionTryError)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c = (try error else error end; consume c)\n"
        "end";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeVarReassignTrySameExpressionIfError)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c = if true then error else consume c end\n"
        "end";

  TEST_ERRORS_1(src, "can't reassign to a consumed identifier in a try expression if there is a partial call involved");
}

TEST_F(VerifyTest, ConsumeVarReassignTrySameExpressionAs)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "var x: (C | None) = None\n"
        "try\n"
        "  c = (x as C; consume c)\n"
        "end";

  TEST_ERRORS_1(src, "can't reassign to a consumed identifier in a try expression if there is a partial call involved");
}
