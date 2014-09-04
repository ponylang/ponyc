#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ds/stringtab.h"
#include "../../src/libponyc/pkg/package.h"
PONY_EXTERN_C_END

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


ast_t* find_sub_tree(ast_t* tree, token_id sub_id)
{
  return find_start(tree, sub_id);
}


void build_ast_from_string(const char* desc, ast_t** out_ast,
  builder_t** out_builder)
{
  package_suppress_build_message();
  builder_t* builder = builder_create(desc);
  ast_t* ast = builder_get_root(builder);
  *out_builder = builder;

  if(out_ast != NULL)
    *out_ast = ast;

  ASSERT_NE((void*)NULL, ast);
}


/*
void free_build_ast(ast_t* ast, source_t* src)
{
  if(ast == NULL)
  {
    source_close(src);
    return;
  }

  printf("free %p\n", ast);
  ast_print(ast, 80);

  if(find_sub_tree(ast, TK_MODULE) == NULL)
    source_close(src);

  ast_free(ast);
}


ast_t* parse_test_module(const char* desc)
{
  source_t* src = source_open_string(desc);
  ast_t* ast = build_ast(src);

  if(ast == NULL)
    source_close(src);

  return ast;
}


void add_to_symbtab(ast_t* scope, const char* name, const char* tree_desc)
{
  source_t* src = source_open_string(tree_desc);
  ast_t* ast = build_ast(src);
  ASSERT_NE((void*)NULL, ast);

  ast_set(scope, stringtab(name), ast);
}


void symtab_entry(ast_t* tree, const char* name, ast_t* expected)
{
  symtab_t* symtab = ast_get_symtab(tree);
  ASSERT_NE((void*)NULL, symtab);
  ASSERT_EQ(expected, symtab_get(symtab, stringtab(name)));
}
*/

void check_symtab_entry(ast_t* scope, const char* name, const char* expected)
{
  ASSERT_NE((void*)NULL, scope);
  ASSERT_NE((void*)NULL, name);

  symtab_t* symtab = ast_get_symtab(scope);
  ASSERT_NE((void*)NULL, symtab);
  
  void* entry = symtab_get(symtab, stringtab(name));

  if(expected == NULL)
  {
    ASSERT_EQ((void*)NULL, entry);
    return;
  }

  ASSERT_NE((void*)NULL, entry);

  builder_t* builder;
  ast_t* expect_ast;
  DO(build_ast_from_string(expected, &expect_ast, &builder));

  ASSERT_TRUE(build_compare_asts_no_sibling(expect_ast, (ast_t*)entry));
}


void check_symtab_entry(builder_t* builder, const char* scope,
  const char* name, const char* expected)
{
  ASSERT_NE((void*)NULL, builder);
  ASSERT_NE((void*)NULL, scope);

  ast_t* scope_ast = builder_find_sub_tree(builder, scope);
  ASSERT_NE((void*)NULL, scope_ast);

  check_symtab_entry(scope_ast, name, expected);
}


void check_tree(const char* expected, ast_t* actual)
{
  ASSERT_NE((void*)NULL, actual);

  builder_t* expect_builder;
  ast_t* expect_ast;
  DO(build_ast_from_string(expected, &expect_ast, &expect_builder));

  ASSERT_TRUE(build_compare_asts_no_sibling(expect_ast, actual));

  builder_free(expect_builder);
}


void load_test_program(const char* name, ast_t** out_prog)
{
  free_errors();

  package_suppress_build_message();
  ast_t* program = program_load(stringtab(name));

  if(program == NULL)
    print_errors();

  ASSERT_NE((void*)NULL, program);

  *out_prog = program;
}


/*
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
*/
