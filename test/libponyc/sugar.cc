#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "sugar"))
#define TEST_ERROR(src) DO(test_error(src, "sugar"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "sugar", expect, "parse"))


class SugarTest: public PassTest
{};


TEST_F(SugarTest, DataType)
{
  const char* short_form =
    "primitive Foo";

  const char* full_form =
    "primitive Foo val\n"
    "  new val create(): Foo val^ => true\n"
    "  fun box eq(that:Foo): Bool => this is that\n"
    "  fun box ne(that:Foo): Bool => this isnt that\n";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithField)
{
  // Create constructor should not be added if there are uninitialsed fields
  const char* short_form =
    "class Foo iso let m:U32";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, ClassWithInitialisedField)
{
  // Create constructor should be added if there are only initialised fields
  const char* short_form =
    "class Foo iso let m:U32 = 3";

  const char* full_form =
    "class Foo iso let m:U32 = 3 new ref create(): Foo ref^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithCreateConstructor)
{
  // Create constructor should not be added if it's already there
  const char* short_form =
    "class Foo iso new create() => 3";

  const char* full_form =
    "class Foo iso new ref create(): Foo ref^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithCreateFunction)
{
  // Create function doesn't clash with create constructor
  const char* short_form =
    "class Foo fun ref create():U32 => 3";

  const char* full_form =
    "class Foo ref fun ref create():U32 => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithoutFieldOrCreate)
{
  const char* short_form =
    "class Foo iso";

  const char* full_form =
    "class Foo iso new ref create(): Foo ref^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithoutDefCap)
{
  const char* short_form = "class Foo     let m:U32";
  const char* full_form =  "class Foo ref let m:U32";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithField)
{
  // Create constructor should not be added if there are uninitialsed fields
  const char* short_form = "actor Foo     let m:U32";
  const char* full_form =  "actor Foo tag let m:U32";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithInitialisedField)
{
  // Create constructor should be added if there are only initialsed fields
  const char* short_form =
    "actor Foo let m:U32 = 3";

  const char* full_form =
    "actor Foo tag let m:U32 = 3 new tag create(): Foo tag^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithCreateConstructor)
{
  // Create constructor should not be added if it's already there
  const char* short_form =
    "actor Foo new create() => 3";

  const char* full_form =
    "actor Foo tag new tag create(): Foo tag^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithCreateFunction)
{
  // Create function doesn't clash with create constructor
  const char* short_form =
    "actor Foo fun ref create():U32 => 3";

  const char* full_form =
    "actor Foo tag fun ref create():U32 => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithCreateBehaviour)
{
  // Create behaviour doesn't clash with create constructor
  const char* short_form =
    "actor Foo be create() => 3";

  const char* full_form =
    "actor Foo tag be tag create():Foo tag => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithoutFieldOrCreate)
{
  const char* short_form =
    "actor Foo";

  const char* full_form =
    "actor Foo tag new tag create(): Foo tag^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithoutDefCap)
{
  const char* short_form = "actor Foo     let m:U32";
  const char* full_form =  "actor Foo tag let m:U32";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, TraitWithCap)
{
  const char* short_form = "trait Foo box";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, TraitWithoutCap)
{
  const char* short_form = "trait Foo";
  const char* full_form =  "trait Foo ref";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, TypeParamWithConstraint)
{
  const char* short_form = "class Foo[A: U32] ref var y:U32";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, TypeParamWithoutConstraint)
{
  const char* short_form = "class Foo[A]    ref var y:U32";
  const char* full_form =  "class Foo[A: A] ref var y:U32";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ConstructorNoReturnType)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  new create() => 3";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  new ref create(): Foo ref^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ConstructorInActor)
{
  const char* short_form =
    "actor Foo var y:U32\n"
    "  new create() => 3";

  const char* full_form =
    "actor Foo tag var y:U32\n"
    "  new tag create(): Foo tag^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ConstructorInDataType)
{
  const char* short_form =
    "primitive Foo\n"
    "  new create() => 3";

  const char* full_form =
    "primitive Foo val\n"
    "  new val create(): Foo val^ => 3\n"
    "  fun box eq(that:Foo): Bool => this is that\n"
    "  fun box ne(that:Foo): Bool => this isnt that\n";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ConstructorInGenericClass)
{
  const char* short_form =
    "class Foo[A: B, C] ref var y:U32\n"
    "  new bar() => 3";

  const char* full_form =
    "class Foo[A: B, C: C] ref var y:U32\n"
    "  new ref bar(): Foo[A, C] ref^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, BehaviourReturnType)
{
  const char* short_form = "actor Foo     var y:U32 be foo()         => 3";
  const char* full_form =  "actor Foo tag var y:U32 be tag foo():Foo tag => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, FunctionComplete)
{
  const char* short_form =
    "class Foo ref var y:U32 fun box foo(): U32 val => 3";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, FunctionNoReturnNoBody)
{
  const char* short_form =
    "trait Foo ref fun box foo()";

  const char* full_form =
    "trait Foo ref fun box foo(): None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, FunctionNoReturnBody)
{
  const char* short_form =
    "trait Foo ref fun box foo() => 3";

  const char* full_form =
    "trait Foo ref fun box foo(): None => 3\n"
    "  None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, IfWithoutElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  if 1 then 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  if 1 then 2 else None end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, IfWithElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  if 1 then 2 else 3 end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, WhileWithoutElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  while 1 do 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  while 1 do 2 else None end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, WhileWithElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  while 1 do 2 else 3 end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, TryWithElseAndThen)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else 2 then 3 end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, TryWithoutElseOrThen)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else None then None end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, TryWithoutElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 then 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  $try_no_check 1 else None then 2 end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, TryWithoutThen)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else 2 then None end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ForWithoutElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  for i in 1 do 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  $seq(\n"
    "    let hygid = $seq(1)\n"
    "    while hygid.has_next() do\n"
    "      let i = $try_no_check\n"
    "        hygid.next()\n"
    "      else\n"
    "        continue\n"
    "      then\n"
    "        None\n"
    "      end\n"
    "      $seq(2)\n"
    "    else None end\n"
    "  )";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ForWithElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  for i in 1 do 2 else 3 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  $seq(\n"
    "    let hygid = $seq(1)\n"
    "    while hygid.has_next() do\n"
    "      let i = $try_no_check\n"
    "        hygid.next()\n"
    "      else\n"
    "        continue\n"
    "      then\n"
    "        None\n"
    "      end\n"
    "      $seq(2)\n"
    "    else 3 end\n"
    "  )";

  TEST_EQUIV(short_form, full_form);
}


// TODO(andy): Tests for sugar_bang, once that's done


TEST_F(SugarTest, CaseWithBody)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1 => 2\n"
    "  else\n"
    "    3\n"
    "  end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, CaseWithBodyAndFollowingCase)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1 => 2\n"
    "  |3 => 4\n"
    "  else\n"
    "    5\n"
    "  end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, CaseWithNoBody)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1\n"
    "  |2 => 3\n"
    "  else\n"
    "    4\n"
    "  end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1 => 3\n"
    "  |2 => 3\n"
    "  else\n"
    "    4\n"
    "  end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseWithNoBodyMultiple)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1\n"
    "  |2\n"
    "  |3\n"
    "  |4 => 5\n"
    "  else\n"
    "    6\n"
    "  end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1 => 5\n"
    "  |2 => 5\n"
    "  |3 => 5\n"
    "  |4 => 5\n"
    "  else\n"
    "    6\n"
    "  end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, MatchWithNoElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1=> 2\n"
    "  end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1 => 2\n"
    "  else\n"
    "    None\n"
    "  end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, UpdateLhsNotCall)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo = 1";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, UpdateNoArgs)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo() = 1";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo.update(where value $updatearg = 1)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, UpdateWithArgs)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo(2, 3 where bar = 4) = 1";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo.update(2, 3 where bar = 4, value $updatearg = 1)";

  TEST_EQUIV(short_form, full_form);
}


// Operator subsituation

TEST_F(SugarTest, Add)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 + 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.add(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Sub)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 - 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.sub(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Multiply)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 * 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.mul(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Divide)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 / 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.div(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Mod)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 % 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.mod(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, UnaryMinus)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => -1";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.neg()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ShiftLeft)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 << 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.shl(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ShiftRight)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 >> 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.shr(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, And)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 and 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.op_and(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Or)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 or 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.op_or(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Xor)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 xor 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.op_xor(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Not)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => not 1";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.op_not()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Eq)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 == 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.eq(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Ne)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 != 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.ne(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Lt)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 < 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.lt(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Le)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 <= 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.le(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Gt)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 > 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.gt(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Ge)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 >= 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.ge(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, As)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo | Bar)): Foo ? => a as Foo ref";

  const char* full_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo | Bar)): Foo ? =>\n"
    "    match a\n"
    "    | let hygid: Foo ref => consume $borrowed hygid\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, AsTuple)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar)): (Foo, Bar) ? => a as (Foo ref, Bar ref)";

  const char* full_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar)): (Foo, Bar) ? =>\n"
    "    match a\n"
    "    | (let hygid: Foo ref, let hygid: Bar ref) =>\n"
    "      (consume $borrowed hygid, consume $borrowed hygid)\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, AsNestedTuple)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, (Bar, Baz))): (Foo, (Bar, Baz)) ? =>\n"
    "    a as (Foo ref, (Bar ref, Baz ref))";

  const char* full_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, (Bar, Baz))): (Foo, (Bar, Baz)) ? =>\n"
    "    match a\n"
    "    | (let hygid: Foo ref, (let hygid: Bar ref, let hygid: Baz ref)) =>\n"
    "      (consume $borrowed hygid,\n"
    "        (consume $borrowed hygid, consume $borrowed hygid))\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, AsDontCare)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: Foo): Foo ? => a as (_)";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, AsDontCare2Tuple)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar)): Foo ? => a as (Foo ref, _)";

  const char* full_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar)): Foo ? =>\n"
    "    match a\n"
    "    | (let hygid: Foo ref, _) =>\n"
    "      consume $borrowed hygid\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, AsDontCareMultiTuple)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar, Baz)): (Foo, Baz) ? =>\n"
    "    a as (Foo ref, _, Baz ref)";

  const char* full_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar, Baz)): (Foo, Baz) ? =>\n"
    "    match a\n"
    "    | (let hygid: Foo ref, _, let hygid: Baz ref) =>\n"
    "      (consume $borrowed hygid, consume $borrowed hygid)\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  TEST_EQUIV(short_form, full_form);
}
