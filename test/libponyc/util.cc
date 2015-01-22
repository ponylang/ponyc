#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <ast/source.h>
#include <ast/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>

#include "util.h"

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


void load_test_program(const char* pass, const char* name, ast_t** out_prog)
{
  free_errors();

  package_suppress_build_message();

  pass_opt_t opt;
  pass_opt_init(&opt);
  limit_passes(&opt, pass);
  ast_t* program = program_load(stringtab(name), &opt);
  pass_opt_done(&opt);

  if(program == NULL)
    print_errors();

  ASSERT_NE((void*)NULL, program);

  *out_prog = program;
}


static const char* _builtin =
  "primitive U32\n"
  "primitive None\n"
  "primitive Bool";


// class PassTest

void PassTest::SetUp()
{
  program = NULL;
  package = NULL;
  _builtin_src = _builtin;
  _first_pkg_path = "prog";
  package_clear_magic();
  package_suppress_build_message();
  free_errors();
}


void PassTest::TearDown()
{
  ast_free(program);
  program = NULL;
  package = NULL;
}


void PassTest::set_builtin(const char* src)
{
  _builtin_src = src;
}


void PassTest::add_package(const char* path, const char* src)
{
  package_add_magic(path, src);
}


void PassTest::default_package_name(const char* path)
{
  _first_pkg_path = path;
}


// Test methods

void PassTest::test_compile(const char* src, const char* pass)
{
  package_add_magic("builtin", _builtin_src);
  package_add_magic(_first_pkg_path, src);

  DO(load_test_program(pass, _first_pkg_path, &program));
  package = ast_child(program);
}


void PassTest::test_error(const char* src, const char* pass)
{
  package_add_magic("builtin", _builtin_src);
  package_add_magic(_first_pkg_path, src);
  free_errors();

  pass_opt_t opt;
  pass_opt_init(&opt);
  limit_passes(&opt, pass);
  ast_t* program = program_load(stringtab(_first_pkg_path), &opt);
  pass_opt_done(&opt);

  ASSERT_EQ((void*)NULL, program);
  package = NULL;
}


void PassTest::test_equiv(const char* actual_src, const char* actual_pass,
  const char* expect_src, const char* expect_pass)
{
  DO(test_compile(actual_src, actual_pass));

  package_add_magic("expect", expect_src);
  ast_t* expect_ast;
  DO(load_test_program(expect_pass, "expect", &expect_ast));
  ast_t* expect_package = ast_child(expect_ast);

  bool r = build_compare_asts(expect_package, package);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect_package);
    printf("Got:\n");
    ast_print(package);
    print_errors();
  }

  ASSERT_TRUE(r);

  ast_free(expect_ast);
}


void PassTest::test_program_ast(const char* src, const char* pass, const char* desc)
{
  DO(test_compile(src, pass));

  builder_t* expect_builder;
  ast_t* expect_ast;
  DO(build_ast_from_string(desc, &expect_ast, &expect_builder));

  bool r = build_compare_asts(expect_ast, program);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect_ast);
    printf("Got:\n");
    ast_print(program);
    print_errors();
  }

  ASSERT_TRUE(r);

  builder_free(expect_builder);
}


void PassTest::test_package_ast(const char* src, const char* pass, const char* desc)
{
  DO(test_compile(src, pass));

  builder_t* expect_builder;
  ast_t* expect_ast;
  DO(build_ast_from_string(desc, &expect_ast, &expect_builder));
  ast_t* expect_package = ast_child(expect_ast);

  bool r = build_compare_asts(expect_package, package);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect_package);
    printf("Got:\n");
    ast_print(package);
    print_errors();
  }

  ASSERT_TRUE(r);

  builder_free(expect_builder);
}
