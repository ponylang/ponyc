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
    "  var f4:T4 delegate T5 = 9"
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



// Classes

TEST_F(ParseEntityTest, ClassMinimal)
{
  const char* src = "class Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, ClassMaximal)
{
  const char* src =
    "class Foo[A] box is T"
    "  \"Doc\""
    "  let f1:T1"
    "  let f2:T2 = 5"
    "  var f3:P3.T3"
    "  var f4:T4 delegate T5 = 9"
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


// Traits

TEST_F(ParseEntityTest, TraitMinimal)
{
  const char* src = "trait Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, TraitMaximal)
{
  const char* src =
    "trait Foo[A] box is T"
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


// Interfaces

TEST_F(ParseEntityTest, InterfaceMinimal)
{
  const char* src = "interface Foo";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, InterfaceMaximal)
{
  const char* src =
    "interface Foo[A] box is T"
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
    "class Foo\n"
    "  var bar: T delegate (T1 & T2)";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, FieldDelegateCannotBeUnion)
{
  const char* src =
    "class Foo\n"
    "  var bar: T delegate (T1 | T2)";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FieldDelegateCannotBeTuple)
{
  const char* src =
    "class Foo\n"
    "  var bar: T delegate (T1, T2)";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FieldDelegateCannotBeArrow)
{
  const char* src =
    "class Foo\n"
    "  var bar: T delegate T1->T2";

  TEST_ERROR(src);
}


TEST_F(ParseEntityTest, FieldDelegateCannotHaveCapability)
{
  const char* src =
    "class Foo\n"
    "  var bar: T delegate T1 ref";

  TEST_ERROR(src);
}


// Use command

TEST_F(ParseEntityTest, UseUri)
{
  const char* src =
    "use \"foo1\" "
    "use bar = \"foo2\" "
    "use \"foo3\" if wombat "
    "use bar = \"foo4\" if wombat";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, UseFfi)
{
  const char* src =
    "use @foo1[U32](a:I32, b:String, ...) ?"
    "use @foo2[U32]() if wombat";

  TEST_COMPILE(src);
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
  const char* src = "use \"foo\" if a and b";

  TEST_COMPILE(src);
}


TEST_F(ParseEntityTest, UseGuardsDoNotAllowOperatorPrecedence)
{
  const char* src1 = "use \"foo\" if (a and b) or c";

  TEST_COMPILE(src1);

  const char* src2 = "use \"foo\" if a and b or c";

  TEST_ERROR(src2);
}


TEST_F(ParseEntityTest, UseMustBeBeforeClass)
{
  const char* src = "class Foo use \"foo\"";

  TEST_ERROR(src);
}
