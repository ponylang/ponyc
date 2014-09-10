#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ast/token.h"
#include "../../src/libponyc/pass/sugar.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


class SugarTest: public testing::Test
{};


static void test_good_sugar(const char* before, const char* after)
{
  DO(test_pass_fn_good(before, after, pass_sugar, "start"));
}


static void test_bad_sugar(const char* desc, ast_result_t expect_result)
{
  DO(test_pass_fn_bad(desc, pass_sugar, "start", expect_result));
}


TEST(SugarTest, DataType)
{
  const char* before =
    "(primitive (id Foo) x x x members)";

  const char* after =
    "(primitive (id Foo) x val x"
    "  (members"
    "    (new x (id create) x x x x (seq (reference (id None))))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ClassWithField)
{
  const char* before =
    "(class (id Foo) x iso x (members (flet (id m) x x)))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, ClassWithCreateConstructor)
{
  const char* before =
    "(class (id Foo) x iso x (members (new ref (id create) x x x x (seq 3))))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, ClassWithCreateFunction)
{
  const char* before =
    "(class (id Foo) x iso x (members (fun ref (id create) x x x x (seq 3))))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassWithoutFieldOrCreate)
{
  const char* before =
    "(class (id Foo) x iso x members)";

  const char* after =
    "(class (id Foo) x iso x"
    "  (members"
    "    (new x (id create) x x x x (seq (reference (id None))))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ClassWithoutDefCap)
{
  const char* before = "(class (id foo) x x   x (members (flet (id m) x x)))";
  const char* after =  "(class (id foo) x ref x (members (flet (id m) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorWithField)
{
  const char* before = "(actor (id Foo) x x   x (members (flet (id m) x x)))";
  const char* after =  "(actor (id Foo) x tag x (members (flet (id m) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorWithCreateConstructor)
{
  const char* before =
    "(actor (id Foo) x x x (members (new ref (id create) x x x x (seq 3))))";

  const char* after =
    "(actor (id Foo) x tag x (members (new ref (id create) x x x x (seq 3))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorWithCreateFunction)
{
  const char* before =
    "(actor (id Foo) x x x (members (fun ref (id create) x x x x (seq 3))))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ActorWithCreateBehaviour)
{
  const char* before =
    "(actor (id Foo) x x x (members (be ref (id create) x x x x (seq 3))))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ActorWithoutFieldOrCreate)
{
  const char* before =
    "(actor (id Foo) x x x members)";

  const char* after =
    "(actor (id Foo) x tag x"
    "  (members"
    "    (new x (id create) x x x x (seq (reference (id None))))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, TraitWithCap)
{
  const char* before = "(trait (id foo) x box x x)";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, TraitWithoutCap)
{
  const char* before = "(trait (id foo) x x   x x)";
  const char* after  = "(trait (id foo) x ref x x)";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, TypeParamWithConstraint)
{
  const char* before = "(typeparam (id foo) thistype x)";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, TypeParamWithoutConstraint)
{
  const char* before = "(typeparam (id foo) x x)";
  const char* after  = "(typeparam (id foo) (structural members tag x) x)";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ConstructorNoNameNoReturnType)
{
  const char* before =
    "(class (id foo) x x x (members (new{def start} ref x x x x x x)))";

  const char* after =
    "(class (id foo) x x x"
    "  (members (new ref (id create) x x"
    "    (nominal x (id foo) x ref ^) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ConstructorInActor)
{
  const char* before =
    "(actor (id foo) x x x (members"
    "  (new{def start} ref (id make) x x x x x)))";

  const char* after =
    "(actor (id foo) x x x"
    "  (members (new ref (id make) x x"
    "    (nominal x (id foo) x tag ^) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ConstructorInDataType)
{
  const char* before =
    "(primitive (id foo) x x x (members"
    "  (new{def start} ref (id make) x x x x x)))";

  const char* after =
    "(primitive (id foo) x x x"
    "  (members (new ref (id make) x x"
    "    (nominal x (id foo) x val ^) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ConstructorInGenericClass)
{
  const char* before =
    "(class (id foo)"
    "  (typeparams"
    "    (typeparam (id A) (nominal (id B) x x x) x)"
    "    (typeparam (id C) x x)"
    "  ) x x (members (new{def start} ref (id bar) x x x x x)))";

  const char* after =
    "(class (id foo)"
    "  (typeparams"
    "    (typeparam (id A) (nominal (id B) x x x) x)"
    "    (typeparam (id C) x x)"
    "  )"
    "  x x"
    "  (members (new ref (id bar) x x"
    "    (nominal"
    "      x (id foo)"
    "      (typeargs"
    "        (nominal x (id A) x x x)"
    "        (nominal x (id C) x x x)"
    "      )"
    "      ref ^"
    "    )"
    "    x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, BehaviourReturnType)
{
  const char* before =
    "(actor (id foo) x x x (members (be{def start} tag (id foo) x x x x x)))";

  const char* after =
    "(actor (id foo) x x x"
    "  (members (be tag (id foo) x x"
    "    (nominal x (id foo) x tag x) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, FunctionComplete)
{
  const char* before =
    "(fun ref (id foo) x x (nominal x (id U32) x x x) x (seq 3))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, FunctionNoName)
{
  const char* before =
    "(fun ref x x x (nominal x (id U32) x x x) x (seq 3))";

  const char* after =
    "(fun ref (id apply) x x (nominal x (id U32) x x x) x (seq 3))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, FunctionNoReturnNoBody)
{
  const char* before =
    "(fun ref (id foo) x x x x x)";

  const char* after =
    "(fun ref (id foo) x x (nominal x (id None) x x x) x x)";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, FunctionNoReturnBody)
{
  const char* before =
    "(fun ref (id foo) x x x x (seq 3))";

  const char* after =
    "(fun"
    "  ref (id foo) x x"
    "  (nominal x (id None) x x x) x"
    "  (seq 3 (reference (id None))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, NominalWithPackage)
{
  const char* before = "(nominal (id foo) (id bar) x x x)";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, NominalWithoutPackage)
{
  const char* before = "(nominal (id foo) x x x x)";
  const char* after =  "(nominal x (id foo) x x x)";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, StructuralWithCapability)
{
  const char* before = "(structural members box x)";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, StructuralWithoutCapabilityInTypeParam)
{
  const char* before =
    "(typeparam (id foo) (structural{def start} members x x) x)";

  const char* after =
    "(typeparam (id foo) (structural members tag x) x)";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, StructuralWithoutCapabilityNotInTypeParam)
{
  const char* before = "(structural members x x)";
  const char* after =  "(structural members ref x)";

  DO(test_good_sugar(before, after));
}


// TODO(andy): This currently fails because of a known bug. See
// sugar_structural() in sugar.c
/*
TEST(SugarTest, StructuralWithoutCapabilityInInnerInTypeParam)
{
  const char* before =
    "(typeparam (id foo)"
    "  (structural"
    "    (members"
    "      (fun ref (id bar) x (params"
    "        (param (id y) (structural{def start} members x x) x))))"
    "    box x) x)";

  const char* after =
    "(typeparam (id foo)"
    "  (structural"
    "    (members"
    "      (fun ref (id bar) x (params"
    "        (param (id y) (structural{def start} members ref x) x))))"
    "    box x) x)";

  DO(test_good_sugar(before, after));
}
*/


TEST(SugarTest, IfWithoutElse)
{
  const char* before = "(if (seq 3) (seq 1) x)";
  const char* after =  "(if (seq 3) (seq 1) (seq (reference (id None))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, IfWithElse)
{
  const char* before = "(if (seq 3) (seq 1) (seq 2))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, WhileWithoutElse)
{
  const char* before = "(while (seq 3) (seq 1) x)";
  const char* after =  "(while (seq 3) (seq 1) (seq (reference (id None))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, WhileWithElse)
{
  const char* before = "(while (seq 3) (seq 1) (seq 2))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, TryWithElseOAndThen)
{
  const char* before = "(try (seq 1) (seq 2) (seq 3))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, TryWithoutElseOrThen)
{
  const char* before = "(try (seq 1) x x)";
  const char* after =
    "(try (seq 1) (seq (reference (id None))) (seq (reference (id None))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, TryWithoutElse)
{
  const char* before = "(try (seq 1) x (seq 3))";
  const char* after =
    "(try (seq 1) (seq (reference (id None))) (seq 3))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, TryWithoutThen)
{
  const char* before = "(try (seq 1) (seq 2) x)";
  const char* after =
    "(try (seq 1) (seq 2) (seq (reference (id None))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ForWithoutElse)
{
  const char* before = "(for (idseq (id i)) x (seq 3) (seq 4) x)";
  const char* after =
    "(seq{scope}"
    "  (= (var (idseq (id hygid)) x) (seq 3))"
    "  (while{scope}"
    "    (call (. (reference (id hygid)) (id has_next)) x x)"
    "    (seq{scope}"
    "      (="
    "        (var (idseq (id i)) x)"
    "        (call (. (reference (id hygid)) (id next)) x x))"
    "      (seq 4)"
    "    )"
    "    (seq (reference (id None)))"
    "  )"
    ")";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ForWithElseAndIteratorType)
{
  const char* before =
    "(for (idseq (id i)) (nominal x (id Foo) x x x) (seq 3) (seq 4) (seq 5))";

  const char* after =
    "(seq{scope}"
    "  (= (var (idseq (id hygid)) (nominal x (id Foo) x x x)) (seq 3))"
    "  (while{scope}"
    "    (call (. (reference (id hygid)) (id has_next)) x x)"
    "    (seq{scope}"
    "      (="
    "        (var (idseq (id i)) (nominal x (id Foo) x x x))"
    "        (call (. (reference (id hygid)) (id next)) x x))"
    "      (seq 4)"
    "    )"
    "    (seq 5)"
    "  )"
    ")";

  DO(test_good_sugar(before, after));
}


// TODO(andy): Tests for sugar_bang, once that's done


TEST(SugarTest, CaseWithBody)
{
  const char* before = "(case (seq 1) x x (seq 2))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, CaseWithBodyAndFollowingCase)
{
  const char* before =
    "(cases"
    "  (case{def start} (seq 1) x x (seq 2))"
    "  (case (seq 3) x x (seq 4)))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, CaseWithNoBody)
{
  const char* before =
    "(cases"
    "  (case{def start} (seq 1) x x x)"
    "  (case (seq 2) x x (seq 3)))";

  const char* after  =
    "(cases"
    "  (case (seq 1) x x (seq 3))"
    "  (case (seq 2) x x (seq 3)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, CaseWithNoBodyMultiple)
{
  const char* before =
    "(cases"
    "  (case{def start} (seq 1) x x x)"
    "  (case (seq 2) x x x)"
    "  (case (seq 3) x x x)"
    "  (case (seq 4) x x (seq 5)))";

  const char* after =
    "(cases"
    "  (case (seq 1) x x (seq 5))"
    "  (case (seq 2) x x x)"
    "  (case (seq 3) x x x)"
    "  (case (seq 4) x x (seq 5)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, UpdateLhsNotCall)
{
  const char* before = "(= (reference (id foo)) (seq 1))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, UpdateNoArgs)
{
  const char* before = "(= (call (seq 1) x x)(seq 2))";

  const char* after  =
    "(call"
    "  (. (seq 1) (id update))"
    "  (positionalargs (seq 2))"
    "  x)";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, UpdateWithArgs)
{
  const char* before =
    "(="
    "  (call"
    "    (seq 1)"
    "    (positionalargs (seq 2) (seq 3))"
    "    (namedargs"
    "      (namedarg (id foo) (seq 4))"
    "      (namedarg (id bar) (seq 5))))"
    "  (seq 6))";

  const char* after =
    "(call"
    "  (. (seq 1) (id update))"
    "  (positionalargs (seq 2) (seq 3) (seq 6))"
    "  (namedargs"
    "    (namedarg (id foo) (seq 4))"
    "    (namedarg (id bar) (seq 5))))";

  DO(test_good_sugar(before, after));
}
