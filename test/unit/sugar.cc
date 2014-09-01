extern "C" {
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ast/token.h"
#include "../../src/libponyc/pass/sugar.h"
}
#include "util.h"
#include <gtest/gtest.h>


class SugarTest: public testing::Test
{};


static void test_good_sugar(const char* before, const char* after)
{
  free_errors();

  builder_t* actual_builder;
  ast_t* actual_ast;
  DO(build_ast_from_string(before, &actual_ast, &actual_builder));

  ast_t* tree = builder_find_sub_tree(actual_builder, "start");
  if(tree == NULL)  // No "start" node defined, use whole tree
    tree = builder_get_root(actual_builder);

  ASSERT_NE((void*)NULL, tree);

  builder_t* expect_builder;
  ast_t* expect_ast;
  DO(build_ast_from_string(after, &expect_ast, &expect_builder));

  ASSERT_EQ(AST_OK, pass_sugar(&tree));

  actual_ast = builder_get_root(actual_builder);

  bool r = build_compare_asts(expect_ast, actual_ast);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect_ast, 80);
    printf("Got:\n");
    ast_print(actual_ast, 80);
  }

  ASSERT_TRUE(r);

  builder_free(actual_builder);
  builder_free(expect_builder);
}


static void test_bad_sugar(const char* desc, ast_result_t expect_result)
{
  builder_t* actual_builder;
  ast_t* actual_ast;
  DO(build_ast_from_string(desc, &actual_ast, &actual_builder));

  ast_t* tree = builder_find_sub_tree(actual_builder, "start");
  if(tree == NULL)
    tree = actual_ast;

  ASSERT_NE((void*)NULL, tree);

  ASSERT_EQ(expect_result, pass_sugar(&tree));

  builder_free(actual_builder);
}


TEST(SugarTest, TraitMain)
{
  const char* before = "(trait (id Main) x box x x)";

  DO(test_bad_sugar(before, AST_ERROR));
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


TEST(SugarTest, LetFieldInTrait)
{
  const char* before = "(trait (flet{def start} (id foo) x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, LetFieldNotInTrait)
{
  const char* before = "(class (flet{def start} (id foo) x x))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, VarFieldInTrait)
{
  const char* before = "(trait (fvar{def start} (id foo) x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, VarFieldNotInTrait)
{
  const char* before = "(class (fvar{def start} (id foo) x x))";

  DO(test_good_sugar(before, before));
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


TEST(SugarTest, BehaviourInClass)
{
  const char* before = "(class (be{def start} tag (id foo) x x x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, BehaviourInActor)
{
  const char* before =
    "(actor (id foo) x x x (members (be{def start} tag (id foo) x x x x x)))";

  const char* after =
    "(actor (id foo) x x x"
    "  (members (be tag (id foo) x x"
    "    (nominal x (id foo) x tag x) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, FunctionIso)
{
  const char* before = "(fun iso (id foo) x x x x x)";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, FunctionTrn)
{
  const char* before = "(fun trn (id foo) x x x x x)";

  DO(test_bad_sugar(before, AST_ERROR));
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


TEST(SugarTest, CaseWithNoBodyOrFollowingCase)
{
  const char* before = "(case (seq 1) x x x)";

  DO(test_bad_sugar(before, AST_FATAL));
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


// Pure type checking "sugar"

TEST(SugarTest, TypeAliasGood)
{
  const char* before = "(type (id foo) (nominal (id A) x x x x))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, TypeAliasMain)
{
  const char* before = "(type (id Main) (nominal (id A) x x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassGoodWithField)
{
  const char* before = "(class (id Foo) x iso x (members (flet (id m) x x)))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, ClassMain)
{
  const char* before = "(class (id Main) x iso x (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassNominalTrait)
{
  const char* before =
    "(class (id Foo) x iso (types (nominal x (id A) x x x))"
    "  (members (flet (id m) x x)))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, ClassNonNominalTrait)
{
  const char* before =
    "(class (id Foo) x iso (types (thistype)) (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassTraitCapability)
{
  const char* before =
    "(class (id Foo) x iso (types (nominal x (id A) x ref x))"
    "  (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassTraitEphemeral)
{
  const char* before =
    "(class (id Foo) x iso (types (nominal x (id A) x x ^))"
    "  (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassSecondTraitEphemeral)
{
  const char* before =
    "(class (id Foo) x iso"
    "  (types"
    "    (nominal x (id A) x x x)"
    "    (nominal x (id B) x x ^))"
    "  (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
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


TEST(SugarTest, ClassWithCreateBehaviour)
{
  const char* before =
    "(class (id Foo) x iso x (members (be ref (id create) x x x x (seq 3))))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ClassWithoutFieldOrCreate)
{
  const char* before =
    "(class (id Foo) x ref x members)";

  const char* after =
    "(class (id Foo) x ref x"
    "  (members"
    "    (new x (id create) x x (nominal x (id Foo) x val x) x"
    "      (seq (reference (id None))))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ClassWithoutDefCap)
{
  const char* before = "(class (id foo) x x   x (members (flet (id m) x x)))";
  const char* after  = "(class (id foo) x ref x (members (flet (id m) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorGoodWithField)
{
  const char* before = "(actor (id Foo) x x   x (members (flet (id m) x x)))";
  const char* after  = "(actor (id Foo) x tag x (members (flet (id m) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorMain)
{
  const char* before = "(actor (id Main) x x   x (members (flet (id m) x x)))";
  const char* after  = "(actor (id Main) x tag x (members (flet (id m) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorNominalTrait)
{
  const char* before =
    "(actor (id Foo) x x (types (nominal x (id A) x x x))"
    "  (members (flet (id m) x x)))";

  const char* after =
    "(actor (id Foo) x tag (types (nominal x (id A) x x x))"
    "  (members (flet (id m) x x)))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorNonNominalTrait)
{
  const char* before =
    "(actor (id Foo) x x (types (thistype)) (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ActorTraitCapability)
{
  const char* before =
    "(actor (id Foo) x x (types (nominal x (id A) x ref x))"
    "  (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ActorTraitEphemeral)
{
  const char* before =
    "(actor (id Foo) x x (types (nominal x (id A) x x ^))"
    "  (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ActorSecondTraitEphemeral)
{
  const char* before =
    "(actor (id Foo) x x"
    "  (types"
    "    (nominal x (id A) x x x)"
    "    (nominal x (id B) x x ^))"
    "  (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
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
    "    (new x (id create) x x (nominal x (id Foo) x val x) x"
    "      (seq (reference (id None))))))";

  DO(test_good_sugar(before, after));
}


TEST(SugarTest, ActorWithDefCap)
{
  const char* before = "(actor (id foo) x ref x (members (flet (id m) x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointGood)
{
  const char* before = "(-> thistype (nominal x (id A) x x x))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, ViewpointLeftUnionType)
{
  const char* before =
    "(->"
    "  (uniontype"
    "    (nominal x (id A) x x x)"
    "    (nominal x (id B) x x x)"
    "  )"
    "  (nominal x (id C) x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftIntersectionType)
{
  const char* before =
    "(->"
    "  (isecttype"
    "    (nominal x (id A) x x x)"
    "    (nominal x (id B) x x x)"
    "  )"
    "  (nominal x (id C) x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftTupleType)
{
  const char* before =
    "(->"
    "  (tupletype"
    "    (nominal x (id A) x x x)"
    "    (nominal x (id B) x x x)"
    "  )"
    "  (nominal x (id C) x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftStructural)
{
  const char* before =
    "(-> (structural members x x) (nominal x (id A) x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftNominalWithCapability)
{
  const char* before =
    "(-> (nominal x (id A) x ref x) (nominal x (id B) x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftNominalEphemeral)
{
  const char* before =
    "(-> (nominal x (id A) x x ^) (nominal x (id B) x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointRightUnionType)
{
  const char* before =
    "(->"
    "  (nominal x (id A) x x x)"
    "  (uniontype"
    "    (nominal x (id B) x x x)"
    "    (nominal x (id C) x x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointRightIntersectionType)
{
  const char* before =
    "(->"
    "  (nominal x (id A) x x x)"
    "  (isecttype"
    "    (nominal x (id B) x x x)"
    "    (nominal x (id C) x x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointRightTupleType)
{
  const char* before =
    "(->"
    "  (nominal x (id A) x x x)"
    "  (tupletype"
    "    (nominal x (id B) x x x)"
    "    (nominal x (id C) x x x)))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ViewpointRightThis)
{
  const char* before = "(-> (nominal x (id A) x x x) thistype)";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ThisTypeNotInViewpoint)
{
  const char* before =
    "(tupletype thistype{def start} (nominal x (id A) x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ThisTypeInViewpointNotInMethod)
{
  const char* before = "(-> thistype{def start} (nominal x (id A) x x x))";

  DO(test_bad_sugar(before, AST_ERROR));
}


TEST(SugarTest, ThisTypeInViewpointInMethod)
{
  const char* before =
    "(fun x (id foo) x x x x"
    "  (-> thistype{def start} (nominal x (id A) x x x)))";

  DO(test_good_sugar(before, before));
}


TEST(SugarTest, EphemeralNotInMethodReturnType)
{
  const char* before =
    "(fun x (id foo) x"
    "  (params (param (id bar) (nominal x (id A) x x ^{def start}) x))"
    "  x x x)";

  DO(test_bad_sugar(before, AST_ERROR));
}
