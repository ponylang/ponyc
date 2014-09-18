#include <platform/platform.h>

PONY_EXTERN_C_BEGIN
#include <ast/symtab.h>
#include <ds/stringtab.h>
PONY_EXTERN_C_END

#include <gtest/gtest.h>


class SymtabTest: public testing::Test
{};


TEST(SymtabTest, AddGet)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab("foo"), (void*)14, SYM_NONE));
  ASSERT_EQ((void*)14, symtab_get(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, InitiallyNotPresent)
{
  symtab_t* symtab = symtab_new();

  ASSERT_EQ((void*)NULL, symtab_get(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, RepeatAddBlocked)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab("foo"), (void*)14, SYM_NONE));
  ASSERT_FALSE(symtab_add(symtab, stringtab("foo"), (void*)15, SYM_NONE));
  ASSERT_EQ((void*)14, symtab_get(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, AddAfterGetFail)
{
  symtab_t* symtab = symtab_new();

  ASSERT_EQ((void*)NULL, symtab_get(symtab, stringtab("foo"), NULL));
  ASSERT_TRUE(symtab_add(symtab, stringtab("foo"), (void*)14, SYM_NONE));
  ASSERT_EQ((void*)14, symtab_get(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, Multiple)
{
  symtab_t* symtab = symtab_new();

  ASSERT_TRUE(symtab_add(symtab, stringtab("foo"), (void*)14, SYM_NONE));
  ASSERT_TRUE(symtab_add(symtab, stringtab("bar"), (void*)15, SYM_NONE));
  ASSERT_TRUE(symtab_add(symtab, stringtab("wombat"), (void*)16, SYM_NONE));
  ASSERT_FALSE(symtab_add(symtab, stringtab("foo"), (void*)17, SYM_NONE));
  ASSERT_EQ((void*)14, symtab_get(symtab, stringtab("foo"), NULL));
  ASSERT_EQ((void*)15, symtab_get(symtab, stringtab("bar"), NULL));
  ASSERT_EQ((void*)16, symtab_get(symtab, stringtab("wombat"), NULL));
  ASSERT_EQ((void*)14, symtab_get(symtab, stringtab("foo"), NULL));

  symtab_free(symtab);
}


TEST(SymtabTest, TwoTables)
{
  symtab_t* symtab1 = symtab_new();
  symtab_t* symtab2 = symtab_new();

  ASSERT_TRUE(symtab_add(symtab1, stringtab("foo"), (void*)14, SYM_NONE));
  ASSERT_TRUE(symtab_add(symtab2, stringtab("bar"), (void*)15, SYM_NONE));

  ASSERT_EQ((void*)14, symtab_get(symtab1, stringtab("foo"), NULL));
  ASSERT_EQ((void*)NULL, symtab_get(symtab1, stringtab("bar"), NULL));

  ASSERT_EQ((void*)NULL, symtab_get(symtab2, stringtab("foo"), NULL));
  ASSERT_EQ((void*)15, symtab_get(symtab2, stringtab("bar"), NULL));

  symtab_free(symtab1);
  symtab_free(symtab2);
}


TEST(SymtabTest, Merge)
{
  symtab_t* symtab1 = symtab_new();
  symtab_t* symtab2 = symtab_new();

  ASSERT_TRUE(symtab_add(symtab1, stringtab("foo"), (void*)14, SYM_NONE));
  ASSERT_TRUE(symtab_add(symtab2, stringtab("bar"), (void*)15, SYM_NONE));

  ASSERT_EQ((void*)14, symtab_get(symtab1, stringtab("foo"), NULL));
  ASSERT_EQ((void*)NULL, symtab_get(symtab1, stringtab("bar"), NULL));

  ASSERT_TRUE(symtab_merge(symtab1, symtab2, NULL, NULL, NULL, NULL));

  ASSERT_EQ((void*)14, symtab_get(symtab1, stringtab("foo"), NULL));
  ASSERT_EQ((void*)15, symtab_get(symtab2, stringtab("bar"), NULL));

  ASSERT_FALSE(symtab_merge(symtab1, symtab2, NULL, NULL, NULL, NULL));

  symtab_free(symtab1);
  symtab_free(symtab2);
}
