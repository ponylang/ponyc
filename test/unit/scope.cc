extern "C" {
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/pass/scope.h"
#include "../../src/libponyc/ds/stringtab.h"
}
#include "util.h"
#include <gtest/gtest.h>


class ScopeTest: public testing::Test
{};


TEST(ScopeTest, Var)
{
  const char* tree =
    "(module:scope (class:scope (id Bar) x x x"
    "  (members (fvar (id foo) x x))))";

  const char* foo = stringtab("foo");

  ast_t* module = parse_test_module(tree);
  ASSERT_NE((void*)NULL, module);

  ast_t* start = find_start(module, TK_FVAR);
  ASSERT_NE((void*)NULL, start);

  ASSERT_EQ(AST_OK, pass_scope(&start));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, TK_CLASS, foo, start));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, TK_MODULE, foo, NULL));

  ast_free(module);
}


TEST(ScopeTest, Let)
{
  const char* tree =
    "(module:scope (class:scope (id Bar) x x x"
    "  (members (flet (id foo) x x))))";

  const char* foo = stringtab("foo");

  ast_t* module = parse_test_module(tree);
  ASSERT_NE((void*)NULL, module);

  ast_t* start = find_start(module, TK_FLET);
  ASSERT_NE((void*)NULL, start);

  ASSERT_EQ(AST_OK, pass_scope(&start));

  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, TK_CLASS, foo, start));
  ASSERT_NO_FATAL_FAILURE(symtab_entry(module, TK_MODULE, foo, NULL));

  ast_free(module);
}


TEST(ScopeTest, FieldNamesClash)
{
  const char* tree =
    "(module:scope (class:scope (id Bar) x x x"
    "  (members"
    "    (fvar (id foo) x x)"
    "    (flet (id foo) x x))))";

  ast_t* module = parse_test_module(tree);
  ASSERT_NE((void*)NULL, module);

  ast_t* start_var = find_start(module, TK_FVAR);
  ASSERT_NE((void*)NULL, start_var);

  ASSERT_EQ(AST_OK, pass_scope(&start_var));

  ast_t* start_let = find_start(module, TK_FLET);
  ASSERT_NE((void*)NULL, start_let);

  ASSERT_EQ(AST_ERROR, pass_scope(&start_let));

  ast_free(module);
}


TEST(ScopeTest, FieldsInDifferentTypesDontClash)
{
  const char* tree =
    "(module:scope"
    "  (class:scope (id Bar) x x x"
    "    (members (fvar (id foo) x x)))"
    "  (class:scope (id Bar2) x x x"
    "    (members (flet (id foo) x x))))";

  ast_t* module = parse_test_module(tree);
  ASSERT_NE((void*)NULL, module);

  ast_t* start_var = find_start(module, TK_FVAR);
  ASSERT_NE((void*)NULL, start_var);

  ASSERT_EQ(AST_OK, pass_scope(&start_var));

  ast_t* start_let = find_start(module, TK_FLET);
  ASSERT_NE((void*)NULL, start_let);

  ASSERT_EQ(AST_OK, pass_scope(&start_let));

  ast_free(module);
}
