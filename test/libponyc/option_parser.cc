#include <gtest/gtest.h>
#include <platform.h>

#ifndef PLATFORM_IS_VISUAL_STUDIO
extern "C" {
#endif
#include "../../src/libponyrt/options/options.h"
#ifndef PLATFORM_IS_VISUAL_STUDIO
}
#endif

class OptionParserTest : public testing::Test
{};

TEST_F(OptionParserTest, LongSingleDashArgument)
{
  for(uint32_t mode : {OPT_ARG_REQUIRED, OPT_ARG_OPTIONAL})
  {
    SCOPED_TRACE(mode);
    const opt_arg_t options[] = {
      {"output", 0, mode, 1},
      {"flag", 'x', OPT_ARG_NONE, 2},
      OPT_ARGS_FINISH
    };
    char program[] = "program";
    char output[] = "--output";
    char dash[] = "-";
    char positional[] = "positional";
    char flag[] = "--flag";
    char* argv[] = {program, output, dash, positional, flag, nullptr};
    int argc = 5;
    opt_state_t s;
    ponyint_opt_init(options, &s, &argc, argv);

    ASSERT_EQ(1, ponyint_opt_next(&s));
    ASSERT_STREQ("-", s.arg_val);
    ASSERT_EQ(3, argc);
    ASSERT_STREQ("program", argv[0]);
    ASSERT_STREQ("positional", argv[1]);
    ASSERT_STREQ("--flag", argv[2]);
    ASSERT_EQ(2, ponyint_opt_next(&s));
    ASSERT_EQ(nullptr, s.arg_val);
    ASSERT_EQ(2, argc);
    ASSERT_STREQ("program", argv[0]);
    ASSERT_STREQ("positional", argv[1]);
    ASSERT_EQ(-1, ponyint_opt_next(&s));
    ASSERT_STREQ("positional", argv[1]);
  }
}

TEST_F(OptionParserTest, LongSingleDashArgumentAtEnd)
{
  for(uint32_t mode : {OPT_ARG_REQUIRED, OPT_ARG_OPTIONAL})
  {
    SCOPED_TRACE(mode);
    const opt_arg_t options[] = {
      {"output", 0, mode, 1},
      OPT_ARGS_FINISH
    };
    char program[] = "program";
    char output[] = "--output";
    char dash[] = "-";
    char* argv[] = {program, output, dash, nullptr};
    int argc = 3;
    opt_state_t s;
    ponyint_opt_init(options, &s, &argc, argv);

    ASSERT_EQ(1, ponyint_opt_next(&s));
    ASSERT_STREQ("-", s.arg_val);
    ASSERT_EQ(1, argc);
    ASSERT_STREQ("program", argv[0]);
    ASSERT_EQ(-1, ponyint_opt_next(&s));
  }
}

TEST_F(OptionParserTest, RequiredLongRejectsOptionLikeArgument)
{
  char candidates[][7] = {"-x", "--flag", "--"};
  for(auto& candidate : candidates)
  {
    SCOPED_TRACE(candidate);
    const opt_arg_t options[] = {
      {"output", 0, OPT_ARG_REQUIRED, 1},
      {"flag", 'x', OPT_ARG_NONE, 2},
      OPT_ARGS_FINISH
    };
    char program[] = "program";
    char output[] = "--output";
    char* argv[] = {program, output, candidate, nullptr};
    int argc = 3;
    opt_state_t s;
    ponyint_opt_init(options, &s, &argc, argv);

    ASSERT_EQ(-2, ponyint_opt_next(&s));
    ASSERT_EQ(nullptr, s.arg_val);
    ASSERT_EQ(3, argc);
    ASSERT_STREQ("program", argv[0]);
    ASSERT_STREQ("--output", argv[1]);
    ASSERT_EQ(candidate, argv[2]);
  }
}

TEST_F(OptionParserTest, OptionalLongPreservesFollowingOption)
{
  char candidates[][7] = {"-x", "--flag"};
  for(auto& candidate : candidates)
  {
    SCOPED_TRACE(candidate);
    const opt_arg_t options[] = {
      {"output", 0, OPT_ARG_OPTIONAL, 1},
      {"flag", 'x', OPT_ARG_NONE, 2},
      OPT_ARGS_FINISH
    };
    char program[] = "program";
    char output[] = "--output";
    char* argv[] = {program, output, candidate, nullptr};
    int argc = 3;
    opt_state_t s;
    ponyint_opt_init(options, &s, &argc, argv);

    ASSERT_EQ(1, ponyint_opt_next(&s));
    ASSERT_EQ(nullptr, s.arg_val);
    ASSERT_EQ(2, argc);
    ASSERT_STREQ("program", argv[0]);
    ASSERT_EQ(candidate, argv[1]);
    ASSERT_EQ(2, ponyint_opt_next(&s));
    ASSERT_EQ(nullptr, s.arg_val);
    ASSERT_EQ(1, argc);
    ASSERT_STREQ("program", argv[0]);
    ASSERT_EQ(-1, ponyint_opt_next(&s));
  }
}

TEST_F(OptionParserTest, LongAttachedSingleDashArgument)
{
  for(uint32_t mode : {OPT_ARG_REQUIRED, OPT_ARG_OPTIONAL})
  {
    SCOPED_TRACE(mode);
    const opt_arg_t options[] = {
      {"output", 0, mode, 1},
      OPT_ARGS_FINISH
    };
    char program[] = "program";
    char output[] = "--output=-";
    char positional[] = "positional";
    char* argv[] = {program, output, positional, nullptr};
    int argc = 3;
    opt_state_t s;
    ponyint_opt_init(options, &s, &argc, argv);

    ASSERT_EQ(1, ponyint_opt_next(&s));
    ASSERT_STREQ("-", s.arg_val);
    ASSERT_EQ(2, argc);
    ASSERT_STREQ("program", argv[0]);
    ASSERT_STREQ("positional", argv[1]);
    ASSERT_EQ(-1, ponyint_opt_next(&s));
    ASSERT_STREQ("positional", argv[1]);
  }
}

TEST_F(OptionParserTest, LongOrdinaryArgument)
{
  for(uint32_t mode : {OPT_ARG_REQUIRED, OPT_ARG_OPTIONAL})
  {
    SCOPED_TRACE(mode);
    const opt_arg_t options[] = {
      {"output", 0, mode, 1},
      OPT_ARGS_FINISH
    };
    char program[] = "program";
    char output[] = "--output";
    char value[] = "trace.json";
    char positional[] = "positional";
    char* argv[] = {program, output, value, positional, nullptr};
    int argc = 4;
    opt_state_t s;
    ponyint_opt_init(options, &s, &argc, argv);

    ASSERT_EQ(1, ponyint_opt_next(&s));
    ASSERT_STREQ("trace.json", s.arg_val);
    ASSERT_EQ(2, argc);
    ASSERT_STREQ("program", argv[0]);
    ASSERT_STREQ("positional", argv[1]);
    ASSERT_EQ(-1, ponyint_opt_next(&s));
  }
}

TEST_F(OptionParserTest, LongMissingArgument)
{
  for(uint32_t mode : {OPT_ARG_REQUIRED, OPT_ARG_OPTIONAL})
  {
    SCOPED_TRACE(mode);
    const opt_arg_t options[] = {
      {"output", 0, mode, 1},
      OPT_ARGS_FINISH
    };
    char program[] = "program";
    char output[] = "--output";
    char* argv[] = {program, output, nullptr};
    int argc = 2;
    opt_state_t s;
    ponyint_opt_init(options, &s, &argc, argv);

    if(mode == OPT_ARG_REQUIRED)
    {
      ASSERT_EQ(-2, ponyint_opt_next(&s));
      ASSERT_EQ(nullptr, s.arg_val);
      ASSERT_EQ(2, argc);
      ASSERT_STREQ("program", argv[0]);
      ASSERT_STREQ("--output", argv[1]);
    }
    else
    {
      ASSERT_EQ(1, ponyint_opt_next(&s));
      ASSERT_EQ(nullptr, s.arg_val);
      ASSERT_EQ(1, argc);
      ASSERT_STREQ("program", argv[0]);
      ASSERT_EQ(-1, ponyint_opt_next(&s));
    }
  }
}

TEST_F(OptionParserTest, LongNoArgumentPreservesSingleDash)
{
  const opt_arg_t options[] = {
    {"flag", 0, OPT_ARG_NONE, 1},
    OPT_ARGS_FINISH
  };
  char program[] = "program";
  char flag[] = "--flag";
  char dash[] = "-";
  char positional[] = "positional";
  char* argv[] = {program, flag, dash, positional, nullptr};
  int argc = 4;
  opt_state_t s;
  ponyint_opt_init(options, &s, &argc, argv);

  ASSERT_EQ(1, ponyint_opt_next(&s));
  ASSERT_EQ(nullptr, s.arg_val);
  ASSERT_EQ(3, argc);
  ASSERT_STREQ("program", argv[0]);
  ASSERT_STREQ("-", argv[1]);
  ASSERT_STREQ("positional", argv[2]);
  ASSERT_EQ(-1, ponyint_opt_next(&s));
  ASSERT_EQ(3, argc);
  ASSERT_STREQ("-", argv[1]);
  ASSERT_STREQ("positional", argv[2]);
}
