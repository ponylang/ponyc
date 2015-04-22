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
  "primitive U8\n"
  "primitive I8\n"
  "  fun neg():I8 => compiler_intrinsic\n"
  "primitive U16\n"
  "primitive I16\n"
  "  fun neg():I16 => compiler_intrinsic\n"
  "primitive U32\n"
  "primitive I32\n"
  "  fun neg():I32 => compiler_intrinsic\n"
  "primitive U64\n"
  "primitive I64\n"
  "  fun neg():I64 => compiler_intrinsic\n"
  "primitive U128\n"
  "primitive I128\n"
  "  fun neg():I128 => compiler_intrinsic\n"
  "primitive F32\n"
  "primitive F64\n"
  "primitive None\n"
  "primitive Bool";


// class PassTest

void PassTest::SetUp()
{
  program = NULL;
  package = NULL;
  module = NULL;
  walk_ast = NULL;
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
  module = NULL;
  walk_ast = NULL;
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


size_t PassTest::ref_count(ast_t* ast, const char* name)
{
  if(ast == NULL)
    return 0;

  size_t count = 0;
  symtab_t* symtab = ast_get_symtab(ast);

  if(symtab != NULL && symtab_find(symtab, stringtab(name), NULL) != NULL)
    count = 1;

  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    count += ref_count(p, name);

  return count;
}



// Test methods

void PassTest::test_compile(const char* src, const char* pass)
{
  package_add_magic("builtin", _builtin_src);
  package_add_magic(_first_pkg_path, src);

  DO(load_test_program(pass, _first_pkg_path, &program));
  package = ast_child(program);
  module = ast_child(package);
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


void PassTest::lookup(ast_t* ast, const char* name, token_id id)
{
  ASSERT_NE((void*)NULL, ast);
  symtab_t* symtab = ast_get_symtab(ast);

  if(symtab == NULL)
  {
    printf("Cannot lookup \"%s\", current node has no symbol table\n", name);
    ASSERT_TRUE(false);
  }

  walk_ast = (ast_t*)symtab_find(symtab, stringtab(name), NULL);

  if(walk_ast == NULL)
  {
    printf("Expected symtab entry for \"%s\" not found\n", name);
    ASSERT_TRUE(false);
  }

  ast_t* name_node;

  switch(id)
  {
    case TK_ACTOR:
    case TK_CLASS:
    case TK_PRIMITIVE:
    case TK_TRAIT:
    case TK_INTERFACE:
    case TK_TYPE:
    case TK_FVAR:
    case TK_FLET:
    case TK_TYPEPARAM:
    case TK_PARAM:
      name_node = ast_child(walk_ast);
      break;

    case TK_BE:
    case TK_FUN:
    case TK_NEW:
      name_node = ast_childidx(walk_ast, 1);
      break;

    case TK_ID:
      name_node = walk_ast;
      break;

    default:
      printf("Don't know how to find name node for token \"%s\"\n",
        token_id_desc(id));
      ASSERT_TRUE(false);
  }

  ASSERT_NE((void*)NULL, name_node);
  ASSERT_EQ(TK_ID, ast_id(name_node));
  ASSERT_STREQ(name, ast_name(name_node));
}


void PassTest::lookup_null(ast_t* ast, const char* name)
{
  ASSERT_NE((void*)NULL, ast);
  symtab_t* symtab = ast_get_symtab(ast);

  if(symtab == NULL)
  {
    printf("Cannot lookup \"%s\", current node has no symbol table\n", name);
    ASSERT_TRUE(false);
  }

  ast_t* null_ast = (ast_t*)symtab_find(symtab, stringtab(name), NULL);

  if(null_ast != NULL)
  {
    printf("Unexpectedly found symtab entry for \"%s\":\n", name);
    ast_print(null_ast);
    ASSERT_TRUE(false);
  }
}


void PassTest::child(size_t index)
{
  ASSERT_NE((void*)NULL, walk_ast);

  if(index >= ast_childcount(walk_ast))
  {
    printf("Cannot find child index %lu, only %lu children present\n", index,
      ast_childcount(walk_ast));
    ASSERT_TRUE(false);
  }

  walk_ast = ast_childidx(walk_ast, index);
  ASSERT_NE((void*)NULL, walk_ast);
}
