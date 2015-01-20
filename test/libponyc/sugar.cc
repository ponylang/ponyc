#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <pkg/package.h>
#include <pass/parsefix.h>
#include <pass/pass.h>
#include <pass/sugar.h>

#include "util.h"


static const char* builtin = "primitive U32";

class SugarTest: public testing::Test
{};


static void test_good_sugar(const char* short_form, const char* full_form)
{
  pass_opt_t opt;
  pass_opt_init(&opt);
  limit_passes(&opt, "sugar");

  package_add_magic("builtin", builtin);
  package_suppress_build_message();
  free_errors();

  package_add_magic("short", short_form);
  ast_t* short_ast = program_load(stringtab("short"), &opt);

  if(short_ast == NULL)
  {
    print_errors();
    ASSERT_NE((void*)NULL, short_ast);
  }

  limit_passes(&opt, "parse");
  package_add_magic("full", full_form);
  ast_t* full_ast = program_load(stringtab("full"), &opt);

  if(full_ast == NULL)
  {
    print_errors();
    ASSERT_NE((void*)NULL, full_ast);
  }

  bool r = build_compare_asts(full_ast, short_ast);

  if(!r)
  {
    printf("Full form:\n");
    ast_print(full_ast);
    printf("Short form:\n");
    ast_print(short_ast);
    print_errors();
  }

  ASSERT_TRUE(r);

  // Reset state for next test
  pass_opt_done(&opt);
}


static void test_bad_sugar(const char* src)
{
  package_add_magic("prog", src);
  package_add_magic("builtin", builtin);
  package_suppress_build_message();
  free_errors();

  pass_opt_t opt;
  pass_opt_init(&opt);
  limit_passes(&opt, "sugar");
  ast_t* ast = program_load(stringtab("prog"), &opt);
  pass_opt_done(&opt);

  if(ast != NULL)
  {
    ast_print(ast);
    ASSERT_EQ((void*)NULL, ast);
  }
}


TEST(SugarTest, DataType)
{
  const char* short_form =
    "primitive Foo";

  const char* full_form =
    "primitive Foo val\n"
    "  new val create(): Foo val^ => true\n"
    "  fun box eq(that:Foo): Bool => this is that\n"
    "  fun box ne(that:Foo): Bool => this isnt that\n";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ClassWithField)
{
  // Create constructor should not be added if there are uninitialsed fields
  const char* short_form =
    "class Foo iso let m:U32";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, ClassWithInitialisedField)
{
  // Create constructor should be added if there are only initialised fields
  const char* short_form =
    "class Foo iso let m:U32 = 3";

  const char* full_form =
    "class Foo iso let m:U32 = 3 new ref create(): Foo ref^ => true";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ClassWithCreateConstructor)
{
  // Create constructor should not be added if it's already there
  const char* short_form =
    "class Foo iso new create() => 3";

  const char* full_form =
    "class Foo iso new ref create(): Foo ref^ => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ClassWithCreateFunction)
{
  // Create function doesn't clash with create constructor
  const char* short_form =
    "class Foo fun ref create():U32 => 3";

  const char* full_form =
    "class Foo ref fun ref create():U32 => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ClassWithoutFieldOrCreate)
{
  const char* short_form =
    "class Foo iso";

  const char* full_form =
    "class Foo iso new ref create(): Foo ref^ => true";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ClassWithoutDefCap)
{
  const char* short_form = "class Foo     let m:U32";
  const char* full_form =  "class Foo ref let m:U32";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ActorWithField)
{
  // Create constructor should not be added if there are uninitialsed fields
  const char* short_form = "actor Foo     let m:U32";
  const char* full_form =  "actor Foo tag let m:U32";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ActorWithInitialisedField)
{
  // Create constructor should be added if there are only initialsed fields
  const char* short_form =
    "actor Foo let m:U32 = 3";

  const char* full_form =
    "actor Foo tag let m:U32 = 3 new tag create(): Foo tag^ => true";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ActorWithCreateConstructor)
{
  // Create constructor should not be added if it's already there
  const char* short_form =
    "actor Foo new create() => 3";

  const char* full_form =
    "actor Foo tag new tag create(): Foo tag^ => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ActorWithCreateFunction)
{
  // Create function doesn't clash with create constructor
  const char* short_form =
    "actor Foo fun ref create():U32 => 3";

  const char* full_form =
    "actor Foo tag fun ref create():U32 => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ActorWithCreateBehaviour)
{
  // Create behaviour doesn't clash with create constructor
  const char* short_form =
    "actor Foo be create() => 3";

  const char* full_form =
    "actor Foo tag be create():Foo tag => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ActorWithoutFieldOrCreate)
{
  const char* short_form =
    "actor Foo";

  const char* full_form =
    "actor Foo tag new tag create(): Foo tag^ => true";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ActorWithoutDefCap)
{
  const char* short_form = "actor Foo     let m:U32";
  const char* full_form =  "actor Foo tag let m:U32";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, TraitWithCap)
{
  const char* short_form = "trait Foo box";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, TraitWithoutCap)
{
  const char* short_form = "trait Foo";
  const char* full_form =  "trait Foo ref";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, TypeParamWithConstraint)
{
  const char* short_form = "class Foo[A: U32] ref var y:U32";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, TypeParamWithoutConstraint)
{
  const char* short_form = "class Foo[A]    ref var y:U32";
  const char* full_form =  "class Foo[A: A] ref var y:U32";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ConstructorNoReturnType)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  new create() => 3";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  new ref create(): Foo ref^ => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ConstructorInActor)
{
  const char* short_form =
    "actor Foo var y:U32\n"
    "  new create() => 3";

  const char* full_form =
    "actor Foo tag var y:U32\n"
    "  new tag create(): Foo tag^ => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ConstructorInDataType)
{
  const char* short_form =
    "primitive Foo\n"
    "  new create() => 3";

  const char* full_form =
    "primitive Foo val\n"
    "  new val create(): Foo val^ => 3\n"
    "  fun box eq(that:Foo): Bool => this is that\n"
    "  fun box ne(that:Foo): Bool => this isnt that\n";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ConstructorInGenericClass)
{
  const char* short_form =
    "class Foo[A: B, C] ref var y:U32\n"
    "  new bar() => 3";

  const char* full_form =
    "class Foo[A: B, C: C] ref var y:U32\n"
    "  new ref bar(): Foo[A, C] ref^ => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, BehaviourReturnType)
{
  const char* short_form = "actor Foo     var y:U32 be foo()         => 3";
  const char* full_form =  "actor Foo tag var y:U32 be foo():Foo tag => 3";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, FunctionComplete)
{
  const char* short_form =
    "class Foo ref var y:U32 fun box foo(): U32 val => 3";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, FunctionNoReturnNoBody)
{
  const char* short_form =
    "trait Foo ref fun box foo()";

  const char* full_form =
    "trait Foo ref fun box foo(): None";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, FunctionNoReturnBody)
{
  const char* short_form =
    "trait Foo ref fun box foo() => 3";

  const char* full_form =
    "trait Foo ref fun box foo(): None => 3\n"
    "  None";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, IfWithoutElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  if 1 then 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  if 1 then 2 else None end";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, IfWithElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  if 1 then 2 else 3 end";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, WhileWithoutElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  while 1 do 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  while 1 do 2 else None end";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, WhileWithElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  while 1 do 2 else 3 end";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, TryWithElseAndThen)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else 2 then 3 end";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, TryWithoutElseOrThen)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else None then None end";

  DO(test_good_sugar(short_form, full_form));
}

/*
TEST(SugarTest, TryWithoutElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 then 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else None then 2 end";

  DO(test_good_sugar(short_form, full_form));
}
*/

TEST(SugarTest, TryWithoutThen)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  try 1 else 2 then None end";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ForWithoutElse)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  for i in 1 do 2 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  $(\n"
    "    let hygid = $(1)\n"
    "    while hygid.has_next() do\n"
    "      let i = hygid.next()\n"
    "      $(2)\n"
    "    else None end\n"
    "  )";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ForWithElseAndIteratorType)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  for i:U32 in 1 do 2 else 3 end";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  $(\n"
    "    let hygid = $(1)\n"
    "    while hygid.has_next() do\n"
    "      let i:U32 = hygid.next()\n"
    "      $(2)\n"
    "    else\n"
    "      3\n"
    "    end\n"
    "  )";

  DO(test_good_sugar(short_form, full_form));
}


// TODO(andy): Tests for sugar_bang, once that's done


TEST(SugarTest, CaseWithBody)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1 => 2\n"
    "  else\n"
    "    3\n"
    "  end";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, CaseWithBodyAndFollowingCase)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  match(x)\n"
    "  |1 => 2\n"
    "  |3 => 4\n"
    "  else\n"
    "    5\n"
    "  end";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, CaseWithNoBody)
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

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, CaseWithNoBodyMultiple)
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

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, MatchWithNoElse)
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

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, UpdateLhsNotCall)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo = 1";

  DO(test_good_sugar(short_form, short_form));
}


TEST(SugarTest, UpdateNoArgs)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo() = 1";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo.update(where value = 1)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, UpdateWithArgs)
{
  const char* short_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo(2, 3 where bar = 4) = 1";

  const char* full_form =
    "class Foo ref var y:U32 fun ref f(): U32 val =>\n"
    "  foo.update(2, 3 where bar = 4, value = 1)";

  DO(test_good_sugar(short_form, full_form));
}


// Operator subsituation

TEST(SugarTest, Add)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 + 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.add(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Sub)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 - 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.sub(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Multiply)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 * 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.mul(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Divide)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 / 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.div(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Mod)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 % 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.mod(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, UnaryMinus)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => -1";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.neg()";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ShiftLeft)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 << 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.shl(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, ShiftRight)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 >> 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.shr(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, And)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 and 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.op_and(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Or)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 or 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.op_or(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Xor)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 xor 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.op_xor(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Not)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => not 1";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.op_not()";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Eq)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 == 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.eq(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Ne)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 != 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.ne(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Lt)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 < 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.lt(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Le)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 <= 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.le(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Gt)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 > 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.gt(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, Ge)
{
  const char* short_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1 >= 2";

  const char* full_form =
    "class Foo ref var y:U32\n"
    "  fun ref f(): U32 val => 1.ge(2)";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, As)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo | Bar)): Foo ? => a as Foo ref";

  const char* full_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo | Bar)): Foo ? =>\n"
    "    match a\n"
    "    | let hygid: Foo ref => consume hygid\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, AsTuple)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar)): (Foo, Bar) ? => a as (Foo ref, Bar ref)";

  const char* full_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar)): (Foo, Bar) ? =>\n"
    "    match a\n"
    "    | (let hygid: Foo ref, let hygid: Bar ref) =>\n"
    "      (consume hygid, consume hygid)\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, AsNestedTuple)
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
    "      (consume hygid, (consume hygid, consume hygid))\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, AsDontCare)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: Foo): Foo ? => a as (_)";

  DO(test_bad_sugar(short_form));
}


TEST(SugarTest, AsDontCare2Tuple)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar)): Foo ? => a as (Foo ref, _)";

  const char* full_form =
    "class Foo ref\n"
    "  fun ref f(a: (Foo, Bar)): Foo ? =>\n"
    "    match a\n"
    "    | (let hygid: Foo ref, _) =>\n"
    "      consume hygid\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  DO(test_good_sugar(short_form, full_form));
}


TEST(SugarTest, AsDontCareMultiTuple)
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
    "      (consume hygid, consume hygid)\n"
    "    else\n"
    "      error\n"
    "    end\n"
    "  new ref create(): Foo ref^ => true";

  DO(test_good_sugar(short_form, full_form));
}
