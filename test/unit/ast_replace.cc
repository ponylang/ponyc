#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ast/token.h"
PONY_EXTERN_C_END

#include <gtest/gtest.h>


class AstReplaceTest: public testing::Test
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


static void test(const char* orig, token_id replace_at, const char* replace,
  const char* expect)
{
  source_t* orig_src = source_open_string(orig);
  ast_t* orig_ast = build_ast(orig_src);
  ASSERT_NE((void*)NULL, orig_ast);

  ast_t* tree = find_start(orig_ast, replace_at);
  ASSERT_NE((void*)NULL, tree);

  bool top = (tree == orig_ast);

  source_t* replace_src = source_open_string(replace);
  ast_t* replace_ast = build_ast(replace_src);
  ASSERT_NE((void*)NULL, replace_ast);

  ast_replace(&tree, replace_ast);

  if(top)
    orig_ast = tree;

  source_t* expect_src = source_open_string(expect);
  ast_t* expect_ast = build_ast(expect_src);
  ASSERT_NE((void*)NULL, expect_ast);

  bool r = build_compare_asts(expect_ast, orig_ast);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect_ast, 80);
    printf("\nGot:\n");
    ast_print(orig_ast, 80);
    printf("\n");
  }

  ASSERT_TRUE(r);

  ast_free(orig_ast);
  source_close(orig_src);
  source_close(replace_src);
  ast_free(expect_ast);
  source_close(expect_src);
}


TEST(AstReplaceTest, WholeTree)
{
  const char* orig = "(+ 1 2)";
  const char* replace = "(- 3 4)";

  ASSERT_NO_FATAL_FAILURE(test(orig, TK_PLUS, replace, replace));
}
