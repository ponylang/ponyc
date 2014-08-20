#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/pass/traits.h"
#include "../../src/libponyc/ds/stringtab.h"
#include "../../src/libponyc/pass/pass.h"
#include "../../src/libponyc/pkg/package.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


class TraitsTest: public testing::Test
{};


TEST(TraitsTest, ClassGetsTraitBody)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32 => 1"
    " class Foo is T";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x x"
    "  (nominal (id hygid) (id U32) x val x)"
    "  x (seq:scope 1))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ASSERT_NE((void*)NULL, program);

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));

  ast_free(program);
}


TEST(TraitsTest, ClassBodyNotOverriddenByTrait)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32 => 1"
    " class Foo is T"
    "   fun ref bar(): U32 => 2";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x x"
    "  (nominal (id hygid) (id U32) x val x)"
    "  x (seq:scope 2))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));

  ast_free(program);
}


TEST(TraitsTest, ClassBodyNotOverriddenBy2Traits)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32 => 1"
    " trait U"
    "   fun ref bar(): U32 => 2"
    " class Foo is T, U"
    "   fun ref bar(): U32 => 3";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x x"
    "  (nominal (id hygid) (id U32) x val x)"
    "  x (seq:scope 3))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));

  ast_free(program);
}


TEST(TraitsTest, NoClassBodyAnd2TraitBodies)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32 => 1"
    " trait U"
    "   fun ref bar(): U32 => 2"
    " class Foo is T, U";

  const char* builtin =
    "data U32";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_EQ(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, TransitiveTraits)
{
  const char* prog =
    " trait T is U"
    " trait U"
    "   fun ref bar(): U32 => 1"
    " class Foo is T";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x x"
    "  (nominal (id hygid) (id U32) x val x)"
    "  x (seq:scope 1))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));

  ast_free(program);
}


TEST(TraitsTest, NoBody)
{
  const char* prog =
    " trait T"
    "   fun ref bar(): U32"
    " class Foo is T";

  const char* builtin =
    "data U32";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_EQ(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, TraitLoop)
{
  const char* prog =
    " trait T is U"
    " trait U is V"
    " trait V is T";

  const char* builtin =
    "data U32";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_LE(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, TraitAndClassNamesDontClash)
{
  const char* prog =
    " trait T"
    "   fun ref bar(x: U32): U32 => x"
    " class Foo is T"
    "   var x: U32";

  const char* builtin =
    "data U32";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_free(program);
}


TEST(TraitsTest, MethodContravarianceClassToTrait)
{
  const char* prog =
    " trait A"
    " trait B is A"
    " trait C is A"
    " trait T"
    "   fun ref bar(x: B): A => x"
    " class Foo is T"
    "   fun ref bar(x: A): B => x";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x"
    "  (params"
    "    (param (id x) (nominal (id hygid) (id A) x ref x) x))"
    "  (nominal (id hygid) (id B) x ref x) x"
    "  (seq:scope (reference (id x))))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));
  ast_free(program);
}


TEST(TraitsTest, MethodContravarianceTraitToTrait)
{
  const char* prog =
    " trait A"
    " trait B is A"
    " trait C is A"
    " trait T"
    "   fun ref bar(x: A): B => x"
    " trait U"
    "   fun ref bar(x: B): A"
    " class Foo is T, U";

  const char* builtin =
    "data U32";

  const char* expected_bar =
    "(fun:scope ref (id bar) x"
    "  (params"
    "    (param (id x) (nominal (id hygid) (id A) x ref x) x))"
    "  (nominal (id hygid) (id B) x ref x) x"
    "  (seq:scope (reference (id x))))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));
  ast_free(program);
}


TEST(TraitsTest, MethodReverseContravariance)
{
  const char* prog =
    " trait A"
    " trait B is A"
    " trait C is A"
    " trait T"
    "   fun ref bar(x: A) => None"
    " class Foo is T"
    "   fun ref bar(x: B) => None";

  const char* builtin =
    "data None";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_LE(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, MethodReverseCovariance)
{
  const char* prog =
    " trait A"
    " trait B is A"
    " trait C is A"
    " trait T"
    "   fun ref bar(): B => None"
    " class Foo is T"
    "   fun ref bar(): A => None";

  const char* builtin =
    "data None";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("flatten");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  free_errors();

  ASSERT_EQ(AST_ERROR, ast_visit(&program, pass_traits, NULL));
  ASSERT_LE(1, get_error_count());

  ast_free(program);
}


TEST(TraitsTest, StructuralVsNominalContravariance)
{
  const char* prog =
    " trait T"
    "   fun ref bar(x: U)"
    " trait U"
    "   fun ref wombat() => None"
    " class Foo is T"
    "   fun ref bar(x: {fun ref wombat()})";

  const char* builtin =
    "data None";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_free(program);
}


TEST(TraitsTest, SelfStructuralContravariance)
{
  const char* prog =
    " trait T"
    "   fun ref bar(x: Foo)"
    "   fun ref wombat() => None"
    " class Foo is T"
    "   fun ref bar(x: {fun ref wombat()}) => None";

  const char* builtin =
    "data None";

  const char* expected_bar =
    "(fun:scope ref (id bar) x"
    "  (params"
    "    (param (id x)"
    "      (structural:scope"
    "        (members"
    "          (fun:scope ref (id wombat) x x"
    "            (nominal (id hygid) (id None) x val x)"
    "            x x))"
    "        ref x) x))"
    "  (nominal (id hygid) (id None) x val x) x"
    "  (seq:scope (reference (id None)) (reference (id None))))";

  package_add_magic("prog", prog);
  package_add_magic("builtin", builtin);
  limit_passes("traits");

  ast_t* program;
  ASSERT_NO_FATAL_FAILURE(load_test_program("prog", &program));

  ast_t* class_ast = find_sub_tree(program, TK_CLASS);
  symtab_t* class_symtab = ast_get_symtab(class_ast);
  ast_t* class_bar = (ast_t*)symtab_get(class_symtab, stringtab("bar"));
  ASSERT_NO_FATAL_FAILURE(check_tree(expected_bar, class_bar));
  ast_free(program);
}
