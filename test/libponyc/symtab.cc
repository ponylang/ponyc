#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/symtab.h>
#include <ast/stringtab.h>

class SymtabTest: public testing::Test
{};


TEST(SymtabTest, AddGet)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab("foo"), (ast_t*)14, SYM_NONE));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, InitiallyNotPresent)
{
  symtab_t* symtab = symtab_new();

  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, RepeatAddBlocked)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab("foo"), (ast_t*)14, SYM_NONE));
  ASSERT_FALSE(symtab_add(symtab, stringtab("foo"), (ast_t*)15, SYM_NONE));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, AddAfterGetFail)
{
  symtab_t* symtab = symtab_new();

  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab, stringtab("foo"), NULL));
  ASSERT_TRUE(symtab_add(symtab, stringtab("foo"), (ast_t*)14, SYM_NONE));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, Multiple)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab("foo"), (ast_t*)14, SYM_NONE));
  ASSERT_TRUE(symtab_add(symtab, stringtab("bar"), (ast_t*)15, SYM_NONE));
  ASSERT_TRUE(symtab_add(symtab, stringtab("wombat"), (ast_t*)16, SYM_NONE));
  ASSERT_FALSE(symtab_add(symtab, stringtab("foo"), (ast_t*)17, SYM_NONE));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab("foo"), NULL));
  ASSERT_EQ((ast_t*)15, symtab_find(symtab, stringtab("bar"), NULL));
  ASSERT_EQ((ast_t*)16, symtab_find(symtab, stringtab("wombat"), NULL));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, TwoTables)
{
  symtab_t* symtab1 = symtab_new();
  symtab_t* symtab2 = symtab_new();

  ASSERT_TRUE(symtab_add(symtab1, stringtab("foo"), (ast_t*)14, SYM_NONE));
  ASSERT_TRUE(symtab_add(symtab2, stringtab("bar"), (ast_t*)15, SYM_NONE));

  ASSERT_EQ((ast_t*)14, symtab_find(symtab1, stringtab("foo"), NULL));
  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab1, stringtab("bar"), NULL));

  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab2, stringtab("foo"), NULL));
  ASSERT_EQ((ast_t*)15, symtab_find(symtab2, stringtab("bar"), NULL));

  symtab_free(symtab1);
  symtab_free(symtab2);
}


TEST(SymtabTest, Merge)
{
  symtab_t* symtab1 = symtab_new();
  symtab_t* symtab2 = symtab_new();

  ASSERT_TRUE(symtab_add(symtab1, stringtab("foo"), (ast_t*)14, SYM_NONE));
  ASSERT_TRUE(symtab_add(symtab2, stringtab("bar"), (ast_t*)15, SYM_NONE));

  ASSERT_EQ((ast_t*)14, symtab_find(symtab1, stringtab("foo"), NULL));
  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab1, stringtab("bar"), NULL));

  ASSERT_TRUE(symtab_merge_public(symtab1, symtab2));

  ASSERT_EQ((ast_t*)14, symtab_find(symtab1, stringtab("foo"), NULL));
  ASSERT_EQ((ast_t*)15, symtab_find(symtab2, stringtab("bar"), NULL));

  ASSERT_FALSE(symtab_merge_public(symtab1, symtab2));

  symtab_free(symtab1);
  symtab_free(symtab2);
}
