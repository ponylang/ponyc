#include <platform/platform.h>

PONY_EXTERN_C_BEGIN
#include <ast/ast.h>
#include <ast/builder.h>
#include <ast/source.h>
#include <ds/stringtab.h>
#include <pkg/package.h>
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

  if(ast == NULL)
    print_errors();

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
  ast_print(ast);

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
  ASSERT_EQ(expected, symtab_get(symtab, stringtab(name), NULL));
}
*/

void check_symtab_entry(ast_t* scope, const char* name, const char* expected)
{
  ASSERT_NE((void*)NULL, scope);
  ASSERT_NE((void*)NULL, name);

  symtab_t* symtab = ast_get_symtab(scope);
  ASSERT_NE((void*)NULL, symtab);

  void* entry = symtab_get(symtab, stringtab(name), NULL);

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


void check_tree(const char* expected, ast_t* actual_ast)
{
  ASSERT_NE((void*)NULL, actual_ast);

  // Build the expected description into an AST
  builder_t* expect_builder;
  ast_t* expect_ast;
  DO(build_ast_from_string(expected, &expect_ast, &expect_builder));

  bool r = build_compare_asts(expect_ast, actual_ast);

  if(!r)
  {
    // Well that didn't work as expected then
    printf("Expected:\n");
    ast_print(expect_ast);
    printf("Got:\n");
    ast_print(actual_ast);
  }

  ASSERT_TRUE(r);

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


void test_pass_fn_good(const char* before, const char* after,
  ast_result_t(*fn)(ast_t**), const char* label)
{
  free_errors();

  // Build the before description into an AST
  builder_t* actual_builder;
  ast_t* actual_ast;
  DO(build_ast_from_string(before, &actual_ast, &actual_builder));

  ast_t* tree = builder_find_sub_tree(actual_builder, label);
  if(tree == NULL)  // Given label not defined, use whole tree
    tree = builder_get_root(actual_builder);

  ASSERT_NE((void*)NULL, tree);

  // Apply transform
  ASSERT_EQ(AST_OK, fn(&tree));

  // Check resulting AST
  actual_ast = builder_get_root(actual_builder);

  DO(check_tree(after, actual_ast));

  builder_free(actual_builder);
}


void test_pass_fn_bad(const char* before, ast_result_t(*fn)(ast_t**),
  const char* label, ast_result_t expect_result)
{
  builder_t* actual_builder;
  ast_t* actual_ast;
  DO(build_ast_from_string(before, &actual_ast, &actual_builder));

  ast_t* tree = builder_find_sub_tree(actual_builder, label);
  if(tree == NULL)  // Given label not defined, use whole tree
    tree = builder_get_root(actual_builder);

  ASSERT_NE((void*)NULL, tree);

  ASSERT_EQ(expect_result, fn(&tree));

  builder_free(actual_builder);
}
