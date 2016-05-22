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
    "use \"builtin\"\n"
    "primitive val Foo\n"
    "  new val create(): Foo val^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithoutCreate)
{
  // Create constructor should be added if there isn't a create
  const char* short_form =
    "class iso Foo\n"
    "  let m:U32 = 3";

  const char* full_form =
    "use \"builtin\"\n"
    "class iso Foo\n"
    "  let m:U32\n"
    "  new iso create(): Foo iso^ =>\n"
    "    m = 3\n"
    "    true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithCreateConstructor)
{
  // Create constructor should not be added if it's already there
  const char* short_form =
    "class iso Foo\n"
    "  new create() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "class iso Foo\n"
    "  new ref create(): Foo ref^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithConstructor)
{
  // Create constructor should not be added if there's already a constructor
  const char* short_form =
    "class iso Foo\n"
    "  new make() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "class iso Foo\n"
    "  new ref make(): Foo ref^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithCreateFunction)
{
  // Create function doesn't clash with create constructor
  const char* short_form =
    "class Foo\n"
    "  fun ref create():U32 => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  fun ref create():U32 => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithoutFieldOrCreate)
{
  const char* short_form =
    "class iso Foo";

  const char* full_form =
    "use \"builtin\"\n"
    "class iso Foo\n"
    "  new iso create(): Foo iso^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ClassWithoutDefCap)
{
  const char* short_form =
    "class Foo";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  new iso create(): Foo iso^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithInitialisedField)
{
  // Create constructor should be added
  const char* short_form =
    "actor Foo";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  new tag create(): Foo tag^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithCreateConstructor)
{
  // Create constructor should not be added if it's already there
  const char* short_form =
    "actor Foo\n"
    " new create() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  new tag create(): Foo tag^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithConstructor)
{
  // Create constructor should not be added if there's already a constructor
  const char* short_form =
    "actor Foo\n"
    " new make() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  new tag make(): Foo tag^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithCreateFunction)
{
  // Create function doesn't clash with create constructor
  const char* short_form =
    "actor Foo\n"
    "  fun ref create():U32 => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  fun ref create():U32 => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithCreateBehaviour)
{
  // Create behaviour doesn't clash with create constructor
  const char* short_form =
    "actor Foo\n"
    "  be create() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  be tag create():Foo tag => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithoutFieldOrCreate)
{
  const char* short_form =
    "actor Foo";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  new tag create(): Foo tag^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ActorWithoutDefCap)
{
  const char* short_form =
    "actor Foo";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  new tag create(): Foo tag^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, TraitWithCap)
{
  const char* short_form = "trait box Foo";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, TraitWithoutCap)
{
  const char* short_form =
    "trait Foo";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, TypeParamWithConstraint)
{
  const char* short_form =
    "class ref Foo[A: U32]\n"
    "  var create: U32";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, TypeParamWithoutConstraint)
{
  const char* short_form =
    "class ref Foo[A]\n"
  "  var create: U32";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo[A: A]\n"
    "  var create: U32";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ConstructorNoReturnType)
{
  const char* short_form =
    "class ref Foo\n"
    "  new create() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  new ref create(): Foo ref^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ConstructorInActor)
{
  const char* short_form =
    "actor Foo\n"
    "  new create() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  new tag create(): Foo tag^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ConstructorInDataType)
{
  const char* short_form =
    "primitive Foo\n"
    "  new create() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "primitive val Foo\n"
    "  new val create(): Foo val^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ConstructorInGenericClass)
{
  const char* short_form =
    "class Foo[A: B, C]\n"
    "  new bar() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo[A: B, C: C]\n"
    "  new ref bar(): Foo[A, C] ref^ => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, BehaviourReturnType)
{
  const char* short_form =
    "actor Foo\n"
    "  var create: U32\n"
    "  be foo() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  var create: U32\n"
    "  be tag foo():Foo tag => 3";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, FunctionComplete)
{
  const char* short_form =
    "class Foo\n"
    "  fun box foo(): U32 val => 3";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, FunctionNoReturnNoBody)
{
  const char* short_form =
    "trait Foo\n"
    "  fun foo()";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo\n"
    "  fun box foo(): None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, FunctionNoReturnBody)
{
  const char* short_form =
    "trait Foo\n"
    "  fun foo() => 3";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo\n"
    "  fun box foo(): None => 3\n"
    "  None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, FunctionParamMustBeId)
{
  const char* good =
    "trait Foo\n"
    "  fun foo(x: U64) => 3";

  TEST_COMPILE(good);

  const char* bad1 =
    "trait Foo\n"
    "  fun foo(x.y(): U64) => 3";

  TEST_ERROR(bad1);

  const char* bad2 =
    "trait Foo\n"
    "  fun foo(_: U64) => 3";

  TEST_ERROR(bad2);

  const char* bad3 =
    "trait Foo\n"
    "  fun foo($: U64) => 3";

  TEST_ERROR(bad3);
}


TEST_F(SugarTest, FunctionParamTypeRequired)
{
  const char* good =
    "trait Foo\n"
    "  fun foo(x: U64) => 3";

  TEST_COMPILE(good);

  const char* bad =
    "trait Foo\n"
    "  fun foo(x) => 3";

  TEST_ERROR(bad);
}


TEST_F(SugarTest, FunctionGuardsNotAllowed)
{
  const char* good =
    "trait Foo\n"
    "  fun foo(x: U64) => 3";

  TEST_COMPILE(good);

  const char* bad =
    "trait Foo\n"
    "  fun foo(x: U64 where x > 0) => 3";

  TEST_ERROR(bad);
}


TEST_F(SugarTest, IfWithoutElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    if 1 then 2 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    if 1 then 2 else None end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, IfWithElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    if 1 then 2 else 3 end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, WhileWithoutElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    while 1 do 2 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    while 1 do 2 else None end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, WhileWithElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    while 1 do 2 else 3 end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, TryWithElseAndThen)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    try 1 else 2 then 3 end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, TryWithoutElseOrThen)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    try 1 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    try 1 else None then None end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, TryWithoutElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    try 1 then 2 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    $try_no_check 1 else None then 2 end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, TryWithoutThen)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    try 1 else 2 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    try 1 else 2 then None end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ForWithoutElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    for i in 1 do 2 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "  (\n"
    "    let $1 = (1)\n"
    "    while $1.has_next() do\n"
    "      let i = $try_no_check\n"
    "        $1.next()\n"
    "      else\n"
    "        continue\n"
    "      then\n"
    "        None\n"
    "      end\n"
    "      (2)\n"
    "    else None end\n"
    "  )";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ForWithElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    for i in 1 do 2 else 3 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "  (\n"
    "    let $1 = (1)\n"
    "    while $1.has_next() do\n"
    "      let i = $try_no_check\n"
    "        $1.next()\n"
    "      else\n"
    "        continue\n"
    "      then\n"
    "        None\n"
    "      end\n"
    "      (2)\n"
    "    else 3 end\n"
    "  )";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, MultiIteratorFor)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    for (i, j) in 1 do 2 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "  (\n"
    "    let $1 = (1)\n"
    "    while $1.has_next() do\n"
    "      (let i, let j) = $try_no_check\n"
    "        $1.next()\n"
    "      else\n"
    "        continue\n"
    "      then\n"
    "        None\n"
    "      end\n"
    "      (2)\n"
    "    else None end\n"
    "  )";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseWithBody)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(x)\n"
    "    |1 => 2\n"
    "    else\n"
    "      3\n"
    "    end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, CaseWithBodyAndFollowingCase)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(x)\n"
    "    |1 => 2\n"
    "    |3 => 4\n"
    "    else\n"
    "      5\n"
    "    end";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, CaseWithNoBody)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(x)\n"
    "    |1\n"
    "    |2 => 3\n"
    "    else\n"
    "      4\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    match(x)\n"
    "    |1 => 3\n"
    "    |2 => 3\n"
    "    else\n"
    "      4\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseWithNoBodyMultiple)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(x)\n"
    "    |1\n"
    "    |2\n"
    "    |3\n"
    "    |4 => 5\n"
    "    else\n"
    "      6\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    match(x)\n"
    "    |1 => 5\n"
    "    |2 => 5\n"
    "    |3 => 5\n"
    "    |4 => 5\n"
    "    else\n"
    "      6\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CasePatternCapture)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(a)\n"
    "    | let x: U32 => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    match(a)\n"
    "    | $let x: U32 => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CasePatternCaptureVar)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(a)\n"
    "    | var x: U32 => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CasePatternCaptureInTuple)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(a)\n"
    "    | (let x: U32, let y: U16) => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    match(a)\n"
    "    | ($let x: U32, $let y: U16) => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CasePatternCaptureInNestedTuple)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(a)\n"
    "    | (4, (let y: U16, _)) => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    match(a)\n"
    "    | (4, ($let y: U16, _)) => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CasePatternCaptureNoType)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(a)\n"
    "    | let x => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CasePatternCaptureTupleType)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(a)\n"
    "    | let x: (U32, U16) => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CasePatternNotCaptureInSequence)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(a)\n"
    "    | (4\n"
    "       let x: U32) => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    match(a)\n"
    "    | (4\n"
    "       let x: U32) => 1\n"
    "    else\n"
    "      2\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, MatchWithNoElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    match(x)\n"
    "    |1=> 2\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    match(x)\n"
    "    |1 => 2\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, UpdateLhsNotCall)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    foo = 1";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, UpdateNoArgs)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    foo() = 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    foo.update(where value $updatearg = 1)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, UpdateWithArgs)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    foo(2, 3 where bar = 4) = 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    foo.update(2, 3 where bar = 4, value $updatearg = 1)";

  TEST_EQUIV(short_form, full_form);
}


// Operator subsituation

TEST_F(SugarTest, Add)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 + 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.add(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Sub)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 - 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.sub(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Multiply)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 * 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.mul(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Divide)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 / 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.div(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Mod)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 % 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.mod(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, UnaryMinus)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    -1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.neg()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ShiftLeft)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 << 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.shl(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ShiftRight)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 >> 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.shr(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, And)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 and 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.op_and(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Or)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 or 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.op_or(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Xor)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 xor 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.op_xor(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Not)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    not 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.op_not()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Eq)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 == 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.eq(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Ne)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 != 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.ne(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Lt)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 < 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.lt(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Le)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 <= 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.le(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Gt)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 > 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.gt(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, Ge)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(): U32 val =>\n"
    "    1 >= 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): U32 val =>\n"
    "    1.ge(2)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, As)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo | Bar)): Foo ? =>\n"
    "    a as Foo ref";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo | Bar)): Foo ? =>\n"
    "    match a\n"
    "    | $let $1: Foo ref => consume $borrowed $1\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, AsTuple)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo, Bar)): (Foo, Bar) ? =>\n"
    "    a as (Foo ref, Bar ref)";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo, Bar)): (Foo, Bar) ? =>\n"
    "    match a\n"
    "    | ($let $1: Foo ref, $let $2: Bar ref) =>\n"
    "      (consume $borrowed $1, consume $borrowed $2)\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, AsNestedTuple)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo, (Bar, Baz))): (Foo, (Bar, Baz)) ? =>\n"
    "    a as (Foo ref, (Bar ref, Baz ref))";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo, (Bar, Baz))): (Foo, (Bar, Baz)) ? =>\n"
    "    match a\n"
    "    | ($let $1: Foo ref, ($let $2: Bar ref, $let $3: Baz ref)) =>\n"
    "      (consume $borrowed $1,\n"
    "        (consume $borrowed $2, consume $borrowed $3))\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, AsDontCare)
{
  const char* short_form =
    "class Foo\n"
    "  fun f(a: Foo): Foo ? =>\n"
    "    a as (_)";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, AsDontCare2Tuple)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo, Bar)): Foo ? =>\n"
    "    a as (Foo ref, _)";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo, Bar)): Foo ? =>\n"
    "    match a\n"
    "    | ($let $1: Foo ref, _) =>\n"
    "      consume $borrowed $1\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, AsDontCareMultiTuple)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo, Bar, Baz)): (Foo, Baz) ? =>\n"
    "    a as (Foo ref, _, Baz ref)";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo, Bar, Baz)): (Foo, Baz) ? =>\n"
    "    match a\n"
    "    | ($let $1: Foo ref, _, $let $2: Baz ref) =>\n"
    "      (consume $borrowed $1, consume $borrowed $2)\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ObjectSimple)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    object fun foo() => 4 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    $T.create()\n"
    "    None\n"

    "primitive val $T\n"
    "  fun box foo(): None =>\n"
    "    4\n"
    "    None\n"
    "  new val create(): $T val^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ObjectWithField)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    object let x: T = 3 fun foo() => 4 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    $T.create(3)\n"
    "    None\n"

    "class ref $T\n"
    "  let x: T\n"
    "  fun box foo(): None =>\n"
    "    4\n"
    "    None\n"
    "  new ref create($1: T): $T ref^ => x = consume $1";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ObjectWithBehaviour)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    object be foo() => 4 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    $T.create()\n"
    "    None\n"

    "actor tag $T\n"
    "  be tag foo(): $T tag =>\n"
    "    4\n"
    "  new tag create(): $T tag^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ObjectBox)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    object box fun foo() => 4 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    $T.create()\n"
    "    None\n"

    "primitive val $T\n"
    "  fun box foo(): None =>\n"
    "    4\n"
    "    None\n"
    "  new val create(): $T val^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ObjectRef)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    object ref fun foo() => 4 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    $T.create()\n"
    "    None\n"

    "class ref $T\n"
    "  fun box foo(): None =>\n"
    "    4\n"
    "    None\n"
    "  new ref create(): $T ref^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ObjectTagWithBehaviour)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    object tag be foo() => 4 end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    $T.create()\n"
    "    None\n"

    "actor tag $T\n"
    "  be tag foo(): $T tag =>\n"
    "    4\n"
    "  new tag create(): $T tag^ => true";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, ObjectRefWithBehaviour)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    object ref be foo() => 4 end";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, LambdaTypeSimple)
{
  const char* short_form =
    "trait Foo\n"
    "  fun f(x: {()})";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo\n"
    "  fun box f(x: $T): None\n"

    "interface ref $T\n"
    "  fun box apply(): None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, LambdaTypeNamed)
{
  const char* short_form =
    "trait Foo\n"
    "  fun f(x: {bar ()})";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo\n"
    "  fun box f(x: $T): None\n"

    "interface ref $T\n"
    "  fun box bar(): None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, LambdaTypeParamsAndReturn)
{
  const char* short_form =
    "trait Foo\n"
    "  fun f(x: {(U32, U16): Bool})";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo\n"
    "  fun box f(x: $T): None\n"

    "interface ref $T\n"
    "  fun box apply(p1: U32, p2: U16): Bool";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, LambdaTypeFunTypeParams)
{
  const char* short_form =
    "trait Foo\n"
    "  fun f(x: {[A: F64, B: F32](A, U16): B})";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo\n"
    "  fun box f(x: $T): None\n"

    "interface ref $T\n"
    "  fun box apply[A: F64, B: F32](p1: A, p2: U16): B";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, LambdaTypeCapAndError)
{
  const char* short_form =
    "trait Foo\n"
    "  fun f(x: {ref (U32, U16): Bool ?})";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo\n"
    "  fun box f(x: $T): None\n"

    "interface ref $T\n"
    "  fun ref apply(p1: U32, p2: U16): Bool ?";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, LambdaTypeScopeTypeParams)
{
  const char* short_form =
    "trait Foo[A: F64, B: F32]\n"
    "  fun f(x: {(A, U16): B})";

  const char* full_form =
    "use \"builtin\"\n"
    "trait ref Foo[A: F64, B: F32]\n"
    "  fun box f(x: $T[A, B]): None\n"

    "interface ref $T[A: F64, B: F32]\n"
    "  fun box apply(p1: A, p2: U16): B";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, LambdaTypeScopeTypeParamsInFlet)
{
  const char* short_form =
    "class Foo[A: F64, B: F32]\n"
    "  let f: {(A, U16): B}";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo[A: F64, B: F32]\n"
    "  let f: $T[A, B]\n"
    "  new iso create(): Foo[A, B] iso^ =>\n"
    "    true\n"

    "interface ref $T[A: F64, B: F32]\n"
    "  fun box apply(p1: A, p2: U16): B";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, LambdaTypeSimpleAliased)
{
  const char* short_form =
    "type Foo is {()}";

  const char* full_form =
    "use \"builtin\"\n"
    "type Foo is $T\n"

    "interface ref $T\n"
    "  fun box apply(): None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, UseGuardNormalises)
{
  const char* short_form =
    "use \"test:Foo\" if (windows or linux) and not debug";

  const char* full_form =
    "use \"builtin\"\n"
    "use \"test:Foo\" if\n"
    "  $flag windows $ifdefor $flag linux\n"
    "  $ifdefand $ifdefnot $flag debug";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, UseGuardNeverTrue)
{
  const char* short_form =
    "use \"test:Foo\" if (debug and not debug)";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, UseGuardAlwaysTrue)
{
  const char* short_form =
    "use \"test:Foo\" if (debug or not debug)";

  TEST_COMPILE(short_form);
}


TEST_F(SugarTest, UseGuardUserFlagNormalises)
{
  const char* short_form =
    "use \"test:Foo\" if \"foo\"";

  const char* full_form =
    "use \"builtin\"\n"
    "use \"test:Foo\" if $flag foo";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, IfdefElseCondition)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    ifdef windows then\n"
    "      3\n"
    "    else\n"
    "      4\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    ifdef $flag windows $extra $ifdefnot $flag windows then\n"
    "      3\n"
    "    else\n"
    "      4\n"
    "    end\n"
    "    None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, IfdefSugarElse)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    ifdef windows then\n"
    "      3\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    ifdef $flag windows $extra $ifdefnot $flag windows then\n"
    "      3\n"
    "    else\n"
    "      None\n"
    "    end\n"
    "    None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, NestedIfdefCondition)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    ifdef windows then\n"
    "      ifdef debug then\n"
    "        1\n"
    "      else\n"
    "        2\n"
    "      end\n"
    "    else\n"
    "      ifdef \"foo\" then\n"
    "        3\n"
    "      else\n"
    "        4\n"
    "      end\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    ifdef $flag windows\n"
    "    $extra $ifdefnot $flag windows then\n"
    "      ifdef $flag windows $ifdefand $flag debug\n"
    "      $extra $flag windows $ifdefand $ifdefnot $flag debug then\n"
    "        1\n"
    "      else\n"
    "        2\n"
    "      end\n"
    "    else\n"
    "      ifdef $ifdefnot $flag windows $ifdefand $flag foo\n"
    "      $extra $ifdefnot $flag windows $ifdefand $ifdefnot $flag foo then\n"
    "        3\n"
    "      else\n"
    "        4\n"
    "      end\n"
    "    end\n"
    "    None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, IfdefElseConditionNeverTrue)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    ifdef debug and not debug then\n"
    "      3\n"
    "    end";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, IfdefElseConditionAlwaysTrue)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    ifdef debug or not debug then\n"
    "      3\n"
    "    end";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, NestedIfdefElseConditionNeverTrue)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    ifdef windows then\n"
    "      ifdef linux then\n"
    "        3\n"
    "      end\n"
    "    end";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, NestedIfdefElseConditionAlwaysTrue)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    ifdef windows then\n"
    "      ifdef windows then\n"
    "        3\n"
    "      end\n"
    "    end";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, IfdefPosix)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun f() =>\n"
    "    ifdef posix then\n"
    "      3\n"
    "    else\n"
    "      4\n"
    "    end";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(): None =>\n"
    "    ifdef $flag linux $ifdefor $flag osx $ifdefor $flag freebsd\n"
    "    $extra $ifdefnot $noseq($flag linux $ifdefor $flag osx $ifdefor\n"
    "      $flag freebsd) then\n"
    "      3\n"
    "    else\n"
    "      4\n"
    "    end\n"
    "    None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunction)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib(1): U64 => 1\n"
    "  fun fib(y: U64): U64 =>\n"
    "    fib(y - 2) + fib(y - 1)";

  const char* full_form =
  "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib(y: U64): (None | U64 | U64 | U64) =>\n"
    "    $1(consume y)\n"
    "  fun box $1($2: U64): (None | U64 | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | 1 => 1\n"
    "    | $let y: U64 => fib(y.sub(2)).add(fib(y.sub(1)))\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionPlusOtherFun)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib(1): U64 => 1\n"
    "  fun not_fib(): U64 => 1\n"
    "  fun fib(y: U64): U64 =>\n"
    "    fib(y - 2) + fib(y - 1)";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box not_fib(): U64 => 1\n"
    "  fun box fib(y: U64): (None | U64 | U64 | U64) =>\n"
    "    $1(consume y)\n"
    "  fun box $1($2: U64): (None | U64 | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | 1 => 1\n"
    "    | $let y: U64 => fib(y.sub(2)).add(fib(y.sub(1)))\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunction2InOneClassPlusOtherFun)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun foo(0): U64 => 0\n"
    "  fun foo(x: U64): U64 => 1\n"
    "  fun other(): U64 => 1\n"
    "  fun bar(0): U32 => 0\n"
    "  fun bar(y: U32): U32 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box other(): U64 => 1\n"
    "  fun box foo(x: U64): (None | U64 | U64) =>\n"
    "    $1(consume x)\n"
    "  fun box $1($2: U64): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let x: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end\n"
    "  fun box bar(y: U32): (None | U32 | U32) =>\n"
    "    $3(consume y)\n"
    "  fun box $3($4: U32): (None | U32 | U32) =>\n"
    "    match consume $4\n"
    "    | 0 => 0\n"
    "    | $let y: U32 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionParamNamedTwice)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib(y: U64): U64 => 1\n"
    "  fun fib(y: U64): U64 => 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib(y: (U64 | U64)): (None | U64 | U64 | U64) =>\n"
    "    $1(consume y)\n"
    "  fun box $1($2: (U64 | U64)): (None | U64 | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let y: U64 => 1\n"
    "    | $let y: U64 => 2\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunction2Params)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0, 0): U64 => 0\n"
    "  fun fib(a: U64, b: U32): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib(a: U64, b: U32): (None | U64 | U64) =>\n"
    "    $1(consume a, consume b)\n"
    "  fun box $1($2: U64, $3: U32): (None | U64 | U64) =>\n"
    "    match (consume $2, consume $3)\n"
    "    | (0, 0) => 0\n"
    "    | ($let a: U64, $let b: U32) => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionParamTypeUnion)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(a: U32): U64 => 0\n"
    "  fun fib(a: U64): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib(a: (U32 | U64)): (None | U64 | U64) =>\n"
    "    $1(consume a)\n"
    "  fun box $1($2: (U32 | U64)): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | $let a: U32 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionReturnUnion)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib(a: U64): U32 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib(a: U64): (None | U64 | U32) =>\n"
    "    $1(consume a)\n"
    "  fun box $1($2: U64): (None | U64 | U32) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionCap)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun ref fib(0): U64 => 0\n"
    "  fun ref fib(a: U64): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun ref fib(a: U64): (None | U64 | U64) =>\n"
    "    $1(consume a)\n"
    "  fun ref $1($2: U64): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionCapClash)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun box fib(0): U64 => 0\n"
    "  fun ref fib(a: U64): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionOneErrors)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun ref fib(0): U64 ? => 0\n"
    "  fun ref fib(a: U64): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun ref fib(a: U64): (None | U64 | U64) ? =>\n"
    "    $1(consume a)\n"
    "  fun ref $1($2: U64): (None | U64 | U64) ? =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionAllError)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun ref fib(0): U64 ? => 0\n"
    "  fun ref fib(a: U64): U64 ? => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun ref fib(a: U64): (None | U64 | U64) ? =>\n"
    "    $1(consume a)\n"
    "  fun ref $1($2: U64): (None | U64 | U64) ? =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionParamCountClash)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib(a: U64, b: U32): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionNoParams)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(): U64 => 0\n"
    "  fun fib(): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionParamNameClash)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(a: U64): U64 => 0\n"
    "  fun fib(b: U64): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionParamNotNamed)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib(1): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionValuePlusTypeBad)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0: U64): U64 => 0\n"
    "  fun fib(a: U64): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionValuePlusDefaultArgBad)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0 = 0): U64 => 0\n"
    "  fun fib(a: U64): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionDontCare)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0, _): U64 => 0\n"
    "  fun fib(a: U64, b: U32): U64 => 1\n"
    "  fun fib(_, _): U64 => 2";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib(a: U64, b: U32): (None | U64 | U64 | U64) =>\n"
    "    $1(consume a, consume b)\n"
    "  fun box $1($2: U64, $3: U32): (None | U64 | U64 | U64) =>\n"
    "    match (consume $2, consume $3)\n"
    "    | (0, _) => 0\n"
    "    | ($let a: U64, $let b: U32) => 1\n"
    "    | (_, _) => 2\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionGuard)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib(a: U64): U64 if a > 3 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib(a: U64): (None | U64 | U64) =>\n"
    "    $1(consume a)\n"
    "  fun box $1($2: U64): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 if a.gt(3) => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionDefaultValue)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib(a: U64 = 4): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib(a: U64 = 4): (None | U64 | U64) =>\n"
    "    $1(consume a)\n"
    "  fun box $1($2: U64): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionDefaultValueClash)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0 = 4): U64 => 0\n"
    "  fun fib(a: U64 = 4): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseBehaviour)
{
  const char* short_form =
    "actor Foo\n"
    "  var create: U32\n"
    "  be fib(0) => 0\n"
    "  be fib(a: U64) => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "actor tag Foo\n"
    "  var create: U32\n"
    "  be tag fib(a: U64): Foo tag =>\n"
    "    $1(consume a)\n"
    "  fun ref $1($2: U64): None =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end\n"
    "    None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionBehaviourClash)
{
  const char* short_form =
    "actor Foo\n"
    "  var create: U32\n"
    "  be fib(0) => 0\n"
    "  fun fib(a: U64) => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionConstructorsFail)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  new fib(0) => create = 0\n"
    "  new fib(a: U64) => create = 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionTypeParam)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib[A](0): A => 0\n"
    "  fun fib[A](a: U64): A => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib[A: A](a: U64): (None | A | A) =>\n"
    "    $1[A](consume a)\n"
    "  fun box $1[A: A]($2: U64): (None | A | A) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunction2TypeParams)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib[A, B](0): U64 => 0\n"
    "  fun fib[A, B](a: U64): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib[A: A, B: B](a: U64): (None | U64 | U64) =>\n"
    "    $1[A, B](consume a)\n"
    "  fun box $1[A: A, B: B]($2: U64): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionTypeParamConstraint)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib[A](0): U64 => 0\n"
    "  fun fib[A: B](a: U64): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib[A: B](a: U64): (None | U64 | U64) =>\n"
    "    $1[A](consume a)\n"
    "  fun box $1[A: B]($2: U64): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionTypeParamConstraintIntersect)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib[A: B](0): U64 => 0\n"
    "  fun fib[A: C](a: U64): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib[A: (B & C)](a: U64): (None | U64 | U64) =>\n"
    "    $1[A](consume a)\n"
    "  fun box $1[A: (B & C)]($2: U64): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionTypeParamCountClash)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib(0): U64 => 0\n"
    "  fun fib[A](a: U64): U64 => 1";

  TEST_ERROR(short_form);
}


TEST_F(SugarTest, CaseFunctionTypeParamNameClash)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib[A](a: U64): U64 => 0\n"
    "  fun fib[B](b: U64): U64 => 1";

  TEST_ERROR(short_form);
}



TEST_F(SugarTest, CaseFunctionDefaultTypeParam)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib[A](0): U64 => 0\n"
    "  fun fib[A = B](a: U64): U64 => 1";

  const char* full_form =
    "use \"builtin\"\n"
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box fib[A: A = B](a: U64): (None | U64 | U64) =>\n"
    "    $1[A](consume a)\n"
    "  fun box $1[A: A = B]($2: U64): (None | U64 | U64) =>\n"
    "    match consume $2\n"
    "    | 0 => 0\n"
    "    | $let a: U64 => 1\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTest, CaseFunctionDefaultTypeParamClash)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun fib[A = B](0): U64 => 0\n"
    "  fun fib[A = C](a: U64): U64 => 1";

  TEST_ERROR(short_form);
}
