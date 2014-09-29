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

  ASSERT_EQ(0, program_assign_pkg_id(prog));
  ASSERT_EQ(1, program_assign_pkg_id(prog));
  ASSERT_EQ(2, program_assign_pkg_id(prog));
  ASSERT_EQ(3, program_assign_pkg_id(prog));
  ASSERT_EQ(4, program_assign_pkg_id(prog));
  ASSERT_EQ(5, program_assign_pkg_id(prog));

  ast_free(prog);
}


TEST(ProgramTest, PackageIdFromChild)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ast_t* child = ast_blank(TK_WHILE);
  ast_add(prog, child);

  ASSERT_EQ(0, program_assign_pkg_id(child));
  ASSERT_EQ(1, program_assign_pkg_id(prog));
  ASSERT_EQ(2, program_assign_pkg_id(child));
  ASSERT_EQ(3, program_assign_pkg_id(prog));
  ASSERT_EQ(4, program_assign_pkg_id(prog));
  ASSERT_EQ(5, program_assign_pkg_id(child));

  ast_free(prog);
}


TEST(ProgramTest, NoLibs)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  program_lib_build_args(prog, "pre", "post", "", "");

  ASSERT_EQ(0, program_lib_arg_length(prog));
  ASSERT_STREQ("", program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, OneLib)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_TRUE(use_library(prog, "foo", NULL, NULL));

  program_lib_build_args(prog, "", "", "", "");

  const char* expect = " foo ";

  ASSERT_EQ(strlen(expect), program_lib_arg_length(prog));
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, OneLibWithAmbles)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_TRUE(use_library(prog, "foo", NULL, NULL));

  program_lib_build_args(prog, "pre", "post", "lpre", "lpost");

  const char* expect = " prelprefoolpost post";

  ASSERT_EQ(strlen(expect), program_lib_arg_length(prog));
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

  program_lib_build_args(prog, "", "", "", "");

  const char* expect = " foo bar wombat ";

  ASSERT_EQ(strlen(expect), program_lib_arg_length(prog));
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

  program_lib_build_args(prog, "pre", "post", "lpre", "lpost");

  const char* expect = " prelprefoolpost lprebarlpost lprewombatlpost post";

  ASSERT_EQ(strlen(expect), program_lib_arg_length(prog));
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

  program_lib_build_args(prog, "", "", "", "");

  const char* expect = " foo bar wombat ";

  ASSERT_EQ(strlen(expect), program_lib_arg_length(prog));
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, BadLibName)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  ASSERT_FALSE(use_library(prog, "foo bar", NULL, NULL));
  ASSERT_FALSE(use_library(prog, "foo\tbar", NULL, NULL));

  ast_free(prog);
}
