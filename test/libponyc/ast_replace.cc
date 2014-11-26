#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <ast/source.h>
#include <ast/token.h>

#include "util.h"

class AstReplaceTest: public testing::Test
{};

/*
static void test(const char* orig, const char* replace, const char* expect)
{
  builder_t* orig_builder;
  ast_t* orig_ast;
  DO(build_ast_from_string(orig, &orig_ast, &orig_builder));

  ast_t* tree = builder_find_sub_tree(orig_builder, "start");
  if(tree == NULL)  // No "start" node defined, use whole tree
    tree = builder_get_root(orig_builder);

  ASSERT_NE((void*)NULL, tree);

  builder_t* replace_builder;
  ast_t* replace_ast;
  DO(build_ast_from_string(replace, &replace_ast, &replace_builder));
  builder_extract_root(replace_builder);

  ast_replace(&tree, replace_ast);

  builder_t* expect_builder;
  ast_t* expect_ast;
  DO(build_ast_from_string(expect, &expect_ast, &expect_builder));

  ast_t* actual_ast = builder_get_root(orig_builder);
  bool r = build_compare_asts(expect_ast, actual_ast);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect_ast);
    printf("\nGot:\n");
    ast_print(actual_ast);
    printf("\n");
  }

  ASSERT_TRUE(r);

  builder_free(orig_builder);
  builder_free(replace_builder);
  builder_free(expect_builder);
}


TEST(AstReplaceTest, WholeTree)
{
  const char* orig = "(+ 1 2)\0";
  const char* replace = "(- 3 4)\0";

  ASSERT_NO_FATAL_FAILURE(test(orig, replace, replace));
}
*/
