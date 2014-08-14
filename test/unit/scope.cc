#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/pass/scope.h"
#include "../../src/libponyc/ds/stringtab.h"
#include "../../src/libponyc/pkg/package.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


class ScopeTest: public testing::Test
{};


TEST(ScopeTest, Var)
{
  const char* tree =
    "(module:scope (class:scope (id Bar) x x x"
    "  (members (fvar (id foo) x x))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* var_ast = find_sub_tree(module, TK_FVAR);

  ASSERT_EQ(AST_OK, pass_scope(&var_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "foo", var_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "foo", NULL));

  ast_free(module);
}


TEST(ScopeTest, Let)
{
  const char* tree =
    "(module:scope (class:scope (id Bar) x x x"
    "  (members (flet (id foo) x x))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* var_ast = find_sub_tree(module, TK_FLET);

  ASSERT_EQ(AST_OK, pass_scope(&var_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "foo", var_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "foo", NULL));

  ast_free(module);
}


TEST(ScopeTest, Param)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members"
    "      (fun:scope ref (id wombat) x"
    "        (params"
    "          (param (id foo) nominal x))))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* fun_ast = find_sub_tree(module, TK_FUN);
  ast_t* param_ast = find_sub_tree(module, TK_PARAM);

  ASSERT_EQ(AST_OK, pass_scope(&param_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(fun_ast, "foo", param_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "foo", NULL));

  ast_free(module);
}


TEST(ScopeTest, New)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members"
    "      (new:scope ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* new_ast = find_sub_tree(module, TK_NEW);

  ASSERT_EQ(AST_OK, pass_scope(&new_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(new_ast, "foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "foo", new_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "foo", NULL));

  ast_free(module);
}


TEST(ScopeTest, Be)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members"
    "      (be:scope tag (id foo) x x x x x))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* be_ast = find_sub_tree(module, TK_BE);

  ASSERT_EQ(AST_OK, pass_scope(&be_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(be_ast, "foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "foo", be_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "foo", NULL));

  ast_free(module);
}


TEST(ScopeTest, Fun)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members"
    "      (fun:scope ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* fun_ast = find_sub_tree(module, TK_FUN);

  ASSERT_EQ(AST_OK, pass_scope(&fun_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(fun_ast, "foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "foo", fun_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "foo", NULL));

  ast_free(module);
}


TEST(ScopeTest, TypeParam)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members"
    "      (fun:scope ref (id wombat)"
    "        (typeparams (typeparam (id Foo) x x)) x x x x))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* fun_ast = find_sub_tree(module, TK_FUN);
  ast_t* param_ast = find_sub_tree(module, TK_TYPEPARAM);

  ASSERT_EQ(AST_OK, pass_scope(&param_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(fun_ast, "Foo", param_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "Foo", NULL));

  ast_free(module);
}


TEST(ScopeTest, Local)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members"
    "      (fun:scope ref (id wombat) x x x x"
    "        (seq (= (var (idseq (id foo))) 3))))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* fun_ast = find_sub_tree(module, TK_FUN);
  ast_t* local_ast = find_sub_tree(module, TK_IDSEQ);
  ast_t* id_ast = ast_child(local_ast);

  ASSERT_EQ(AST_OK, pass_scope(&local_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(fun_ast, "foo", id_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "foo", NULL));

  ast_free(module);
}


TEST(ScopeTest, MultipleLocals)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members"
    "      (fun:scope ref (id wombat) x x x x"
    "        (seq (= (var"
    "          (idseq (id foo)(id bar)(id aardvark))) 3))))))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);
  ast_t* fun_ast = find_sub_tree(module, TK_FUN);
  ast_t* local_ast = find_sub_tree(module, TK_IDSEQ);
  ast_t* foo_ast;
  ast_t* bar_ast;
  ast_t* aardvark_ast;
  AST_GET_CHILDREN(local_ast, &foo_ast, &bar_ast, &aardvark_ast);

  ASSERT_EQ(AST_OK, pass_scope(&local_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(fun_ast, "foo", foo_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(fun_ast, "bar", bar_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(fun_ast, "aardvark", aardvark_ast));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "bar", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "aardvark", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "bar", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "aardvark", NULL));

  ast_free(module);
}


TEST(ScopeTest, Actor)
{
  const char* tree =
    "(package:scope (module:scope (actor:scope (id Foo) x x x x)))";

  ast_t* package = parse_test_module(tree);
  ast_t* module = find_sub_tree(package, TK_MODULE);
  ast_t* actor_ast = find_sub_tree(package, TK_ACTOR);

  ASSERT_EQ(AST_OK, pass_scope(&actor_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(actor_ast, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(package, "Foo", actor_ast));

  ast_free(package);
}


TEST(ScopeTest, Class)
{
  const char* tree =
    "(package:scope (module:scope (class:scope (id Foo) x x x x)))";

  ast_t* package = parse_test_module(tree);
  ast_t* module = find_sub_tree(package, TK_MODULE);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);

  ASSERT_EQ(AST_OK, pass_scope(&class_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(class_ast, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(package, "Foo", class_ast));

  ast_free(package);
}


TEST(ScopeTest, Data)
{
  const char* tree =
    "(package:scope (module:scope (data:scope (id Foo) x x x x)))";

  ast_t* package = parse_test_module(tree);
  ast_t* module = find_sub_tree(package, TK_MODULE);
  ast_t* data_ast = find_sub_tree(module, TK_DATA);

  ASSERT_EQ(AST_OK, pass_scope(&data_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(data_ast, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(package, "Foo", data_ast));

  ast_free(package);
}


TEST(ScopeTest, Trait)
{
  const char* tree =
    "(package:scope (module:scope (trait:scope (id Foo) x x x x)))";

  ast_t* package = parse_test_module(tree);
  ast_t* module = find_sub_tree(package, TK_MODULE);
  ast_t* trait_ast = find_sub_tree(module, TK_TRAIT);

  ASSERT_EQ(AST_OK, pass_scope(&trait_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(trait_ast, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(package, "Foo", trait_ast));

  ast_free(package);
}


TEST(ScopeTest, Type)
{
  const char* tree =
    "(package:scope (module:scope (type (id Foo) x)))";

  ast_t* package = parse_test_module(tree);
  ast_t* module = find_sub_tree(package, TK_MODULE);
  ast_t* type_ast = find_sub_tree(module, TK_TYPE);

  ASSERT_EQ(AST_OK, pass_scope(&type_ast));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, "Foo", NULL));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(package, "Foo", type_ast));

  ast_free(package);
}


TEST(ScopeTest, NonTypeCannotHaveTypeName)
{
  const char* tree =
    "(package:scope (module:scope (class:scope (id Bar) x x x"
    "  (members (fvar (id Foo) x x)))))";

  ast_t* module = parse_test_module(tree);
  ast_t* var_ast = find_sub_tree(module, TK_FVAR);

  ASSERT_EQ(AST_ERROR, pass_scope(&var_ast));

  ast_free(module);
}


TEST(ScopeTest, TypeCannotHaveNonTypeName)
{
  const char* tree =
    "(module:scope (class:scope (id rar) x x x x))";

  ast_t* module = parse_test_module(tree);
  ast_t* class_ast = find_sub_tree(module, TK_CLASS);

  ASSERT_EQ(AST_ERROR, pass_scope(&class_ast));

  ast_free(module);
}


TEST(ScopeTest, SameScopeNameClash)
{
  const char* tree =
    "(module:scope (class:scope (id Bar) x x x"
    "  (members"
    "    (fvar (id foo) x x)"
    "    (flet (id foo) x x))))";

  ast_t* module = parse_test_module(tree);
  ast_t* var_ast = find_sub_tree(module, TK_FVAR);
  ast_t* let_ast = find_sub_tree(module, TK_FLET);

  ASSERT_EQ(AST_OK, pass_scope(&var_ast));
  ASSERT_EQ(AST_ERROR, pass_scope(&let_ast));

  ast_free(module);
}


TEST(ScopeTest, ParentScopeNameClash)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members"
    "      (fun:scope ref (id foo) x"
    "        (params"
    "          (param (id foo) nominal x)) x x x))))";

  ast_t* module = parse_test_module(tree);
  ast_t* fun_ast = find_sub_tree(module, TK_FUN);
  ast_t* param_ast = find_sub_tree(module, TK_PARAM);

  ASSERT_EQ(AST_OK, pass_scope(&fun_ast));
  ASSERT_EQ(AST_ERROR, pass_scope(&param_ast));

  ast_free(module);
}


TEST(ScopeTest, SiblingScopeNoClash)
{
  const char* tree =
    "(package:scope"
    "  (module:scope"
    "    (class:scope (id Bar1) x x x"
    "      (members"
    "        (fun:scope ref (id foo) x x x x x)))"
    "    (class:scope (id Bar2) x x x"
    "      (members"
    "        (fun:scope ref (id foo) x x x x x)))))";

  ast_t* package = parse_test_module(tree);
  ast_t* module = find_sub_tree(package, TK_MODULE);

  ASSERT_EQ(AST_OK, ast_visit(&module, pass_scope, NULL));

  ast_free(package);
}


TEST(ScopeTest, Package)
{
  const char* tree =
    "(program:scope (package:scope (module:scope)))";

  const char* builtin =
    "class U32";

  ast_t* program = parse_test_module(tree);
  ast_t* package = find_sub_tree(program, TK_PACKAGE);
  symtab_t* package_symtab = ast_get_symtab(package);

  package_add_magic("builtin", builtin);
  package_limit_passes("scope1");

  ASSERT_EQ(AST_OK, pass_scope(&package));

  // Builtin types go in the package symbol table
  ASSERT_NE((void*)NULL, symtab_get(package_symtab, stringtab("U32")));

  ast_free(program);
}


TEST(ScopeTest, Use)
{
  const char* tree =
    "(program:scope (package:scope (module:scope (use \"test\" x))))";

  const char* used_package =
    "class Foo";

  const char* builtin =
    "class U32";

  ast_t* program = parse_test_module(tree);
  ast_t* module = find_sub_tree(program, TK_MODULE);
  symtab_t* module_symtab = ast_get_symtab(module);
  ast_t* use_ast = find_sub_tree(program, TK_USE);

  package_add_magic("builtin", builtin);
  package_add_magic("test", used_package);
  package_limit_passes("scope1");

  ASSERT_EQ(AST_OK, pass_scope(&use_ast));

  // Use imported types go in the module symbol table
  ASSERT_NE((void*)NULL, symtab_get(module_symtab, stringtab("Foo")));

  ast_free(program);
}


TEST(ScopeTest, UseAs)
{
  const char* tree =
    "(program:scope (package:scope"
    "  (module:scope (use \"test\" (id bar)))))";

  const char* used_package =
    "class Foo";

  const char* builtin =
    "class U32";

  ast_t* program = parse_test_module(tree);
  ast_t* module = find_sub_tree(program, TK_MODULE);
  symtab_t* module_symtab = ast_get_symtab(module);
  ast_t* use_ast = find_sub_tree(program, TK_USE);

  package_add_magic("builtin", builtin);
  package_add_magic("test", used_package);
  package_limit_passes("scope1");

  ASSERT_EQ(AST_OK, pass_scope(&use_ast));

  // Use imported types go in the module symbol table
  ASSERT_NE((void*)NULL, symtab_get(module_symtab, stringtab("bar")));
  ASSERT_EQ((void*)NULL, symtab_get(module_symtab, stringtab("Foo")));

  ast_free(program);
}
