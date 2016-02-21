#include <gtest/gtest.h>
#include <platform.h>

#include <pkg/buildflagset.h>
#include <ast/stringtab.h>


class BuildFlagSetTest: public testing::Test
{};


TEST(BuildFlagSetTest, PrintEmpty)
{
  buildflagset_t* set = buildflagset_create();
  ASSERT_NE((void*)NULL, set);

  // We don't care exactly what string is returned for an empty set, but it
  // must be something and not just whitespace.
  const char* text = buildflagset_print(set);

  ASSERT_NE((void*)NULL, text);
  ASSERT_NE('\0', text[0]);
  ASSERT_NE(' ', text[0]);

  buildflagset_free(set);
}


TEST(BuildFlagSetTest, EmptyHas1Config)
{
  buildflagset_t* set = buildflagset_create();
  ASSERT_NE((void*)NULL, set);

  ASSERT_EQ(1, buildflagset_configcount(set));

  buildflagset_free(set);
}


TEST(BuildFlagSetTest, Flag)
{
  buildflagset_t* set = buildflagset_create();
  ASSERT_NE((void*)NULL, set);

  buildflagset_add(set, stringtab("foo"));
  ASSERT_EQ(2, buildflagset_configcount(set));

  buildflagset_startenum(set);

  bool had_true = false;
  bool had_false = false;
  int count = 0;

  while(buildflagset_next(set))
  {
    count++;

    if(buildflagset_get(set, stringtab("foo")))
    {
      had_true = true;
      ASSERT_STREQ("foo", buildflagset_print(set));
    }
    else
    {
      had_false = true;
      ASSERT_STREQ("!foo", buildflagset_print(set));
    }
  }

  ASSERT_EQ(2, count);
  ASSERT_TRUE(had_true);
  ASSERT_TRUE(had_false);

  buildflagset_free(set);
}


TEST(BuildFlagSetTest, PlatformOS)
{
  buildflagset_t* set = buildflagset_create();
  ASSERT_NE((void*)NULL, set);

  buildflagset_add(set, stringtab("windows"));
  ASSERT_LT(2, buildflagset_configcount(set));

  // Check we get at least "windows" and "linux" configs. Don't check for all
  // OSes so we don't have to update this test when we add an OS.
  bool had_windows = false;
  bool had_linux = false;

  buildflagset_startenum(set);

  while(buildflagset_next(set))
  {
    const char* p = buildflagset_print(set);

    if(strcmp(p, "windows") == 0)
      had_windows = true;

    if(strcmp(p, "linux") == 0)
      had_linux = true;
  }

  ASSERT_TRUE(had_windows);
  ASSERT_TRUE(had_linux);

  buildflagset_free(set);
}

TEST(BuildFlagSetTest, MultipleFlags)
{
  buildflagset_t* set = buildflagset_create();
  ASSERT_NE((void*)NULL, set);

  buildflagset_add(set, stringtab("a"));
  buildflagset_add(set, stringtab("b"));
  buildflagset_add(set, stringtab("c"));
  ASSERT_EQ(8, buildflagset_configcount(set));

  buildflagset_startenum(set);

  bool had[8] = { false, false, false, false, false, false, false, false };
  int count = 0;

  while(buildflagset_next(set))
  {
    count++;
    int config = 0;

    if(buildflagset_get(set, stringtab("a"))) config += 1;
    if(buildflagset_get(set, stringtab("b"))) config += 2;
    if(buildflagset_get(set, stringtab("c"))) config += 4;

    ASSERT_FALSE(had[config]);
    had[config] = true;

    if(config == 5)
    {
      // a !b c
      // Flags could come back in all order, need to check for all 6
      const char* p = buildflagset_print(set);
      ASSERT_TRUE(strcmp(p, "a !b c") == 0 || strcmp(p, "a c !b") == 0 ||
        strcmp(p, "!b a c") == 0 || strcmp(p, "!b c a") == 0 ||
        strcmp(p, "c a !b") == 0 || strcmp(p, "c !b a") == 0);
    }
  }

  ASSERT_EQ(8, count);

  for(int i = 0; i < 8; i++)
  {
    ASSERT_TRUE(had[i]);
  }

  buildflagset_free(set);
}


TEST(BuildFlagSetTest, UserFlagNoneDefined)
{
  ASSERT_FALSE(is_build_flag_defined("foo"));
}


TEST(BuildFlagSetTest, UserFlagDefined)
{
  define_build_flag("foo");

  ASSERT_TRUE(is_build_flag_defined("foo"));
}


TEST(BuildFlagSetTest, UserFlagNotDefined)
{
  define_build_flag("foo");

  ASSERT_TRUE(is_build_flag_defined("foo"));
  ASSERT_FALSE(is_build_flag_defined("bar"));
}


TEST(BuildFlagSetTest, UserFlagCaseSensitive)
{
  define_build_flag("foo");

  ASSERT_TRUE(is_build_flag_defined("foo"));
  ASSERT_FALSE(is_build_flag_defined("FOO"));
}


TEST(BuildFlagSetTest, UserFlagMultiple)
{
  define_build_flag("foo");
  define_build_flag("BAR");
  define_build_flag("Wombat");

  ASSERT_TRUE(is_build_flag_defined("foo"));
  ASSERT_TRUE(is_build_flag_defined("BAR"));
  ASSERT_FALSE(is_build_flag_defined("aardvark"));
  ASSERT_TRUE(is_build_flag_defined("Wombat"));
}
