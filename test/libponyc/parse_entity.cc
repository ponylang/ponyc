#include <gtest/gtest.h>
#include "util.h"


// Parsing tests regarding entities, methods, fields and use commamnds

#define TEST_ERROR(src) DO(test_error(src, "syntax"))
#define TEST_COMPILE(src) DO(test_compile(src, "syntax"))


class ParseEntityTest : public PassTest
{};


// Basic tests

TEST_F(ParseEntityTest, Rubbish)
{
  const char* src = "rubbish";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, Empty)
{
  const char* src = "";

  TEST_COMPILE(src);
}


// Actors

TEST_F(ParseEntityTest, ActorMinimal)
{
  const char* src = "actor Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ActorCApi)
{
  const char* src = "actor @ Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ActorMaximal)
{
  const char* src =
    "actor Foo[A] is T"
    "  \"Doc\""
    "  let f1:T1"
    "  let f2:T2 = 5"
    "  var f3:P3.T3"
    //"  var f4:T4 delegate T5 = 9"
    "  var f4:T4 = 9"
    "  new m1() => 1"
    "  be m2() => 2"
    "  fun ref m3() => 3";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ActorCanBeCalledMain)
{
  const char* src = "actor Main";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ActorCannotSpecifyCapability)
{
  const char* src = "actor box Foo";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ActorCannotBeGenericAndCApi)
{
  const char* src = "actor @ Foo[A]";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ActorCannotBeDontcare)
{
  const char* src = "actor _";

  TEST_ERROR(src);
}


// Classes

TEST_F(ParseEntityTest, ClassMinimal)
{
  const char* src = "class Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ClassMaximal)
{
  const char* src =
    "class box Foo[A] is T"
    "  \"Doc\""
    "  let f1:T1"
    "  let f2:T2 = 5"
    "  var f3:P3.T3"
    //"  var f4:T4 delegate T5 = 9"
    "  var f4:T4 = 9"
    "  new m1() => 1"
    "  fun ref m2() => 2";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ClassCannotBeCalledMain)
{
  const char* src = "class Main";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ClassCannotHaveBehaviour)
{
  const char* src = "class Foo be m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ClassCannotBeCApi)
{
  const char* src = "class @ Foo";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ClassCannotBeDontcare)
{
  const char* src = "class _";

  TEST_ERROR(src);
}


// Primitives

TEST_F(ParseEntityTest, PrimitiveMinimal)
{
  const char* src = "primitive Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, PrimitiveMaximal)
{
  const char* src =
    "primitive Foo[A] is T"
    "  \"Doc\""
    "  new m1() => 1"
    "  fun ref m3() => 3";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, PrimitiveCannotBeCalledMain)
{
  const char* src = "primitive Main";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, PrimitiveCannotHaveFields)
{
  const char* src = "primitive Foo var x:U32";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, PrimitiveCannotSpecifyCapability)
{
  const char* src = "primitive box Foo";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, PrimitiveCannotHaveBehaviour)
{
  const char* src = "primitive Foo be m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, PrimitiveCannotBeCApi)
{
  const char* src = "primitive @ Foo";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, PrimitiveCannotBeDontcare)
{
  const char* src = "primitive _";

  TEST_ERROR(src);
}


// Traits

TEST_F(ParseEntityTest, TraitMinimal)
{
  const char* src = "trait Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, TraitMaximal)
{
  const char* src =
    "trait box Foo[A] is T"
    "  \"Doc\""
    "  be m1() => 1"
    "  fun ref m2() => 2";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, TraitCannotBeCalledMain)
{
  const char* src = "trait Main";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, TraitCannotHaveFields)
{
  const char* src = "trait Foo var x:U32";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, TraitCannotHaveConstructor)
{
  const char* src = "trait Foo new m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, TraitCannotBeCApi)
{
  const char* src = "trait @ Foo";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, TraitCannotBeDontcare)
{
  const char* src = "trait _";

  TEST_ERROR(src);
}


// Interfaces

TEST_F(ParseEntityTest, InterfaceMinimal)
{
  const char* src = "interface Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, InterfaceMaximal)
{
  const char* src =
    "interface box Foo[A] is T"
    "  \"Doc\""
    "  be m1() => 1"
    "  fun ref m2() => 2";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, InterfaceCannotBeCalledMain)
{
  const char* src = "interface Main";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, InterfaceCannotHaveFields)
{
  const char* src = "interface Foo var x:U32";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, InterfaceCannotHaveConstructor)
{
  const char* src = "interface Foo new m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, InterfaceCannotBeCApi)
{
  const char* src = "interface @ Foo";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, InterfaceCannotBeDontcare)
{
  const char* src = "interface _";

  TEST_ERROR(src);
}


// Aliases

TEST_F(ParseEntityTest, Alias)
{
  const char* src = "type Foo is Bar";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, AliasMustHaveType)
{
  const char* src = "type Foo";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, AliasCannotBeDontcare)
{
  const char* src = "type _ is Foo";

  TEST_ERROR(src);
}


// Functions

TEST_F(ParseEntityTest, FunctionMinimal)
{
  const char* src = "class Foo fun ref apply() => 3";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, FunctionMaximal)
{
  const char* src = "actor Foo fun ref m[A](y:U32):I16 ? => 3";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, FunctionCannotHaveEllipsis)
{
  const char* src = "class Foo ref fun m(x:U32, ...) => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, TraitFunctionBodiesAreOptional)
{
  const char* src = "trait Foo fun ref m()";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, InterfaceFunctionBodiesAreOptional)
{
  const char* src = "interface Foo fun ref m()";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ClassFunctionMustHaveBody)
{
  const char* src = "class Foo fun ref m()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ClassFunctionCannotBeCCallable)
{
  const char* src = "class Foo fun ref @m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, PrimitiveFunctionCannotBeCCallable)
{
  const char* src = "primitive Foo fun ref @m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, TraitFunctionCannotBeCCallable)
{
  const char* src = "trait Foo fun ref @m()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, InterfaceFunctionCannotBeCCallable)
{
  const char* src = "interface Foo fun ref @m()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FunctionCannotBeDontcare)
{
  const char* src = "class Foo fun _() => 3";

  TEST_ERROR(src);
}


// Behaviours

TEST_F(ParseEntityTest, Behaviour)
{
  const char* src = "actor Foo be m() => 3";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, BehaviourCannotHaveCapability)
{
  const char* src = "actor Foo be ref m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, BehaviourCannotHaveEllipsis)
{
  const char* src = "actor Foo be m(x:U32, ...) => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, BehaviourCannotHaveReturnType)
{
  const char* src = "actor Foo be m():U32 => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, BehaviourCannotBePartial)
{
  const char* src = "actor Foo be m() ? => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ActorBehaviourMustHaveBody)
{
  const char* src = "actor Foo be m()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, TraitBehaviourBodiesAreOptional)
{
  const char* src = "trait Foo be m()";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, InterfaceBehaviourBodiesAreOptional)
{
  const char* src = "interface Foo be m()";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, TraitBehaviourCannotBeCCallable)
{
  const char* src = "trait Foo be ref @m()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, InterfaceBehaviourCannotBeCCallable)
{
  const char* src = "interface Foo be ref @m()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, BehaviourCannotBeDontcare)
{
  const char* src = "actor Foo be _() => 3";

  TEST_ERROR(src);
}


// Constructors

TEST_F(ParseEntityTest, ConstructorMinimal)
{
  const char* src = "class Foo new create() => 3";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ConstructorMaximal)
{
  const char* src = "class Foo new m[A](y:U32) ? => 3";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, PrimitiveConstructorCannotHaveCapability)
{
  const char* src = "primitive Foo new val m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ActorConstructorCannotHaveCapability)
{
  const char* src = "actor Foo new tag m() => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ConstructorCannotHaveEllipsis)
{
  const char* src = "class Foo new m(x:U32, ...) => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ConstructorCannotHaveReturnType)
{
  const char* src = "class Foo new m():U32 => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ActorConstructorCannotBePartial)
{
  const char* src = "actor Foo new m() ? => 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ConstructorMustHaveBody)
{
  const char* src = "class Foo new m()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ConstructorCannotBeDontcare)
{
  const char* src = "class Foo new _() => 3";

  TEST_ERROR(src);
}


// Double arrow

TEST_F(ParseEntityTest, DoubleArrowMissing)
{
  const char* src = "class Foo fun ref f() 1";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, DoubleArrowWithoutBody)
{
  const char* src = "class Foo fun ref f() =>";

  TEST_ERROR(src);
}


// Field tests

TEST_F(ParseEntityTest, FieldMustHaveType)
{
  const char* src = "class Foo var bar";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, LetFieldMustHaveType)
{
  const char* src = "class Foo let bar";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FieldDelegate)
{
  const char* src =
    "class Foo\n"
    "  var bar: T delegate T1";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, FieldDelegateCanBeIntersect)
{
  const char* src =
    "trait T\n"
    "trait T1\n"
    "trait T2\n"

    "class Foo\n"
    "  var bar: T delegate (T1 & T2)";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, FieldDelegateCannotBeUnion)
{
  const char* src =
    "trait T\n"
    "trait T1\n"
    "trait T2\n"

    "class Foo\n"
    "  var bar: T delegate (T1 | T2)";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FieldDelegateCannotBeTuple)
{
  const char* src =
    "trait T\n"
    "trait T1\n"
    "trait T2\n"

    "class Foo\n"
    "  var bar: T delegate (T1, T2)";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FieldDelegateCannotBeArrow)
{
  const char* src =
    "trait T\n"
    "trait T1\n"
    "trait T2\n"

    "class Foo\n"
    "  var bar: T delegate T1->T2";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FieldDelegateCannotHaveCapability)
{
  const char* src =
    "trait T\n"
    "trait T1\n"

    "class Foo\n"
    "  var bar: T delegate T1 ref";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FieldCannotBeDontcare)
{
  const char* src =
    "class Foo let _: None";

  TEST_ERROR(src);
}


// Provides list tests

TEST_F(ParseEntityTest, ProvidesListCanBeIntersect)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"

    "class Foo is (T1 & T2)";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ProvidesListCannotBeUnion)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"

    "class Foo is (T1 | T2)";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ProvidesListCannotBeTuple)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"

    "class Foo is (T1, T2)";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ProvidesListCannotBeArrow)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"

    "class Foo is T1->T2";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, ProvidesListCannotHaveCapability)
{
  const char* src =
    "trait T1\n"

    "class Foo is T1 ref";

  TEST_ERROR(src);
}


// Use command

TEST_F(ParseEntityTest, UseUri)
{
  const char* src =
    "use \"foo1\"\n"
    "use bar = \"foo2\"\n"
    "use \"foo3\" if debug\n"
    "use bar = \"foo4\" if debug";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, UseFfi)
{
  const char* src =
    "use @foo1[U32](a:I32, b:String, ...) ?\n"
    "use @foo2[U32]() if debug";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, UseMustBeBeforeClass)
{
  const char* src = "class Foo use \"foo\"";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, UseFfiMustHaveReturnType)
{
  const char* src = "use @foo()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, UseFfiMustHaveOnlyOneReturnType)
{
  const char* src = "use @foo[U8, U16]()";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, UseFfiCannotHaveDefaultArguments)
{
  const char* src = "use @foo[U32](x:U32 = 3)";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, UseFfiEllipsisMustBeLast)
{
  const char* src = "use @foo[U32](a:U32, ..., b:U32)";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, UseGuardsAreExpressions)
{
  const char* src = "use \"foo\" if linux and debug";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, UseGuardsDoNotAllowOperatorPrecedence)
{
  const char* src1 = "use \"foo\" if (linux and debug) or windows";

  TEST_COMPILE(src1);

  const char* src2 = "use \"foo\" if linux and debug or windows";

  TEST_ERROR(src2);
}


TEST_F(ParseEntityTest, UseGuardsUserAndPlatformFlagsCanCollide)
{
  const char* src1 = "use \"foo\" if \"linuxfoo\"";

  TEST_COMPILE(src1);

  const char* src2 = "use \"foo\" if \"linux\"";

  TEST_ERROR(src2);

  const char* src3 = "use \"foo\" if \"LINUX\"";

  TEST_ERROR(src3);
}


TEST_F(ParseEntityTest, UseGuardsOutlawedFlagName)
{
  const char* src1 = "use \"foo\" if \"ndebug\"";

  TEST_ERROR(src1);

  const char* src2 = "use \"foo\" if \"NDEBUG\"";

  TEST_ERROR(src2);
}


TEST_F(ParseEntityTest, UserFlagInCondition)
{
  const char* src =
    "use \"foo\" if \"wombat\"";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, UnquotedUserFlagInCondition)
{
  const char* src =
    "use \"foo\" if wombat";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, OsNameCaseSensitiveInConditionRight)
{
  const char* src =
    "use \"foo\" if windows";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, OsNameCaseSensitiveInConditionWrong)
{
  const char* src =
    "use \"foo\" if WINDOWS";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, UseUriCannotBeDontcare)
{
  const char* src =
    "use _ = \"foo\"";

  TEST_ERROR(src);
}


// Compile intrinsic

TEST_F(ParseEntityTest, CompileIntrinsicAllowedAsFunctionBody)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    compile_intrinsic";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, DocstringAllowedBeforeCompileIntrinsic)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    \"We do something\"\n"
    "    compile_intrinsic";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, TwoDocstringsNotAllowedBeforeCompileIntrinsic)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    \"We do something\"\n"
    "    \"And something else\"\n"
    "    compile_intrinsic";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, NonDocExpressionNotAllowedBeforeCompileIntrinsic)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    let x = 3\n"
    "    compile_intrinsic";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, DocstringNotAllowedAfterCompileIntrinsic)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    compile_intrinsic\n"
    "    \"We do something\"";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, NonDocExpressionNotAllowedAfterCompileIntrinsic)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    compile_intrinsic\n"
    "    let x = 3";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, CompileIntrinsicNotAllowedInControlStructure)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    if x then\n"
    "      compile_intrinsic\n"
    "    end";

  TEST_ERROR(src);
}
