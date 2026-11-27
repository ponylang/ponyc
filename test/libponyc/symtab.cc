#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/symtab.h>
#include <ast/stringtab.h>

#include <stdio.h>

class SymtabTest: public testing::Test
{
protected:
  strtable_t* tab;

  void SetUp() override { tab = stringtab_new(); }
  void TearDown() override { stringtab_free(tab); }
};


TEST_F(SymtabTest, AddGet)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab(tab, "foo"), (ast_t*)14, SYM_NONE, tab));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab(tab, "foo"), NULL));

  symtab_free(symtab);
}


TEST_F(SymtabTest, InitiallyNotPresent)
{
  symtab_t* symtab = symtab_new();

  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab, stringtab(tab, "foo"), NULL));

  symtab_free(symtab);
}


TEST_F(SymtabTest, RepeatAddBlocked)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab(tab, "foo"), (ast_t*)14, SYM_NONE, tab));
  ASSERT_FALSE(symtab_add(symtab, stringtab(tab, "foo"), (ast_t*)15, SYM_NONE, tab));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab(tab, "foo"), NULL));

  symtab_free(symtab);
}


TEST_F(SymtabTest, AddAfterGetFail)
{
  symtab_t* symtab = symtab_new();

  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab, stringtab(tab, "foo"), NULL));
  ASSERT_TRUE(symtab_add(symtab, stringtab(tab, "foo"), (ast_t*)14, SYM_NONE, tab));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab(tab, "foo"), NULL));

  symtab_free(symtab);
}


TEST_F(SymtabTest, Multiple)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab(tab, "foo"), (ast_t*)14, SYM_NONE, tab));
  ASSERT_TRUE(symtab_add(symtab, stringtab(tab, "bar"), (ast_t*)15, SYM_NONE, tab));
  ASSERT_TRUE(symtab_add(symtab, stringtab(tab, "wombat"), (ast_t*)16, SYM_NONE, tab));
  ASSERT_FALSE(symtab_add(symtab, stringtab(tab, "foo"), (ast_t*)17, SYM_NONE, tab));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab(tab, "foo"), NULL));
  ASSERT_EQ((ast_t*)15, symtab_find(symtab, stringtab(tab, "bar"), NULL));
  ASSERT_EQ((ast_t*)16, symtab_find(symtab, stringtab(tab, "wombat"), NULL));
  ASSERT_EQ((ast_t*)14, symtab_find(symtab, stringtab(tab, "foo"), NULL));

  symtab_free(symtab);
}


TEST_F(SymtabTest, TwoTables)
{
  symtab_t* symtab1 = symtab_new();
  symtab_t* symtab2 = symtab_new();

  ASSERT_TRUE(symtab_add(symtab1, stringtab(tab, "foo"), (ast_t*)14, SYM_NONE, tab));
  ASSERT_TRUE(symtab_add(symtab2, stringtab(tab, "bar"), (ast_t*)15, SYM_NONE, tab));

  ASSERT_EQ((ast_t*)14, symtab_find(symtab1, stringtab(tab, "foo"), NULL));
  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab1, stringtab(tab, "bar"), NULL));

  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab2, stringtab(tab, "foo"), NULL));
  ASSERT_EQ((ast_t*)15, symtab_find(symtab2, stringtab(tab, "bar"), NULL));

  symtab_free(symtab1);
  symtab_free(symtab2);
}


TEST_F(SymtabTest, Merge)
{
  symtab_t* symtab1 = symtab_new();
  symtab_t* symtab2 = symtab_new();

  ASSERT_TRUE(symtab_add(symtab1, stringtab(tab, "foo"), (ast_t*)14, SYM_NONE, tab));
  ASSERT_TRUE(symtab_add(symtab2, stringtab(tab, "bar"), (ast_t*)15, SYM_NONE, tab));

  ASSERT_EQ((ast_t*)14, symtab_find(symtab1, stringtab(tab, "foo"), NULL));
  ASSERT_EQ((ast_t*)NULL, symtab_find(symtab1, stringtab(tab, "bar"), NULL));

  ASSERT_TRUE(symtab_merge_public(symtab1, symtab2, tab));

  ASSERT_EQ((ast_t*)14, symtab_find(symtab1, stringtab(tab, "foo"), NULL));
  ASSERT_EQ((ast_t*)15, symtab_find(symtab2, stringtab(tab, "bar"), NULL));

  ASSERT_FALSE(symtab_merge_public(symtab1, symtab2, tab));

  symtab_free(symtab1);
  symtab_free(symtab2);
}


// Interning into the same table is stable: equal strings get one canonical
// pointer. This underpins the symtab's pointer-identity comparisons.
TEST_F(SymtabTest, StringtabSameTableInternsStable)
{
  ASSERT_EQ(stringtab(tab, "foo"), stringtab(tab, "foo"));
  ASSERT_NE(stringtab(tab, "foo"), stringtab(tab, "bar"));
}


// Each table is independent: interning the same content into two different
// tables yields distinct pointers. This is the core property of the move off a
// process-global table (issue #5273) — a regression to a shared/global table
// would make these pointers equal.
TEST_F(SymtabTest, StringtabTablesAreIndependent)
{
  strtable_t* tab2 = stringtab_new();

  ASSERT_NE(stringtab(tab, "foo"), stringtab(tab2, "foo"));
  ASSERT_EQ(stringtab(tab2, "foo"), stringtab(tab2, "foo"));

  stringtab_free(tab2);
}


TEST_F(SymtabTest, RemoveDefRemovesEveryName)
{
  // Many names share one definition, so removing them shifts entries into
  // slots the removal has already passed. Every name must still go, and
  // names with another definition must stay.
  symtab_t* symtab = symtab_new();
  char name[16];

  for(int i = 0; i < 200; i++)
  {
    snprintf(name, sizeof(name), "n%d", i);
    ASSERT_TRUE(symtab_add(symtab, stringtab(tab, name), (ast_t*)14, SYM_NONE,
      tab));
  }

  for(int i = 0; i < 20; i++)
  {
    snprintf(name, sizeof(name), "k%d", i);
    ASSERT_TRUE(symtab_add(symtab, stringtab(tab, name), (ast_t*)15, SYM_NONE,
      tab));
  }

  symtab_remove_def(symtab, (ast_t*)14);

  for(int i = 0; i < 200; i++)
  {
    snprintf(name, sizeof(name), "n%d", i);
    ASSERT_EQ((ast_t*)NULL, symtab_find(symtab, stringtab(tab, name), NULL))
      << name;
    ASSERT_TRUE(symtab_add(symtab, stringtab(tab, name), (ast_t*)16, SYM_NONE,
      tab)) << name;
  }

  for(int i = 0; i < 20; i++)
  {
    snprintf(name, sizeof(name), "k%d", i);
    ASSERT_EQ((ast_t*)15, symtab_find(symtab, stringtab(tab, name), NULL))
      << name;
  }

  symtab_free(symtab);
}
