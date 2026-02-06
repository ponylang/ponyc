#include <gtest/gtest.h>
#include <platform.h>

#include <pkg/buildflagset.h>

class UserFlagsTest: public testing::Test
{};

TEST(UserFlagsTest, UserFlagNoneDefined)
{
  userflags_t* flags = userflags_create();

  define_userflag(flags, "foo");
  clear_userflags(flags);
  ASSERT_FALSE(is_userflag_defined(flags, "foo"));
  userflags_free(flags);
}


TEST(UserFlagsTest, UserFlagDefined)
{
  userflags_t* flags = userflags_create();
  define_userflag(flags, "foo");

  ASSERT_TRUE(is_userflag_defined(flags, "foo"));
  userflags_free(flags);
}


TEST(UserFlagsTest, UserFlagNotDefined)
{
  userflags_t* flags = userflags_create();
  define_userflag(flags, "foo");

  ASSERT_TRUE(is_userflag_defined(flags, "foo"));
  ASSERT_FALSE(is_userflag_defined(flags, "bar"));
  userflags_free(flags);
}


TEST(UserFlagsTest, UserFlagCaseSensitive)
{
  userflags_t* flags = userflags_create();
  define_userflag(flags, "foo");

  ASSERT_TRUE(is_userflag_defined(flags, "foo"));
  ASSERT_FALSE(is_userflag_defined(flags, "FOO"));
  userflags_free(flags);
}


TEST(UserFlagsTest, UserFlagMultiple)
{
  userflags_t* flags = userflags_create();
  define_userflag(flags, "foo");
  define_userflag(flags, "BAR");
  define_userflag(flags, "Wombat");

  ASSERT_TRUE(is_userflag_defined(flags, "foo"));
  ASSERT_TRUE(is_userflag_defined(flags, "BAR"));
  ASSERT_FALSE(is_userflag_defined(flags, "aardvark"));
  ASSERT_TRUE(is_userflag_defined(flags, "Wombat"));
  userflags_free(flags);
}
