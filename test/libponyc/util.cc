#include <gtest/gtest.h>
#include <platform.h>

#include <ponyc.h>
#include <ast/ast.h>
#include <ast/lexer.h>
#include <ast/source.h>
#include <ast/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>
#include <codegen/genjit.h>
#include <../libponyrt/pony.h>
#include <../libponyrt/mem/pool.h>
#include <ponyassert.h>

#include "util.h"
#include <string.h>
#include <string>

using std::string;


// These will be set when running a JIT'ed program.
extern "C"
{
  EXPORT_SYMBOL pony_type_t** __PonyDescTablePtr;
  EXPORT_SYMBOL size_t __PonyDescTableSize;
}


static const char* const _builtin =
  "primitive U8 is Real[U8]\n"
  "  new create(a: U8 = 0) => a\n"
  "  fun mul(a: U8): U8 => this * a\n"
  "primitive I8 is Real[I8]"
  "  new create(a: I8 = 0) => a\n"
  "  fun neg(): I8 => -this\n"
  "primitive U16 is Real[U16]"
  "  new create(a: U16 = 0) => a\n"
  "primitive I16 is Real[I16]"
  "  new create(a: I16 = 0) => a\n"
  "  fun neg(): I16 => -this\n"
  "  fun mul(a: I16): I16 => this * a\n"
  "primitive U32 is Real[U32]"
  "  new create(a: U32 = 0) => a\n"
  "primitive I32 is Real[I32]"
  "  new create(a: I32 = 0) => a\n"
  "  fun neg(): I32 => -this\n"
  "  fun mul(a: I32): I32 => this * a\n"
  "primitive U64 is Real[U64]"
  "  new create(a: U64 = 0) => a\n"
  "  fun op_xor(a: U64): U64 => this xor a\n"
  "  fun add(a: U64): U64 => this + a\n"
  "  fun lt(a: U64): Bool => this < a\n"
  "primitive I64 is Real[I64]"
  "  new create(a: I64 = 0) => a\n"
  "  fun neg():I64 => -this\n"
  "  fun mul(a: I64): I64 => this * a\n"
  "  fun op_or(a: I64): I64 => this or a\n"
  "  fun op_and(a: I64): I64 => this and a\n"
  "  fun op_xor(a: I64): I64 => this xor a\n"
  "primitive U128 is Real[U128]"
  "  new create(a: U128 = 0) => a\n"
  "  fun mul(a: U128): U128 => this * a\n"
  "  fun div(a: U128): U128 => this / a\n"
  "primitive I128 is Real[I128]"
  "  new create(a: I128 = 0) => a\n"
  "  fun neg(): I128 => -this\n"
  "primitive ULong is Real[ULong]"
  "  new create(a: ULong = 0) => a\n"
  "primitive ILong is Real[ILong]"
  "  new create(a: ILong = 0) => a\n"
  "  fun neg(): ILong => -this\n"
  "primitive USize is Real[USize]"
  "  new create(a: USize = 0) => a\n"
  "  fun u64(): U64 => compile_intrinsic\n"
  "primitive ISize is Real[ISize]"
  "  new create(a: ISize = 0) => a\n"
  "  fun neg(): ISize => -this\n"
  "primitive F32 is Real[F32]"
  "  new create(a: F32 = 0) => a\n"
  "primitive F64 is Real[F64]"
  "  new create(a: F64 = 0) => a\n"
  "type Number is (Signed | Unsigned | Float)\n"
  "type Signed is (I8 | I16 | I32 | I64 | I128 | ILong | ISize)\n"
  "type Unsigned is (U8 | U16 | U32 | U64 | U128 | ULong | USize)\n"
  "type Float is (F32 | F64)\n"
  "trait val Real[A: Real[A] val]\n"
  "class val Env\n"
  "  new _create(argc: U32, argv: Pointer[Pointer[U8]] val,\n"
  "    envp: Pointer[Pointer[U8]] val)\n"
  "  => None\n"
  "primitive None\n"
  "interface tag Any\n"
  "primitive Bool\n"
  "  new create(a: Bool) => a\n"
  "  fun op_and(a: Bool): Bool => this and a\n"
  "  fun op_not(): Bool => not this\n"
  "class val String\n"
  "struct Pointer[A]\n"
  "  new create() => compile_intrinsic\n"
  "  fun tag is_null(): Bool => compile_intrinsic\n"
  "interface Seq[A]\n"
  "  fun apply(index: USize): this->A ?\n"
  // Fake up arrays (and iterators) enough to allow tests to
  // - create array literals
  // - call .values() iterator in a for loop
  // - be a subtype of Seq
  // - call genprim_array_serialise_trace (which expects the three fields)
  "class Array[A] is Seq[A]\n"
  "  var _size: USize = 0\n"
  "  var _alloc: USize = 0\n"
  "  var _ptr: Pointer[A] = Pointer[A]\n"
  "  new create(len: USize, alloc: USize = 0) => true\n"
  "  fun ref push(value: A): Array[A]^ => this\n"
  "  fun apply(index: USize): this->A ? => error\n"
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
  pony_assert(expected != NULL);
  pony_assert(actual != NULL);

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
  codegen_pass_init(&opt);
  package_init(&opt);
  program = NULL;
  package = NULL;
  module = NULL;
  compile = NULL;
  _builtin_src = _builtin;
  _first_pkg_path = "prog";
  package_clear_magic(&opt);
  opt.verbosity = VERBOSITY_QUIET;
  opt.check_tree = true;
  opt.allow_test_symbols = true;
  last_pass = PASS_PARSE;
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
  last_pass = PASS_PARSE;
  package_done(&opt);
  codegen_pass_cleanup(&opt);
  pass_opt_done(&opt);
}


void PassTest::set_builtin(const char* src)
{
  _builtin_src = src;
}


void PassTest::add_package(const char* path, const char* src)
{
  package_add_magic_src(path, src, &opt);
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
    ast_print(expect, 80);
    printf("Got:\n");
    ast_print(actual, 80);
    errors_print(errors);
    ASSERT_TRUE(false);
  }

  errors_free(errors);
}


void PassTest::test_compile(const char* src, const char* pass)
{
  DO(build_package(pass, src, _first_pkg_path, true, NULL, false));

  package = ast_child(program);
  module = ast_child(package);
}


void PassTest::test_compile_resume(const char* pass)
{
  DO(build_package(pass, NULL, NULL, true, NULL, true));
}


void PassTest::test_error(const char* src, const char* pass)
{
  DO(build_package(pass, src, _first_pkg_path, false, NULL, false));

  package = NULL;
  module = NULL;
  ASSERT_EQ((void*)NULL, program);
}


void PassTest::test_expected_errors(const char* src, const char* pass,
  const char** errors)
{
  DO(build_package(pass, src, _first_pkg_path, false, errors, false));

  package = NULL;
  module = NULL;
  ASSERT_EQ((void*)NULL, program);
}


void PassTest::test_equiv(const char* actual_src, const char* actual_pass,
  const char* expect_src, const char* expect_pass)
{
  DO(build_package(expect_pass, expect_src, "expect", true, NULL, false));
  ast_t* expect_ast = program;
  ast_t* expect_package = ast_child(expect_ast);
  program = NULL;

  DO(test_compile(actual_src, actual_pass));

  DO(check_ast_same(expect_package, package));
  ast_free(expect_ast);
}


ast_t* PassTest::type_of(const char* name)
{
  pony_assert(name != NULL);
  pony_assert(program != NULL);
  return type_of_within(program, name);
}


ast_t* PassTest::numeric_literal(uint64_t num)
{
  pony_assert(program != NULL);
  return numeric_literal_within(program, num);
}


ast_t* PassTest::lookup_in(ast_t* ast, const char* name)
{
  pony_assert(ast != NULL);
  pony_assert(name != NULL);

  symtab_t* symtab = ast_get_symtab(ast);
  pony_assert(symtab != NULL);

  return symtab_find(symtab, stringtab(name), NULL);
}


ast_t* PassTest::lookup_type(const char* name)
{
  pony_assert(package != NULL);
  return lookup_in(package, name);
}


ast_t* PassTest::lookup_member(const char* type_name, const char* member_name)
{
  pony_assert(type_name != NULL);
  pony_assert(member_name != NULL);

  ast_t* type = lookup_type(type_name);
  pony_assert(type != NULL);

  return lookup_in(type, member_name);
}


bool PassTest::run_program(int* exit_code)
{
  pony_assert(compile != NULL);

  pony_exitcode(0);
  jit_symbol_t symbols[] = {
    {"__PonyDescTablePtr", &__PonyDescTablePtr, sizeof(pony_type_t**)},
    {"__PonyDescTableSize", &__PonyDescTableSize, sizeof(size_t)}};
  return gen_jit_and_run(compile, exit_code, symbols, 2);
}


// Private methods

void PassTest::build_package(const char* pass, const char* src,
  const char* package_name, bool check_good, const char** expected_errors,
  bool resume)
{
  ASSERT_NE((void*)NULL, pass);

  if(!resume)
  {
    ASSERT_NE((void*)NULL, src);
    ASSERT_NE((void*)NULL, package_name);

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
    last_pass = PASS_PARSE;

#ifndef PONY_PACKAGES_DIR
#  error Packages directory undefined
#else
    if(_builtin_src != NULL)
    {
      package_add_magic_src("builtin", _builtin_src, &opt);
    } else {
      char path[FILENAME_MAX];
      path_cat(PONY_PACKAGES_DIR, "builtin", path);
      package_add_magic_path("builtin", path, &opt);
    }
#endif

    package_add_magic_src(package_name, src, &opt);

    limit_passes(&opt, pass);
    program = program_load(stringtab(package_name), &opt);

    if((program != NULL) && (opt.limit >= PASS_REACH))
    {
      compile = POOL_ALLOC(compile_t);

      if(!codegen_gen_test(compile, program, &opt, PASS_PARSE))
      {
        codegen_cleanup(compile);
        POOL_FREE(compile_t, compile);
        ast_free(program);
        compile = NULL;
        program = NULL;
      }
    }
  } else {
    ASSERT_NE((void*)NULL, program);

    limit_passes(&opt, pass);

    if(ast_passes_program(program, &opt))
    {
      if(opt.limit >= PASS_REACH)
      {
        if(compile == NULL)
          compile = POOL_ALLOC(compile_t);

        if(!codegen_gen_test(compile, program, &opt, last_pass))
        {
          codegen_cleanup(compile);
          POOL_FREE(compile_t, compile);
          ast_free(program);
          compile = NULL;
          program = NULL;
        }
      }
    } else {
      ast_free(program);
      program = NULL;
    }
  }

  last_pass = opt.limit;

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
    errors_print(opt.check.errors);

    ASSERT_NE((void*)NULL, program);
  }
}


ast_t* PassTest::type_of_within(ast_t* ast, const char* name)
{
  pony_assert(ast != NULL);
  pony_assert(name != NULL);

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
  pony_assert(ast != NULL);

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


// Environment methods

void Environment::SetUp()
{
  codegen_llvm_init();
}

void Environment::TearDown()
{
  codegen_llvm_shutdown();
}


int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  testing::AddGlobalTestEnvironment(new Environment());
  return RUN_ALL_TESTS();
}
