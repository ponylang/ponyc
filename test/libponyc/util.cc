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
#include <assert.h>

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


// Replace any underscores in the given type string with "DontCare" and append
// it to the other given string
static void append_type(const char* type, string* target)
{
  assert(type != NULL);
  assert(target != NULL);

  for(const char* p = type; *p != '\0'; p++)
  {
    if(*p == '_')
      target->append("DontCare");
    else
      target->push_back(*p);
  }
}


// Replace any nominal references to "DontCare" in the given AST with
// TK_DONTCAREs
static void replace_dontcares(ast_t* ast)
{
  assert(ast != NULL);

  if(ast_id(ast) == TK_NOMINAL)
  {
    if(strcmp(ast_name(ast_childidx(ast, 1)), "DontCare") == 0)
    {
      ast_erase(ast);
      ast_setid(ast, TK_DONTCARE);
      return;
    }
  }

  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    replace_dontcares(p);
}


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
  const char* type2, const char* type3,
  const char* typeparam1_name, const char* tp1_constraint,
  const char* typeparam2_name, const char* tp2_constraint)
{
  assert(extra_src != NULL);
  assert(type1 != NULL);

  if(type2 == NULL) type2 = "None";
  if(type3 == NULL) type3 = "None";
  if(typeparam1_name == NULL) typeparam1_name = "TP1";
  if(typeparam2_name == NULL) typeparam2_name = "TP2";

  // First setup our program source
  string prog_src =
    "primitive DontCare\n"
    "actor GenTypes[";

  prog_src += typeparam1_name;

  if(tp1_constraint != NULL)
    prog_src += string(": ") + tp1_constraint;

  prog_src += string(", ") + typeparam2_name;

  if(tp2_constraint != NULL)
    prog_src += string(": ") + tp2_constraint;

  prog_src += "]\n";
  prog_src += "  fun f(p1: ";
  append_type(type1, &prog_src);
  prog_src += ", p2: ";
  append_type(type2, &prog_src);
  prog_src += ", p3: ";
  append_type(type3, &prog_src);
  prog_src += ") => None\n\n";
  prog_src += extra_src;

  // Next build our program
  DO(test_compile(prog_src.c_str(), "expr"));

  // Finally, extract the resulting type ASTs
  DO(lookup_member("GenTypes", "f", TK_FUN));
  DO(child(3); check(TK_PARAMS));
  ast_t* params = walk_ast;
  replace_dontcares(params);

  DO(child(0); child(1));
  prog_type_1 = walk_ast;

  DO(walk(params); child(1); child(1));
  prog_type_2 = walk_ast;

  DO(walk(params); child(2); child(1));
  prog_type_3 = walk_ast;
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
