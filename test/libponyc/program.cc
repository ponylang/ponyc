#include <gtest/gtest.h>
#include <platform.h>
#include <ast/ast.h>
#include <pkg/program.h>


class ProgramTest : public testing::Test
{};


TEST(ProgramTest, PackageID)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_EQ((unsigned int)0, program_assign_pkg_id(prog));
  ASSERT_EQ((unsigned int)1, program_assign_pkg_id(prog));
  ASSERT_EQ((unsigned int)2, program_assign_pkg_id(prog));
  ASSERT_EQ((unsigned int)3, program_assign_pkg_id(prog));
  ASSERT_EQ((unsigned int)4, program_assign_pkg_id(prog));
  ASSERT_EQ((unsigned int)5, program_assign_pkg_id(prog));

  ast_free(prog);
}


TEST(ProgramTest, NoLibs)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  program_lib_build_args(prog, "", "", "pre", "post", "", "");
  ASSERT_STREQ("prepost", program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, OneLib)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_TRUE(use_library(prog, "foo", NULL, NULL));

  program_lib_build_args(prog, "", "", "", "", "", "");

  const char* expect = "\"foo\" ";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, OneLibWithAmbles)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_TRUE(use_library(prog, "foo", NULL, NULL));

  program_lib_build_args(prog, "", "", "pre", "post", "lpre", "lpost");

  const char* expect = "prelpre\"foo\"lpost post";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, MultipleLibs)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_TRUE(use_library(prog, "foo", NULL, NULL));
  ASSERT_TRUE(use_library(prog, "bar", NULL, NULL));
  ASSERT_TRUE(use_library(prog, "wombat", NULL, NULL));

  program_lib_build_args(prog, "", "", "", "", "", "");

  const char* expect = "\"foo\" \"bar\" \"wombat\" ";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, MultipleLibsWithAmbles)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_TRUE(use_library(prog, "foo", NULL, NULL));
  ASSERT_TRUE(use_library(prog, "bar", NULL, NULL));
  ASSERT_TRUE(use_library(prog, "wombat", NULL, NULL));

  program_lib_build_args(prog, "", "", "pre", "post", "lpre", "lpost");

  const char* expect =
    "prelpre\"foo\"lpost lpre\"bar\"lpost lpre\"wombat\"lpost post";

  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, RepeatedLibs)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_TRUE(use_library(prog, "foo", NULL, NULL));
  ASSERT_TRUE(use_library(prog, "bar", NULL, NULL));
  ASSERT_TRUE(use_library(prog, "bar", NULL, NULL));
  ASSERT_TRUE(use_library(prog, "foo", NULL, NULL));
  ASSERT_TRUE(use_library(prog, "wombat", NULL, NULL));

  program_lib_build_args(prog, "", "", "" "", "", "", "");

  const char* expect = "\"foo\" \"bar\" \"wombat\" ";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, BadLibName)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_FALSE(use_library(prog, "foo\nbar", NULL, NULL));
  ASSERT_FALSE(use_library(prog, "foo\tbar", NULL, NULL));
  ASSERT_FALSE(use_library(prog, "foo;bar", NULL, NULL));
  ASSERT_FALSE(use_library(prog, "foo$bar", NULL, NULL));
  //ASSERT_FALSE(use_library(prog, "foo\\bar", NULL, NULL));

  ast_free(prog);
}


TEST(ProgramTest, LibPaths)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_TRUE(use_path(prog, "foo", NULL, NULL));

  program_lib_build_args(prog, "static", "dynamic", "", "", "", "");

  const char* expect = "static\"foo\" dynamic\"foo\" ";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}
