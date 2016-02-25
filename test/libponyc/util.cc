#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/lexer.h>
#include <ast/source.h>
#include <ast/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>

#include "util.h"
#include <string.h>
#include <string>
#include <assert.h>

using std::string;


static const char* _builtin =
  "primitive U8\n"
  "  new create() => 0\n"
  "primitive I8\n"
  "  new create() => 0\n"
  "  fun neg():I8 => -this\n"
  "primitive U16\n"
  "  new create() => 0\n"
  "primitive I16\n"
  "  new create() => 0\n"
  "  fun neg():I16 => -this\n"
  "primitive U32\n"
  "  new create() => 0\n"
  "primitive I32\n"
  "  new create() => 0\n"
  "  fun neg():I32 => -this\n"
  "primitive U64\n"
  "  new create() => 0\n"
  "primitive I64\n"
  "  new create() => 0\n"
  "  fun neg():I64 => -this\n"
  "primitive U128\n"
  "  new create() => 0\n"
  "primitive I128\n"
  "  new create() => 0\n"
  "  fun neg():I128 => -this\n"
  "primitive ULong\n"
  "  new create() => 0\n"
  "primitive ILong\n"
  "  new create() => 0\n"
  "  fun neg():ILong => -this\n"
  "primitive USize\n"
  "  new create() => 0\n"
  "primitive ISize\n"
  "  new create() => 0\n"
  "  fun neg():ISize => -this\n"
  "primitive F32\n"
  "  new create() => 0\n"
  "primitive F64\n"
  "  new create() => 0\n"
  "primitive None\n"
  "primitive Bool\n"
  "class val String\n"
  "class Pointer[A]\n";


// Check whether the 2 given ASTs are identical
static bool compare_asts(ast_t* expected, ast_t* actual)
{
  assert(expected != NULL);
  assert(actual != NULL);

  token_id expected_id = ast_id(expected);
  token_id actual_id = ast_id(actual);

  if(expected_id != actual_id)
  {
    ast_error(expected, "AST ID mismatch, got %d (%s), expected %d (%s)",
      ast_id(actual), ast_get_print(actual),
      ast_id(expected), ast_get_print(expected));
    return false;
  }

  // Even in 2 ASTS that we consider to be the same, hygenic ids might not
  // match each other.
  // To catch all possible error cases we should keep a mapping of the hygenic
  // ids in the 2 trees and check for a 1 to 1 correlation.
  // For now just let all hygenic ids match each other. This will allow some
  // errors through, but it's much easier for now.
  if(ast_id(expected) == TK_ID && ast_name(actual)[0] == '$' &&
    ast_name(expected)[0] == '$')
  {
    // Allow expected and actual hygenic names to match.
  }
  else if(strcmp(ast_get_print(expected), ast_get_print(actual)) != 0)
  {
    ast_error(expected, "AST text mismatch, got %s, expected %s",
      ast_get_print(actual), ast_get_print(expected));
    return false;
  }

  if(ast_has_scope(expected) && !ast_has_scope(actual))
  {
    ast_error(expected, "AST missing scope");
    return false;
  }

  if(!ast_has_scope(expected) && ast_has_scope(actual))
  {
    ast_error(actual, "Unexpected AST scope");
    return false;
  }

  // Check types
  ast_t* expected_type = ast_type(expected);
  ast_t* actual_type = ast_type(actual);

  if(expected_type != NULL || actual_type != NULL)
  {
    if(expected_type == NULL)
    {
      ast_error(actual, "Unexpected type found, %s",
        ast_get_print(actual_type));
      return false;
    }

    if(actual_type == NULL)
    {
      ast_error(actual, "Expected type not found, %s",
        ast_get_print(expected_type));
      return false;
    }

    if(!compare_asts(expected_type, actual_type))
      return false;
  }

  // Check children
  ast_t* expected_child = ast_child(expected);
  ast_t* actual_child = ast_child(actual);

  while(expected_child != NULL && actual_child != NULL)
  {
    if(!compare_asts(expected_child, actual_child))
      return false;

    expected_child = ast_sibling(expected_child);
    actual_child = ast_sibling(actual_child);
  }

  if(expected_child != NULL)
  {
    ast_error(expected, "Expected child %s not found",
      ast_get_print(expected));
    return false;
  }

  if(actual_child != NULL)
  {
    ast_error(actual, "Unexpected child node found, %s",
      ast_get_print(actual));
    return false;
  }

  return true;
}


// class PassTest

void PassTest::SetUp()
{
  program = NULL;
  package = NULL;
  module = NULL;
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


void PassTest::check_ast_same(ast_t* expect, ast_t* actual)
{
  ASSERT_NE((void*)NULL, expect);
  ASSERT_NE((void*)NULL, actual);

  if(!compare_asts(expect, actual))
  {
    printf("Expected:\n");
    ast_print(expect);
    printf("Got:\n");
    ast_print(actual);
    print_errors();
    ASSERT_TRUE(false);
  }
}


void PassTest::test_compile(const char* src, const char* pass)
{
  DO(build_package(pass, src, _first_pkg_path, true, &program));

  package = ast_child(program);
  module = ast_child(package);
}


void PassTest::test_error(const char* src, const char* pass)
{
  DO(build_package(pass, src, _first_pkg_path, false, &program));

  package = NULL;
  module = NULL;
  ASSERT_EQ((void*)NULL, program);
}


void PassTest::test_equiv(const char* actual_src, const char* actual_pass,
  const char* expect_src, const char* expect_pass)
{
  DO(test_compile(actual_src, actual_pass));
  ast_t* expect_ast;

  DO(build_package(expect_pass, expect_src, "expect", true, &expect_ast));
  ast_t* expect_package = ast_child(expect_ast);

  DO(check_ast_same(expect_package, package));
  ast_free(expect_ast);
}


ast_t* PassTest::type_of(const char* name)
{
  assert(name != NULL);
  assert(program != NULL);
  return type_of_within(program, name);
}


ast_t* PassTest::lookup_in(ast_t* ast, const char* name)
{
  assert(ast != NULL);
  assert(name != NULL);

  symtab_t* symtab = ast_get_symtab(ast);
  assert(symtab != NULL);

  return symtab_find(symtab, stringtab(name), NULL);
}


ast_t* PassTest::lookup_type(const char* name)
{
  assert(package != NULL);
  return lookup_in(package, name);
}


ast_t* PassTest::lookup_member(const char* type_name, const char* member_name)
{
  assert(type_name != NULL);
  assert(member_name != NULL);

  ast_t* type = lookup_type(type_name);
  assert(type != NULL);

  return lookup_in(type, member_name);
}


// Private methods

void PassTest::build_package(const char* pass, const char* src,
  const char* package_name, bool check_good, ast_t** out_package)
{
  ASSERT_NE((void*)NULL, pass);
  ASSERT_NE((void*)NULL, src);
  ASSERT_NE((void*)NULL, package_name);
  ASSERT_NE((void*)NULL, out_package);

  pass_opt_t opt;
  pass_opt_init(&opt);
  package_init(&opt);

  lexer_allow_test_symbols();

  package_add_magic("builtin", _builtin_src);

  package_add_magic(package_name, src);

  package_suppress_build_message();

  limit_passes(&opt, pass);
  *out_package = program_load(stringtab(package_name), &opt);

  package_done(&opt, false);
  pass_opt_done(&opt);

  if(check_good)
  {
    if(*out_package == NULL)
      print_errors();

    ASSERT_NE((void*)NULL, *out_package);
  }

}


ast_t* PassTest::type_of_within(ast_t* ast, const char* name)
{
  assert(ast != NULL);
  assert(name != NULL);

  // Is this node the definition we're looking for?
  switch(ast_id(ast))
  {
    case TK_PARAM:
    case TK_LET:
    case TK_VAR:
    case TK_FLET:
    case TK_FVAR:
      if(strcmp(name, ast_name(ast_child(ast))) == 0)
      {
        // Match found
        return ast_childidx(ast, 1);
      }

      break;

    default:
      break;
  }

  // Check children.
  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
  {
    ast_t* r = type_of_within(p, name);

    if(r != NULL)
      return r;
  }

  // Not found.
  return NULL;
}
