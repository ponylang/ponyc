extern "C" {
#include "../../src/libponyc/ast/symtab.h"
}
#include <gtest/gtest.h>


class SymtabTest: public testing::Test
{};


TEST(SymtabTest, AddGet)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, "foo", (void*)14));
  ASSERT_EQ((void*)14, symtab_get(symtab, "foo"));

  symtab_free(symtab);
}


TEST(SymtabTest, InitiallyNotPresent)
{
  symtab_t* symtab = symtab_new();

  ASSERT_EQ((void*)NULL, symtab_get(symtab, "foo"));

  symtab_free(symtab);
}


TEST(SymtabTest, RepeatAddBlocked)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, "foo", (void*)14));
  ASSERT_FALSE(symtab_add(symtab, "foo", (void*)15));
  ASSERT_EQ((void*)14, symtab_get(symtab, "foo"));

  symtab_free(symtab);
}


TEST(SymtabTest, AddAfterGetFail)
{
  symtab_t* symtab = symtab_new();

  ASSERT_EQ((void*)NULL, symtab_get(symtab, "foo"));
  ASSERT_TRUE(symtab_add(symtab, "foo", (void*)14));
  ASSERT_EQ((void*)14, symtab_get(symtab, "foo"));

  symtab_free(symtab);
}


TEST(SymtabTest, Multiple)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, "foo", (void*)14));
  ASSERT_TRUE(symtab_add(symtab, "bar", (void*)15));
  ASSERT_TRUE(symtab_add(symtab, "wombat", (void*)16));
  ASSERT_FALSE(symtab_add(symtab, "foo", (void*)17));
  ASSERT_EQ((void*)14, symtab_get(symtab, "foo"));
  ASSERT_EQ((void*)15, symtab_get(symtab, "bar"));
  ASSERT_EQ((void*)16, symtab_get(symtab, "wombat"));
  ASSERT_EQ((void*)14, symtab_get(symtab, "foo"));

  symtab_free(symtab);
}


TEST(SymtabTest, TwoTables)
{
  symtab_t* symtab1 = symtab_new();
  symtab_t* symtab2 = symtab_new();

  ASSERT_TRUE(symtab_add(symtab1, "foo", (void*)14));
  ASSERT_TRUE(symtab_add(symtab2, "bar", (void*)15));

  ASSERT_EQ((void*)14, symtab_get(symtab1, "foo"));
  ASSERT_EQ((void*)NULL, symtab_get(symtab1, "bar"));

  ASSERT_EQ((void*)NULL, symtab_get(symtab2, "foo"));
  ASSERT_EQ((void*)15, symtab_get(symtab2, "bar"));

  symtab_free(symtab1);
  symtab_free(symtab2);
}


TEST(SymtabTest, Merge)
{
  symtab_t* symtab1 = symtab_new();
  symtab_t* symtab2 = symtab_new();

  ASSERT_TRUE(symtab_add(symtab1, "foo", (void*)14));
  ASSERT_TRUE(symtab_add(symtab2, "bar", (void*)15));

  ASSERT_EQ((void*)14, symtab_get(symtab1, "foo"));
  ASSERT_EQ((void*)NULL, symtab_get(symtab1, "bar"));

  ASSERT_TRUE(symtab_merge(symtab1, symtab2));

  ASSERT_EQ((void*)14, symtab_get(symtab1, "foo"));
  ASSERT_EQ((void*)15, symtab_get(symtab2, "bar"));
  
  ASSERT_FALSE(symtab_merge(symtab1, symtab2));

  symtab_free(symtab1);
  symtab_free(symtab2);
}
