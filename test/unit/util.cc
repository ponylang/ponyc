extern "C" {
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/ast/source.h"
}
#include "util.h"
#include <gtest/gtest.h>


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


ast_t* find_start(ast_t* tree, token_id start_id)
{
  ast_t* ast = find_start_internal(tree, start_id);

  if(ast == NULL)
    printf("Token id %d not found in tree\n", start_id);

  return ast;
}


void test_good_pass(const char* before, const char* after, token_id start_id,
  ast_result_t (*pass_fn)(ast_t**))
{
  free_errors();

  source_t* actual_src = source_open_string(before);
  ast_t* actual_ast = build_ast(actual_src);
  ASSERT_NE((void*)NULL, actual_ast);

  ast_t* tree = find_start(actual_ast, start_id);
  ASSERT_NE((void*)NULL, tree);

  bool top = (tree == actual_ast);

  source_t* expect_src = source_open_string(after);
  ast_t* expect_ast = build_ast(expect_src);
  ASSERT_NE((void*)NULL, expect_ast);

  ASSERT_EQ(AST_OK, pass_fn(&tree));

  if(top)
    actual_ast = tree;

  bool r = build_compare_asts(expect_ast, actual_ast);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect_ast, 80);
    printf("Got:\n");
    ast_print(actual_ast, 80);
  }

  ASSERT_TRUE(r);

  ast_free(actual_ast);
  source_close(actual_src);
  ast_free(expect_ast);
  source_close(expect_src);
}


void test_bad_pass(const char* desc, token_id start_id,
  ast_result_t expect_result, ast_result_t (*pass_fn)(ast_t**))
{
  free_errors();

  source_t* src = source_open_string(desc);
  ast_t* ast = build_ast(src);
  ASSERT_NE((void*)NULL, ast);

  ast_t* tree = find_start(ast, start_id);
  ASSERT_NE((void*)NULL, tree);

  ASSERT_EQ(expect_result, pass_fn(&tree));

  ast_free(ast);
  source_close(src);
}
