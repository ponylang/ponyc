#include <gtest/gtest.h>
#include <platform.h>

#include <pkg/buildflagset.h>
#include <ast/stringtab.h>

class UserFlagsTest: public testing::Test
{
protected:
  strtable_t* tab;
  void SetUp() override { tab = stringtab_new(); }
  void TearDown() override { stringtab_free(tab); }
};

TEST_F(UserFlagsTest, UserFlagNoneDefined)
{
  userflags_t* flags = userflags_create(tab);

  define_userflag(flags, "foo");
  clear_userflags(flags);
  ASSERT_FALSE(is_userflag_defined(flags, "foo"));
  userflags_free(flags);
}


TEST_F(UserFlagsTest, UserFlagDefined)
{
  userflags_t* flags = userflags_create(tab);
  define_userflag(flags, "foo");

  ASSERT_TRUE(is_userflag_defined(flags, "foo"));
  userflags_free(flags);
}


TEST_F(UserFlagsTest, UserFlagNotDefined)
{
  userflags_t* flags = userflags_create(tab);
  define_userflag(flags, "foo");

  ASSERT_TRUE(is_userflag_defined(flags, "foo"));
  ASSERT_FALSE(is_userflag_defined(flags, "bar"));
  userflags_free(flags);
}


TEST_F(UserFlagsTest, UserFlagCaseSensitive)
{
  userflags_t* flags = userflags_create(tab);
  define_userflag(flags, "foo");

  ASSERT_TRUE(is_userflag_defined(flags, "foo"));
  ASSERT_FALSE(is_userflag_defined(flags, "FOO"));
  userflags_free(flags);
}


TEST_F(UserFlagsTest, UserFlagMultiple)
{
  userflags_t* flags = userflags_create(tab);
  define_userflag(flags, "foo");
  define_userflag(flags, "BAR");
  define_userflag(flags, "Wombat");

  ASSERT_TRUE(is_userflag_defined(flags, "foo"));
  ASSERT_TRUE(is_userflag_defined(flags, "BAR"));
  ASSERT_FALSE(is_userflag_defined(flags, "aardvark"));
  ASSERT_TRUE(is_userflag_defined(flags, "Wombat"));
  userflags_free(flags);
}
