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
      const char* errs[] =
        {"an actor constructor must handle any potential error", NULL};
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
    "can't reassign to a consumed identifier in a try expression if there is a"
    " partial call involved");
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

  TEST_ERRORS_1(src, "can't reassign to a consumed identifier in a try"
    " expression if there is a partial call involved");
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

  TEST_ERRORS_1(src, "can't reassign to a consumed identifier in a try"
    " expression if there is a partial call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignTrySameExpressionPartial)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C): C iso^ ? => error\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c = P(consume c) ?\n"
        "end\n";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignTrySameExpression)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c = consume c\n"
        "  error\n"
        "end\n";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldReassignTrySameExpressionTryElse)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c = try error else consume c end\n"
        "  error\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldReassignTrySameExpressionTryError)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c = (try error else error end; consume c)\n"
        "end";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignTrySameExpressionIfError)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c = if true then error else consume c end\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignTrySameExpressionAs)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "var x: (C | None) = None\n"
        "try\n"
        "  c = (x as C; consume c)\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignTry)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  let d = consume c\n"
        "  c = consume d\n"
        "  error\n"
        "end\n";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignSameExpressionPartial)
{
  const char* src =
    "class iso C\n"

    "primitive P\n"
      "fun apply(c: C): C iso^ ? => error\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = P(consume c) ?";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = consume c\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldReassignSameExpressionTryElse)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = try error else consume c end\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldReassignSameExpressionTryError)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = (try error else error end; consume c)";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignSameExpressionIfError)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = if true then error else consume c end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignSameExpressionAs)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var x: (C | None) = None\n"
        "c = (x as C; consume c)";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldReassign)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let d = consume c\n"
        "c = consume d";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest,
  ConsumeVarFieldReassignSameExpressionConsumedReferenceFunction)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = blah(consume c)\n"
        "error\n"

      "fun ref blah(d: C): C iso^ =>\n"
        "c = consume c\n"
        "consume d";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignSameExpressionFunctionReference)
{
  const char* src =
    "class iso C\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do2() => c\n"

      "fun ref do1(z: C): C iso^ =>\n"
        "do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c = do1(consume c)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest, ConsumeVarFieldReassignSameExpressionTraitFunctionReference)
{
  const char* src =
    "class iso C\n"

    "trait T\n"
      "fun ref do1(z: C): C iso^ => consume z\n"

    "class D is T\n"

    "actor Main\n"
      "var c: C\n"
      "var d: T\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = D\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = d.do1(consume c)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldReassignSameExpressionTraitConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"

    "trait T\n"
      "fun ref do1(z: C): C iso^ => consume z\n"

    "class D is T\n"

    "actor Main\n"
      "var c: C\n"
      "var d: D\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = D\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = d.do1(consume c)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeVarFieldReassignSameExpressionInterfaceFunctionReference)
{
  const char* src =
    "class iso C\n"

    "interface I\n"
      "fun ref do1(z: C): C iso^ => consume z\n"

    "class D is I\n"

    "actor Main\n"
      "var c: C\n"
      "var d: I\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = D\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = d.do1(consume c)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldReassignSameExpressionInterfaceConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"

    "interface I\n"
      "fun ref do1(z: C): C iso^ => consume z\n"

    "class D is I\n"

    "actor Main\n"
      "var c: C\n"
      "var d: D\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = D\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c = d.do1(consume c)\n"
        "error";

  TEST_COMPILE(src);
}









TEST_F(VerifyTest, ConsumeVarFieldEmbedSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "embed s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeVarFieldLetSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "let s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignTrySameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "var c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = P(consume c.s) ?\n"
        "end\n";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignTrySameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = consume c.s\n"
        "  error\n"
        "end\n";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignTrySameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = try error else consume c.s end\n"
        "  error\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignTrySameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = (try error else error end; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignTrySameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = if true then error else consume c.s end\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignTrySameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "var x: (C | None) = None\n"
        "try\n"
        "  c.s = (x as C; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignTry)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  let d = consume c.s\n"
        "  c.s = consume d\n"
        "  error\n"
        "end\n";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignSameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = P(consume c.s) ?";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = consume c.s\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignSameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = try error else consume c.s end\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignSameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = (try error else error end; consume c.s)";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignSameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = if true then error else consume c.s end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignSameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var x: (C | None) = None\n"
        "c.s = (x as C; consume c.s)";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassign)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let d = consume c.s\n"
        "c.s = consume d";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionConsumedReferenceFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = blah(consume c.s)\n"
        "error\n"

      "fun ref blah(d: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.s = consume c.s\n"
        "consume d";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do2() => c.s\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionFunctionReferenceSameObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do3()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionFunctionReferenceDifferentObjectFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.do3()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionFunctionReferenceDifferentObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.s = consume d.s\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionFunctionConsumeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c = consume c\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot consume parent object of a consumed field in function call tree.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionFunctionConsumeFakeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do2() =>\n"
        "var c = String\n"
        "c = consume c\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionFunctionConsumeFakeParentField)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarFieldVarSubfieldReassignSameExpressionFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: Array[U8] iso):\n"
      "  (Array[U8] iso^, Array[U8] iso^)\n"
      "=>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff() ? =>\n"
        "(c.s, d.s) = do1(consume c.s, consume d.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionTraitFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "var e: T\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionTraitConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionInterfaceFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "var e: I\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarFieldVarSubfieldReassignSameExpressionInterfaceConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "var c: C\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}




TEST_F(VerifyTest, ConsumeLetFieldEmbedSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "embed s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeLetFieldLetSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "let s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignTrySameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "let c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = P(consume c.s) ?\n"
        "end\n";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignTrySameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = consume c.s\n"
        "  error\n"
        "end\n";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignTrySameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = try error else consume c.s end\n"
        "  error\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignTrySameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = (try error else error end; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignTrySameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = if true then error else consume c.s end\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignTrySameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "var x: (C | None) = None\n"
        "try\n"
        "  c.s = (x as C; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignTry)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  let d = consume c.s\n"
        "  c.s = consume d\n"
        "  error\n"
        "end\n";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignSameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = P(consume c.s) ?";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = consume c.s\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignSameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = try error else consume c.s end\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignSameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = (try error else error end; consume c.s)";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignSameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = if true then error else consume c.s end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignSameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var x: (C | None) = None\n"
        "c.s = (x as C; consume c.s)";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassign)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let d = consume c.s\n"
        "c.s = consume d";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionConsumedReferenceFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = blah(consume c.s)\n"
        "error\n"

      "fun ref blah(d: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.s = consume c.s\n"
        "consume d";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do2() => c.s\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionFunctionReferenceSameObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "let c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do3()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionFunctionReferenceDifferentObjectFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.do3()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionFunctionReferenceDifferentObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.s = consume d.s\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionFunctionConsumeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c = consume c\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_3(src,
    "can't consume a let or embed field",
    "can't assign to a let or embed definition more than once",
    "left side must be something that can be assigned to");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionFunctionConsumeFakeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do2() =>\n"
        "var c = String\n"
        "c = consume c\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionFunctionConsumeFakeParentField)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetFieldVarSubfieldReassignSameExpressionFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "let c: C\n"
      "let d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: Array[U8] iso):\n"
      "  (Array[U8] iso^, Array[U8] iso^)\n"
      "=>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff() ? =>\n"
        "(c.s, d.s) = do1(consume c.s, consume d.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionTraitFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "var e: T\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionTraitConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionInterfaceFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "var e: I\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetFieldVarSubfieldReassignSameExpressionInterfaceConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "let c: C\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}





TEST_F(VerifyTest, ConsumeEmbedFieldEmbedSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "embed s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeEmbedFieldLetSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "let s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignTrySameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "embed c: C\n"

      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = P(consume c.s) ?\n"
        "end\n";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignTrySameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = consume c.s\n"
        "  error\n"
        "end\n";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignTrySameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = try error else consume c.s end\n"
        "  error\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignTrySameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = (try error else error end; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignTrySameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  c.s = if true then error else consume c.s end\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignTrySameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "var x: (C | None) = None\n"
        "try\n"
        "  c.s = (x as C; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignTry)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  let d = consume c.s\n"
        "  c.s = consume d\n"
        "  error\n"
        "end\n";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignSameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = P(consume c.s) ?";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = consume c.s\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignSameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = try error else consume c.s end\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignSameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = (try error else error end; consume c.s)";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignSameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = if true then error else consume c.s end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignSameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var x: (C | None) = None\n"
        "c.s = (x as C; consume c.s)";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassign)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let d = consume c.s\n"
        "c.s = consume d";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionConsumedReferenceFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = blah(consume c.s)\n"
        "error\n"

      "fun ref blah(d: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.s = consume c.s\n"
        "consume d";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do2() => c.s\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionFunctionReferenceSameObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "embed c: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do3()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionFunctionReferenceDifferentObjectFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.do3()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionFunctionReferenceDifferentObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.s = consume d.s\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionFunctionConsumeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c = consume c\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_3(src,
    "can't consume a let or embed field",
    "can't assign to a let or embed definition more than once",
    "left side must be something that can be assigned to");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionFunctionConsumeFakeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do2() =>\n"
        "var c = String\n"
        "c = consume c\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionFunctionConsumeFakeParentField)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "c.do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeEmbedFieldVarSubfieldReassignSameExpressionFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "embed c: C\n"
      "embed d: C\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: Array[U8] iso):\n"
      "  (Array[U8] iso^, Array[U8] iso^)\n"
      "=>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff() ? =>\n"
        "(c.s, d.s) = do1(consume c.s, consume d.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionTraitFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "var e: T\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionTraitConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionInterfaceFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "var e: I\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeEmbedFieldVarSubfieldReassignSameExpressionInterfaceConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "embed c: C\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "c = C\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}





TEST_F(VerifyTest, ConsumeVarLocalEmbedSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "embed s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "var c = C\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeVarLocalLetSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "let s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "var c = C\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignTrySameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c.s = P(consume c.s) ?\n"
        "end\n";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignTrySameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c.s = consume c.s\n"
        "  error\n"
        "end\n";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignTrySameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c.s = try error else consume c.s end\n"
        "  error\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignTrySameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c.s = (try error else error end; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignTrySameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  c.s = if true then error else consume c.s end\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignTrySameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "var x: (C | None) = None\n"
        "try\n"
        "  c.s = (x as C; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignTry)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "var c = C\n"
        "try\n"
        "  let d = consume c.s\n"
        "  c.s = consume d\n"
        "  error\n"
        "end\n";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignSameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = P(consume c.s) ?";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = consume c.s\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignSameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = try error else consume c.s end\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignSameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = (try error else error end; consume c.s)";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignSameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = if true then error else consume c.s end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignSameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "var x: (C | None) = None\n"
        "c.s = (x as C; consume c.s)";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassign)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "let d = consume c.s\n"
        "c.s = consume d";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionConsumedReferenceFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = blah(consume c.s)\n"
        "error\n"

      "fun ref blah(d: Array[U8] iso): Array[U8] iso^ =>\n"
        "var c = C\n"
        "c.s = consume c.s\n"
        "consume d";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do2() =>\n"
        "var c = C\n"
        "c.s\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionFunctionReferenceSameObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = do1(consume c.s, c.do3())\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionFunctionReferenceDifferentObjectFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.do3()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionFunctionReferenceDifferentObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.s = consume d.s\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionFunctionConsumeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: C): (Array[U8] iso^, C iso^) =>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = do1(consume c.s, consume c)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionFunctionConsumeFakeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do2() =>\n"
        "var c = recover iso Array[U8] end\n"
        "c = consume c\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = do1(consume c.s, c.do2())\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionFunctionConsumeFakeParentField)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = do1(consume c.s, c.do2())\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeVarLocalVarSubfieldReassignSameExpressionFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: Array[U8] iso):\n"
      "  (Array[U8] iso^, Array[U8] iso^)\n"
      "=>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "var d = C\n"
        "(c.s, d.s) = do1(consume c.s, consume d.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionTraitFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "var d: C\n"
      "var e: T\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionTraitConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionInterfaceFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "var d: C\n"
      "var e: I\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeVarLocalVarSubfieldReassignSameExpressionInterfaceConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "var c = C\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}





TEST_F(VerifyTest, ConsumeLetLocalEmbedSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "embed s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "let c = C\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeLetLocalLetSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "let s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff()\n"

      "fun ref dostuff() =>\n"
        "let c = C\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignTrySameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  c.s = P(consume c.s) ?\n"
        "end\n";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignTrySameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  c.s = consume c.s\n"
        "  error\n"
        "end\n";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignTrySameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  c.s = try error else consume c.s end\n"
        "  error\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignTrySameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  c.s = (try error else error end; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignTrySameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  c.s = if true then error else consume c.s end\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignTrySameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "var x: (C | None) = None\n"
        "try\n"
        "  c.s = (x as C; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignTry)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let c = C\n"
        "try\n"
        "  let d = consume c.s\n"
        "  c.s = consume d\n"
        "  error\n"
        "end\n";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignSameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = P(consume c.s) ?";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = consume c.s\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignSameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = try error else consume c.s end\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignSameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = (try error else error end; consume c.s)";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignSameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = if true then error else consume c.s end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignSameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "var x: (C | None) = None\n"
        "c.s = (x as C; consume c.s)";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassign)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "let d = consume c.s\n"
        "c.s = consume d";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionConsumedReferenceFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = blah(consume c.s)\n"
        "error\n"

      "fun ref blah(d: Array[U8] iso): Array[U8] iso^ =>\n"
        "let c = C\n"
        "c.s = consume c.s\n"
        "consume d";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do2() =>\n"
        "let c = C\n"
        "c.s\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "do2()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionFunctionReferenceSameObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = do1(consume c.s, c.do3())\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionFunctionReferenceDifferentObjectFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.do3()\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionFunctionReferenceDifferentObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.s = consume d.s\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionFunctionConsumeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: C): (Array[U8] iso^, C iso^) =>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = do1(consume c.s, consume c)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionFunctionConsumeFakeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do2() =>\n"
        "var c = recover iso Array[U8] end\n"
        "c = consume c\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = do1(consume c.s, c.do2())\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionFunctionConsumeFakeParentField)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = do1(consume c.s, c.do2())\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeLetLocalVarSubfieldReassignSameExpressionFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: Array[U8] iso):\n"
      "  (Array[U8] iso^, Array[U8] iso^)\n"
      "=>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "var d = C\n"
        "(c.s, d.s) = do1(consume c.s, consume d.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionTraitFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "var d: C\n"
      "var e: T\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionTraitConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionInterfaceFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "var d: C\n"
      "var e: I\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeLetLocalVarSubfieldReassignSameExpressionInterfaceConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff()?\n"
        "end\n"

      "fun ref dostuff() ? =>\n"
        "let c = C\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}









TEST_F(VerifyTest, ConsumeParamLocalEmbedSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "embed s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeParamLocalLetSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "let s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "c.s = consume c.s";

  TEST_ERRORS_1(src,
    "can't consume let or embed fields");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignTrySameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "try\n"
        "  c.s = P(consume c.s) ?\n"
        "end\n";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignTrySameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "try\n"
        "  c.s = consume c.s\n"
        "  error\n"
        "end\n";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignTrySameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "try\n"
        "  c.s = try error else consume c.s end\n"
        "  error\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignTrySameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "try\n"
        "  c.s = (try error else error end; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignTrySameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "try\n"
        "  c.s = if true then error else consume c.s end\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignTrySameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "var x: (C | None) = None\n"
        "try\n"
        "  c.s = (x as C; consume c.s)\n"
        "end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignTry)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "dostuff(C)\n"

      "fun ref dostuff(c: C) =>\n"
        "try\n"
        "  let d = consume c.s\n"
        "  c.s = consume d\n"
        "  error\n"
        "end\n";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignSameExpressionPartial)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "primitive P\n"
      "fun apply(a: Array[U8] iso): Array[U8] iso^ ? => error\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = P(consume c.s) ?";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignSameExpression)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = consume c.s\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignSameExpressionTryElse)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = try error else consume c.s end\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignSameExpressionTryError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = (try error else error end; consume c.s)";

  TEST_ERRORS_1(src, "unreachable code");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignSameExpressionIfError)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = if true then error else consume c.s end";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignSameExpressionAs)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "var x: (C | None) = None\n"
        "c.s = (x as C; consume c.s)";

  TEST_ERRORS_1(src,
    "can't reassign to a consumed field in an expression if there is a partial"
    " call involved");
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassign)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "let d = consume c.s\n"
        "c.s = consume d";

  TEST_ERRORS_1(src,
    "consuming a field is only allowed if it is reassigned in the same"
    " expression");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionConsumedReferenceFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = blah(consume c.s)\n"
        "error\n"

      "fun ref blah(d: Array[U8] iso): Array[U8] iso^ =>\n"
        "let c = C\n"
        "c.s = consume c.s\n"
        "consume d";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref do2() =>\n"
        "let c = C\n"
        "c.s\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "do2()\n"
        "consume z\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionFunctionReferenceSameObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = do1(consume c.s, c.do3())\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionFunctionReferenceDifferentObjectFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.do3()\n"
        "consume z\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionFunctionReferenceDifferentObject)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do3() => s\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso): Array[U8] iso^ =>\n"
        "d.s = consume d.s\n"
        "consume z\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionFunctionConsumeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: C): (Array[U8] iso^, C iso^) =>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = do1(consume c.s, consume c)\n"
        "error";

  TEST_ERRORS_1(src,
    "can't use a consumed local or field in an expression");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionFunctionConsumeFakeParent)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "new iso create() => s = recover s.create() end\n"
      "fun ref do2() =>\n"
        "var c = recover iso Array[U8] end\n"
        "c = consume c\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = do1(consume c.s, c.do2())\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionFunctionConsumeFakeParentField)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "var d: C\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: None): Array[U8] iso^ =>\n"
        "consume z\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = do1(consume c.s, c.do2())\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ConsumeParamLocalVarSubfieldReassignSameExpressionFunction)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"
      "fun ref do2() =>\n"
        "c = consume c\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref do1(z: Array[U8] iso, y: Array[U8] iso):\n"
      "  (Array[U8] iso^, Array[U8] iso^)\n"
      "=>\n"
        "(consume z, consume y)\n"

      "fun ref dostuff(c: C) ? =>\n"
        "var d = C\n"
        "(c.s, d.s) = do1(consume c.s, consume d.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot reference consumed field or a field of the same type after consume"
    " or in any function called.");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionTraitFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "var d: C\n"
      "var e: T\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionTraitConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "trait T\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is T\n"

    "actor Main\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionInterfaceFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "var d: C\n"
      "var e: I\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_ERRORS_1(src,
    "Cannot call functions on traits or interfaces after field consume or in"
    " any function called.");
}

TEST_F(VerifyTest,
  ConsumeParamLocalVarSubfieldReassignSameExpressionInterfaceConcreteTypeFunctionReference)
{
  const char* src =
    "class iso C\n"
      "var s: Array[U8] iso\n"
      "var c: Array[U8] iso\n"
      "new iso create() =>\n"
        "s = recover s.create() end\n"
        "c = recover c.create() end\n"

    "interface I\n"
      "fun ref do1(z: Array[U8] iso): Array[U8] iso^\n"
      "=>\n"
        "consume z\n"

    "class E is I\n"

    "actor Main\n"
      "var d: C\n"
      "var e: E\n"
      "new create(env: Env) =>\n"
        "d = C\n"
        "e = E\n"
        "try\n"
        "  dostuff(C)?\n"
        "end\n"

      "fun ref dostuff(c: C) ? =>\n"
        "c.s = e.do1(consume c.s)\n"
        "error";

  TEST_COMPILE(src);
}
