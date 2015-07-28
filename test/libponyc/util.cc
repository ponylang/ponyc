#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <ast/source.h>
#include <ast/stringtab.h>
#include <pass/pass.h>
#include <pkg/package.h>

#include "util.h"
#include <string.h>
#include <string>

using std::string;


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
  prog_type_1 = NULL;
  prog_type_2 = NULL;
  prog_type_3 = NULL;
  prog_type_4 = NULL;
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
  prog_type_1 = NULL;
  prog_type_2 = NULL;
  prog_type_3 = NULL;
  prog_type_4 = NULL;
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

  bool r = build_compare_asts(expect, actual);

  if(!r)
  {
    printf("Expected:\n");
    ast_print(expect);
    printf("Got:\n");
    ast_print(actual);
    print_errors();
  }

  ASSERT_TRUE(r);
}


void PassTest::generate_types(const char* extra_src, const char* type1,
  const char* type2, const char* type3, const char* type4)
{
  ASSERT_NE((void*)NULL, extra_src);

  string t1 = (type1 == NULL) ? "None" : type1;
  string t2 = (type2 == NULL) ? "None" : type2;
  string t3 = (type3 == NULL) ? "None" : type3;
  string t4 = (type4 == NULL) ? "None" : type4;

  // First setup our program source
  string program =
    "actor GenTypes\n"
    "  fun f("
    "    t1: " + t1 + ","
    "    t2: " + t2 + ","
    "    t3: " + t3 + ","
    "    t4: " + t4 + ")\n"
    "  =>\n"
    "    None\n"
    "\n";

  program += extra_src;

  // Next build our program
  DO(test_compile(program.c_str(), "expr"));

  // Finally, extract the resulting type ASTs
  DO(lookup_member("GenTypes", "f", TK_FUN));
  DO(child(3); check(TK_PARAMS));
  ast_t* vars = walk_ast;

  DO(child(0); check(TK_PARAM); child(1));
  prog_type_1 = walk_ast;

  DO(walk(vars); child(1); check(TK_PARAM); child(1));
  prog_type_2 = walk_ast;

  DO(walk(vars); child(2); check(TK_PARAM); child(1));
  prog_type_3 = walk_ast;

  DO(walk(vars); child(3); check(TK_PARAM); child(1));
  prog_type_4 = walk_ast;
}


// Test methods

void PassTest::test_compile(const char* src, const char* pass)
{
  package_add_magic("builtin", _builtin_src);
  DO(build_package(pass, src, _first_pkg_path, true, &program));

  package = ast_child(program);
  module = ast_child(package);
}


void PassTest::test_error(const char* src, const char* pass)
{
  package_add_magic("builtin", _builtin_src);
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


// Walk methods

void PassTest::walk(ast_t* start)
{
  ASSERT_NE((void*)NULL, start);
  walk_ast = start;
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


void PassTest::sibling()
{
  ASSERT_NE((void*)NULL, walk_ast);
  walk_ast = ast_sibling(walk_ast);
}


void PassTest::parent()
{
  ASSERT_NE((void*)NULL, walk_ast);
  walk_ast = ast_parent(walk_ast);
}


void PassTest::type()
{
  ASSERT_NE((void*)NULL, walk_ast);
  walk_ast = ast_type(walk_ast);
}


void PassTest::check(token_id expected_id)
{
  ASSERT_NE((void*)NULL, walk_ast);
  ASSERT_EQ(expected_id, ast_id(walk_ast));
}


// Lookup methods

void PassTest::lookup_in(ast_t* ast, const char* name)
{
  ASSERT_NE((void*)NULL, ast);
  ASSERT_NE((void*)NULL, name);

  walk_ast = NULL;
  symtab_t* symtab = ast_get_symtab(ast);

  if(symtab == NULL)
  {
    printf("Cannot lookup \"%s\", node has no symbol table\n", name);
    ASSERT_TRUE(false);
  }

  walk_ast = (ast_t*)symtab_find(symtab, stringtab(name), NULL);
}


void PassTest::lookup_in(ast_t* ast, const char* name, token_id expected_id)
{
  DO(lookup_in(ast, name));

  if(walk_ast == NULL)
  {
    printf("Expected symtab entry for \"%s\" not found\n", name);
    ASSERT_TRUE(false);
  }

  ASSERT_EQ(expected_id, ast_id(walk_ast));
}


void PassTest::lookup(const char* name)
{
  ASSERT_NE((void*)NULL, walk_ast);
  lookup_in(walk_ast, name);
}


void PassTest::lookup(const char* name, token_id expected_id)
{
  ASSERT_NE((void*)NULL, walk_ast);
  lookup_in(walk_ast, name, expected_id);
}


void PassTest::lookup_type(const char* name)
{
  ASSERT_NE((void*)NULL, name);
  ASSERT_NE((void*)NULL, package);

  walk_ast = NULL;
  symtab_t* symtab = ast_get_symtab(package);
  ASSERT_NE((void*)NULL, symtab);

  walk_ast = (ast_t*)symtab_find(symtab, stringtab(name), NULL);
}


void PassTest::lookup_type(const char* name, token_id expected_id)
{
  ASSERT_NE((void*)NULL, name);
  ASSERT_NE((void*)NULL, package);

  DO(lookup_type(name));

  if(walk_ast == NULL)
  {
    printf("Expected type \"%s\" not found\n", name);
    ASSERT_TRUE(false);
  }

  ASSERT_EQ(expected_id, ast_id(walk_ast));
}


void PassTest::lookup_member(const char* type_name, const char* member_name)
{
  ASSERT_NE((void*)NULL, type_name);
  ASSERT_NE((void*)NULL, member_name);
  ASSERT_NE((void*)NULL, package);

  DO(lookup_type(type_name));
  ASSERT_NE((void*)NULL, walk_ast);

  // Lookup method
  symtab_t* symtab = ast_get_symtab(walk_ast);
  ASSERT_NE((void*)NULL, symtab);

  walk_ast = (ast_t*)symtab_find(symtab, stringtab(member_name), NULL);
}


void PassTest::lookup_member(const char* type_name, const char* member_name,
  token_id expected_id)
{
  ASSERT_NE((void*)NULL, type_name);
  ASSERT_NE((void*)NULL, member_name);

  DO(lookup_member(type_name, member_name));

  if(walk_ast == NULL)
  {
    printf("Expected member \"%s.%s\" not found\n", type_name, member_name);
    ASSERT_TRUE(false);
  }

  ASSERT_EQ(expected_id, ast_id(walk_ast));
}


// Private methods

void PassTest::build_package(const char* pass, const char* src,
  const char* package_name, bool check_good, ast_t** out_package)
{
  ASSERT_NE((void*)NULL, pass);
  ASSERT_NE((void*)NULL, src);
  ASSERT_NE((void*)NULL, package_name);
  ASSERT_NE((void*)NULL, out_package);

  package_add_magic(package_name, src);

  free_errors();
  package_suppress_build_message();

  pass_opt_t opt;
  pass_opt_init(&opt);
  limit_passes(&opt, pass);
  *out_package = program_load(stringtab(package_name), &opt);
  pass_opt_done(&opt);

  if(check_good)
  {
    if(*out_package == NULL)
      print_errors();

    ASSERT_NE((void*)NULL, *out_package);
  }
}
