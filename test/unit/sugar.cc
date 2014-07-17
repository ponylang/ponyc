extern "C" {
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ast/token.h"
#include "../../src/libponyc/pass/sugar.h"
}
#include <gtest/gtest.h>


class SugarTest: public testing::Test
{};


static ast_t* find_start_internal(ast_t* tree, token_id start_id)
{
  if(tree == NULL)
    return NULL;

  if(ast_id(tree) == start_id)
    return tree;

  ast_t* ast = find_start_internal(ast_child(tree), start_id);
  if(ast != NULL)
    return ast;

  ast = find_start_internal(ast_sibling(tree), start_id);
  if(ast != NULL)
    return ast;

  return NULL;
}


static ast_t* find_start(ast_t* tree, token_id start_id)
{
  ast_t* ast = find_start_internal(tree, start_id);

  if(ast == NULL)
    printf("Token id %d not found in tree\n", start_id);

  return ast;
}


static void test_good_sugar(const char* before, const char* after,
  token_id start_id)
{
  source_t* actual_src = source_open_string(before);
  ast_t* actual_ast = build_ast(actual_src);
  ASSERT_NE((void*)NULL, actual_ast);

  ast_t* tree = find_start(actual_ast, start_id);
  ASSERT_NE((void*)NULL, tree);

  source_t* expect_src = source_open_string(after);
  ast_t* expect_ast = build_ast(expect_src);
  ASSERT_NE((void*)NULL, expect_ast);

  ASSERT_EQ(AST_OK, pass_sugar(&tree));

  bool r = build_compare_asts(expect_ast, actual_ast);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect_ast, 80);
    printf("\nGot:\n");
    ast_print(actual_ast, 80);
    printf("\n");
  }

  ASSERT_TRUE(r);

  ast_free(actual_ast);
  source_close(actual_src);
  ast_free(expect_ast);
  source_close(expect_src);
}


static void test_bad_sugar(const char* desc, token_id start_id,
  ast_result_t expect_result)
{
  source_t* src = source_open_string(desc);
  ast_t* ast = build_ast(src);
  ASSERT_NE((void*)NULL, ast);

  ast_t* tree = find_start(ast, start_id);
  ASSERT_NE((void*)NULL, tree);

  ASSERT_EQ(expect_result, pass_sugar(&tree));

  ast_free(ast);
  source_close(src);
}


TEST(SugarTest, TraitWithCap)
{
  const char* before = "(trait (id foo) x box x x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_TRAIT));
}


TEST(SugarTest, TraitWithoutCap)
{
  const char* before = "(trait (id foo) x x   x x)";
  const char* after  = "(trait (id foo) x ref x x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_TRAIT));
}


TEST(SugarTest, TypeParamWithConstraint)
{
  const char* before = "(typeparam (id foo) thistype x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_TYPEPARAM));
}


TEST(SugarTest, TypeParamWithoutConstraint)
{
  const char* before = "(typeparam (id foo) x x)";
  const char* after  = "(typeparam (id foo) (structural members tag x) x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_TYPEPARAM));
}


TEST(SugarTest, LetFieldInTrait)
{
  const char* before = "(trait (flet (id foo) x x))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_FLET, AST_ERROR));
}


TEST(SugarTest, LetFieldNotInTrait)
{
  const char* before = "(class (flet (id foo) x x))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_FLET));
}


TEST(SugarTest, VarFieldInTrait)
{
  const char* before = "(trait (fvar (id foo) x x))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_FVAR, AST_ERROR));
}


TEST(SugarTest, VarFieldNotInTrait)
{
  const char* before = "(class (fvar (id foo) x x))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_FVAR));
}


TEST(SugarTest, ConstructorNoNameNoReturnType)
{
  const char* before =
    "(class (id foo) x x x (members (new ref x x x x x x)))";

  const char* after =
    "(class (id foo) x x x"
    "  (members (new ref (id create) x x"
    "    (nominal x (id foo) x ref ^) x x)))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_NEW));
}


TEST(SugarTest, ConstructorInGenericClass)
{
  const char* before =
    "(class (id foo)"
    "  (typeparams"
    "    (typeparam (id A) (nominal (id B) x x x) x)"
    "    (typeparam (id C) x x)"
    "  ) x x (members (new ref (id bar) x x x x x)))";

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

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_NEW));
}


TEST(SugarTest, BehaviourInClass)
{
  const char* before = "(class (be tag (id foo) x x x x x))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_BE, AST_ERROR));
}


TEST(SugarTest, BehaviourInActor)
{
  const char* before =
    "(actor (id foo) x x x (members (be tag (id foo) x x x x x)))";

  const char* after =
    "(actor (id foo) x x x"
    "  (members (be tag (id foo) x x"
    "    (nominal x (id foo) x tag x) x x)))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_BE));
}


TEST(SugarTest, FunctionIso)
{
  const char* before = "(fun iso (id foo) x x x x x)";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_FUN, AST_ERROR));
}


TEST(SugarTest, FunctionTrn)
{
  const char* before = "(fun trn (id foo) x x x x x)";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_FUN, AST_ERROR));
}


TEST(SugarTest, FunctionComplete)
{
  const char* before =
    "(fun ref (id foo) x x (nominal x (id U32) x x x) x (seq 3))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_FUN));
}


TEST(SugarTest, FunctionNoName)
{
  const char* before =
    "(fun ref x x x (nominal x (id U32) x x x) x (seq 3))";

  const char* after =
    "(fun ref (id apply) x x (nominal x (id U32) x x x) x (seq 3))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_FUN));
}


TEST(SugarTest, FunctionNoReturnNoBody)
{
  const char* before =
    "(fun ref (id foo) x x x x x)";

  const char* after =
    "(fun ref (id foo) x x (nominal x (id None) x x x) x x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_FUN));
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

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_FUN));
}


TEST(SugarTest, NominalWithPackage)
{
  const char* before = "(nominal (id foo) (id bar) x x x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_NOMINAL));
}


TEST(SugarTest, NominalWithoutPackage)
{
  const char* before = "(nominal (id foo) x x x x)";
  const char* after =  "(nominal x (id foo) x x x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_NOMINAL));
}


TEST(SugarTest, StructuralWithCapability)
{
  const char* before = "(structural members box x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_STRUCTURAL));
}


TEST(SugarTest, StructuralWithoutCapabilityInTypeParam)
{
  const char* before = "(typeparam (id foo) (structural members x x) x)";
  const char* after =  "(typeparam (id foo) (structural members tag x) x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_STRUCTURAL));
}


TEST(SugarTest, StructuralWithoutCapabilityNotInTypeParam)
{
  const char* before = "(structural members x x)";
  const char* after =  "(structural members ref x)";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_STRUCTURAL));
}


TEST(SugarTest, IfWithoutElse)
{
  const char* before = "(if (seq 3) (seq 1) x)";
  const char* after =  "(if (seq 3) (seq 1) (seq (reference (id None))))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_IF));
}


TEST(SugarTest, IfWithElse)
{
  const char* before = "(if (seq 3) (seq 1) (seq 2))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_IF));
}


TEST(SugarTest, TryWithoutElseOrThen)
{
  const char* before = "(try (seq 1) x x)";
  const char* after =
    "(try (seq 1) (seq (reference (id None))) (seq (reference (id None))))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_TRY));
}


TEST(SugarTest, TryWithoutElse)
{
  const char* before = "(try (seq 1) x (seq 3))";
  const char* after =
    "(try (seq 1) (seq (reference (id None))) (seq 3))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_TRY));
}


TEST(SugarTest, TryWithoutThen)
{
  const char* before = "(try (seq 1) (seq 2) x)";
  const char* after =
    "(try (seq 1) (seq 2) (seq (reference (id None))))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_TRY));
}


/*
TEST(SugarTest, ForWithoutElse)
{
  const char* before = "(for (idseq (id i)) x (seq 3) (seq 4) x)";
  const char* after =
    "(while:scope"
    "  (call (. (reference (id s0)) (id next)))"
    "  (seq:scope"
    "    (= (var (idseq (id i)) x) (seq 3))"
    "    (seq 4)"
    "  )"
    "  (seq (reference (id None)))"
    ")";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, after, TK_FOR));
}
*/


// Pure type checking "sugar"

TEST(SugarTest, ViewpointGood)
{
  const char* before = "(-> thistype (nominal x (id A) x x x))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_ARROW));
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

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
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

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
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

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftStructural)
{
  const char* before =
    "(-> (structural members x x) (nominal x (id A) x x x))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftNominalWithCapability)
{
  const char* before =
    "(-> (nominal x (id A) x ref x) (nominal x (id B) x x x))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
}


TEST(SugarTest, ViewpointLeftNominalEphemeral)
{
  const char* before =
    "(-> (nominal x (id A) x x ^) (nominal x (id B) x x x))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
}


TEST(SugarTest, ViewpointRightUnionType)
{
  const char* before =
    "(->"
    "  (nominal x (id A) x x x)"
    "  (uniontype"
    "    (nominal x (id B) x x x)"
    "    (nominal x (id C) x x x)))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
}


TEST(SugarTest, ViewpointRightIntersectionType)
{
  const char* before =
    "(->"
    "  (nominal x (id A) x x x)"
    "  (isecttype"
    "    (nominal x (id B) x x x)"
    "    (nominal x (id C) x x x)))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
}


TEST(SugarTest, ViewpointRightTupleType)
{
  const char* before =
    "(->"
    "  (nominal x (id A) x x x)"
    "  (tupletype"
    "    (nominal x (id B) x x x)"
    "    (nominal x (id C) x x x)))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
}


TEST(SugarTest, ViewpointRightThis)
{
  const char* before = "(-> (nominal x (id A) x x x) thistype)";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_ARROW, AST_ERROR));
}


TEST(SugarTest, ThisTypeNotInViewpoint)
{
  const char* before = "(tupletype thistype (nominal x (id A) x x x))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_THISTYPE, AST_ERROR));
}


TEST(SugarTest, ThisTypeInViewpointNotInMethod)
{
  const char* before = "(-> thistype (nominal x (id A) x x x))";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_THISTYPE, AST_ERROR));
}


TEST(SugarTest, ThisTypeInViewpointInMethod)
{
  const char* before =
    "(fun x (id foo) x x x x (-> thistype (nominal x (id A) x x x)))";

  ASSERT_NO_FATAL_FAILURE(test_good_sugar(before, before, TK_THISTYPE));
}


TEST(SugarTest, EphemeralNotInMethodReturnType)
{
  const char* before =
    "(fun x (id foo) x"
    "  (params (param (id bar) (nominal x (id A) x x ^) x))"
    "  x x x)";

  ASSERT_NO_FATAL_FAILURE(test_bad_sugar(before, TK_HAT, AST_ERROR));
}
