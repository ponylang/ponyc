#include <gtest/gtest.h>
#include <platform.h>

#include <ponyc.h>
#include <ast/ast.h>
#include <ast/lexer.h>
#include <ast/source.h>
#include <ast/stringtab.h>
#include <pkg/package.h>
#include <../libponyrt/mem/pool.h>

#include "util.h"
#include <string.h>
#include <string>
#include <assert.h>

using std::string;


static const char* _builtin =
  "primitive U8 is Real[U8]\n"
  "  new create() => 0\n"
  "  fun mul(a: U8): U8 => 0\n"
  "primitive I8 is Real[I8]"
  "  new create() => 0\n"
  "  fun neg():I8 => -this\n"
  "primitive U16 is Real[U16]"
  "  new create() => 0\n"
  "primitive I16 is Real[I16]"
  "  new create() => 0\n"
  "  fun neg():I16 => -this\n"
  "  fun mul(a: I16): I16 => 0\n"
  "primitive U32 is Real[U32]"
  "  new create() => 0\n"
  "primitive I32 is Real[I32]"
  "  new create() => 0\n"
  "  fun neg():I32 => -this\n"
  "primitive U64 is Real[U64]"
  "  new create() => 0\n"
  "primitive I64 is Real[I64]"
  "  new create() => 0\n"
  "  fun neg():I64 => -this\n"
  "  fun mul(a: I64): I64 => 0\n"
  "  fun op_or(a: I64): I64 => 0\n"
  "  fun op_and(a: I64): I64 => 0\n"
  "  fun op_xor(a: I64): I64 => 0\n"
  "primitive U128 is Real[U128]"
  "  new create() => 0\n"
  "  fun mul(a: U128): U128 => 0\n"
  "  fun div(a: U128): U128 => 0\n"
  "primitive I128 is Real[I128]"
  "  new create() => 0\n"
  "  fun neg():I128 => -this\n"
  "primitive ULong is Real[ULong]"
  "  new create() => 0\n"
  "primitive ILong is Real[ILong]"
  "  new create() => 0\n"
  "  fun neg():ILong => -this\n"
  "primitive USize is Real[USize]"
  "  new create() => 0\n"
  "primitive ISize is Real[ISize]"
  "  new create() => 0\n"
  "  fun neg():ISize => -this\n"
  "primitive F32 is Real[F32]"
  "  new create() => 0\n"
  "primitive F64 is Real[F64]"
  "  new create() => 0\n"
  "type Number is (Signed | Unsigned | Float)\n"
  "type Signed is (I8 | I16 | I32 | I64 | I128 | ILong | ISize)\n"
  "type Unsigned is (U8 | U16 | U32 | U64 | U128 | ULong | USize)\n"
  "type Float is (F32 | F64)\n"
  "trait val Real[A: Real[A] val]\n"
  "class val Env\n"
  "primitive None\n"
  "primitive Bool\n"
  "class val String\n"
  "class Pointer[A]\n"
  "interface Seq[A]\n"
  // Fake up arrays and iterators enough to allow tests to
  // - create array literals
  // - call .values() iterator in a for loop
  "class Array[A] is Seq[A]\n"
  "  new create(len: USize, alloc: USize = 0) => true\n"
  "  fun ref push(value: A): Array[A]^ => this\n"
  "  fun values(): Iterator[A] => object ref\n"
  "    fun ref has_next(): Bool => false\n"
  "    fun ref next(): A ? => error\n"
  "  end\n"
  "interface Iterator[A]\n"
  "  fun ref has_next(): Bool\n"
  "  fun ref next(): A ?\n";


// Check whether the 2 given ASTs are identical
static bool compare_asts(ast_t* expected, ast_t* actual, errors_t *errors)
{
  assert(expected != NULL);
  assert(actual != NULL);

  token_id expected_id = ast_id(expected);
  token_id actual_id = ast_id(actual);

  if(expected_id != actual_id)
  {
    ast_error(errors, expected,
      "AST ID mismatch, got %d (%s), expected %d (%s)",
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
    ast_error(errors, expected, "AST text mismatch, got %s, expected %s",
      ast_get_print(actual), ast_get_print(expected));
    return false;
  }

  if(ast_has_scope(expected) && !ast_has_scope(actual))
  {
    ast_error(errors, expected, "AST missing scope");
    return false;
  }

  if(!ast_has_scope(expected) && ast_has_scope(actual))
  {
    ast_error(errors, actual, "Unexpected AST scope");
    return false;
  }

  // Check types
  ast_t* expected_type = ast_type(expected);
  ast_t* actual_type = ast_type(actual);

  if(expected_type != NULL || actual_type != NULL)
  {
    if(expected_type == NULL)
    {
      ast_error(errors, actual, "Unexpected type found, %s",
        ast_get_print(actual_type));
      return false;
    }

    if(actual_type == NULL)
    {
      ast_error(errors, actual, "Expected type not found, %s",
        ast_get_print(expected_type));
      return false;
    }

    if(!compare_asts(expected_type, actual_type, errors))
      return false;
  }

  // Check children
  ast_t* expected_child = ast_child(expected);
  ast_t* actual_child = ast_child(actual);

  while(expected_child != NULL && actual_child != NULL)
  {
    if(!compare_asts(expected_child, actual_child, errors))
      return false;

    expected_child = ast_sibling(expected_child);
    actual_child = ast_sibling(actual_child);
  }

  if(expected_child != NULL)
  {
    ast_error(errors, expected, "Expected child %s not found",
      ast_get_print(expected));
    return false;
  }

  if(actual_child != NULL)
  {
    ast_error(errors, actual, "Unexpected child node found, %s",
      ast_get_print(actual));
    return false;
  }

  return true;
}


// class PassTest

void PassTest::SetUp()
{
  pass_opt_init(&opt);
  codegen_init(&opt);
  package_init(&opt);
  program = NULL;
  package = NULL;
  module = NULL;
  compile = NULL;
  _builtin_src = _builtin;
  _first_pkg_path = "prog";
  package_clear_magic();
  package_suppress_build_message();
}


void PassTest::TearDown()
{
  if(compile != NULL)
  {
    codegen_cleanup(compile);
    POOL_FREE(compile_t, compile);
    compile = NULL;
  }
  ast_free(program);
  program = NULL;
  package = NULL;
  module = NULL;
  package_done();
  codegen_shutdown(&opt);
  pass_opt_done(&opt);
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

  errors_t *errors = errors_alloc();

  if(!compare_asts(expect, actual, errors))
  {
    printf("Expected:\n");
    ast_print(expect);
    printf("Got:\n");
    ast_print(actual);
    errors_print(errors);
    ASSERT_TRUE(false);
  }

  errors_free(errors);
}


void PassTest::test_compile(const char* src, const char* pass)
{
  DO(build_package(pass, src, _first_pkg_path, true, NULL));

  package = ast_child(program);
  module = ast_child(package);
}


void PassTest::test_error(const char* src, const char* pass)
{
  DO(build_package(pass, src, _first_pkg_path, false, NULL));

  package = NULL;
  module = NULL;
  ASSERT_EQ((void*)NULL, program);
}


void PassTest::test_expected_errors(const char* src, const char* pass,
  const char** errors)
{
  DO(build_package(pass, src, _first_pkg_path, false, errors));

  package = NULL;
  module = NULL;
  ASSERT_EQ((void*)NULL, program);
}


void PassTest::test_equiv(const char* actual_src, const char* actual_pass,
  const char* expect_src, const char* expect_pass)
{
  DO(test_compile(actual_src, actual_pass));
  ast_t* expect_ast;

  DO(build_package(expect_pass, expect_src, "expect", true, NULL));
  expect_ast = program;
  ast_t* expect_package = ast_child(expect_ast);

  DO(check_ast_same(expect_package, package));
  ast_free(expect_ast);
  program = NULL;
}


ast_t* PassTest::type_of(const char* name)
{
  assert(name != NULL);
  assert(program != NULL);
  return type_of_within(program, name);
}


ast_t* PassTest::numeric_literal(uint64_t num)
{
  assert(program != NULL);
  return numeric_literal_within(program, num);
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
  const char* package_name, bool check_good, const char** expected_errors)
{
  ASSERT_NE((void*)NULL, pass);
  ASSERT_NE((void*)NULL, src);
  ASSERT_NE((void*)NULL, package_name);

  lexer_allow_test_symbols();

  package_add_magic("builtin", _builtin_src);

  package_add_magic(package_name, src);

  package_suppress_build_message();

  limit_passes(&opt, pass);
  program = program_load(stringtab(package_name), &opt);

  if((program != NULL) && (opt.limit >= PASS_REACH))
  {
    compile = POOL_ALLOC(compile_t);

    if(!codegen_gen_test(compile, program, &opt))
    {
      codegen_cleanup(compile);
      POOL_FREE(compile_t, compile);
      compile = NULL;
    }
  }

  if(expected_errors != NULL)
  {
    const char* expected_error = *expected_errors;
    size_t expected_count = 0;
    while(expected_error != NULL)
    {
      expected_count++;
      expected_error = expected_errors[expected_count];
    }

    ASSERT_EQ(expected_count, errors_get_count(opt.check.errors));

    errormsg_t* e = errors_get_first(opt.check.errors);
    for(size_t i = 0; i < expected_count; i++)
    {
      expected_error = expected_errors[i];

      if((e == NULL) || (expected_error == NULL))
        break;

      EXPECT_TRUE(strstr(e->msg, expected_error) != NULL)
        << "Actual error: " << e->msg;
      e = e->next;
    }
  }

  if(check_good)
  {
    if(program != NULL)
      errors_print(opt.check.errors);

    ASSERT_NE((void*)NULL, program);
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


ast_t* PassTest::numeric_literal_within(ast_t* ast, uint64_t num)
{
  assert(ast != NULL);

  // Is this node the definition we're looking for?
  if (ast_id(ast) == TK_INT && lexint_cmp64(ast_int(ast), num) == 0)
  {
    return ast;
  }

  // Check children.
  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
  {
    ast_t* r = numeric_literal_within(p, num);

    if(r != NULL)
      return r;
  }

  // Not found.
  return NULL;
}
