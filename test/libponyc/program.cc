#include <gtest/gtest.h>
#include <platform.h>
#include <ast/ast.h>
#include <ast/symtab.h>
#include <pkg/program.h>
#include <pkg/package.h>


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

  pass_opt_t opt;
  pass_opt_init(&opt);

  program_linker_build_args(prog, &opt, "", "", "pre", "post", "", "", NULL, 0);

  ASSERT_EQ(errors_get_count(opt.check.errors), 0);
  errors_print(opt.check.errors);
  pass_opt_done(&opt);
  ASSERT_STREQ("prepost", program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, OneLib)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(use_library(prog, "foo", NULL, &opt));

  program_linker_build_args(prog, &opt, "", "", "", "", "", "", NULL, 0);

  ASSERT_EQ(errors_get_count(opt.check.errors), 0);
  errors_print(opt.check.errors);
  pass_opt_done(&opt);

  const char* expect = "\"foo\" ";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, OneLibWithAmbles)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(use_library(prog, "foo", NULL, &opt));

  program_linker_build_args(prog, &opt, "", "", "pre", "post", "lpre", "lpost", NULL, 0);

  ASSERT_EQ(errors_get_count(opt.check.errors), 0);
  errors_print(opt.check.errors);
  pass_opt_done(&opt);

  const char* expect = "prelpre\"foo\"lpost post";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, MultipleLibs)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(use_library(prog, "foo", NULL, &opt));
  ASSERT_TRUE(use_library(prog, "bar", NULL, &opt));
  ASSERT_TRUE(use_library(prog, "wombat", NULL, &opt));

  program_linker_build_args(prog, &opt, "", "", "", "", "", "", NULL, 0);

  ASSERT_EQ(errors_get_count(opt.check.errors), 0);
  errors_print(opt.check.errors);
  pass_opt_done(&opt);

  const char* expect = "\"foo\" \"bar\" \"wombat\" ";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, MultipleLibsWithAmbles)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(use_library(prog, "foo", NULL, &opt));
  ASSERT_TRUE(use_library(prog, "bar", NULL, &opt));
  ASSERT_TRUE(use_library(prog, "wombat", NULL, &opt));

  program_linker_build_args(prog, &opt, "", "", "pre", "post", "lpre", "lpost", NULL, 0);

  ASSERT_EQ(errors_get_count(opt.check.errors), 0);
  errors_print(opt.check.errors);
  pass_opt_done(&opt);

  const char* expect =
    "prelpre\"foo\"lpost lpre\"bar\"lpost lpre\"wombat\"lpost post";

  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, RepeatedLibs)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_TRUE(use_library(prog, "foo", NULL, &opt));
  ASSERT_TRUE(use_library(prog, "bar", NULL, &opt));
  ASSERT_TRUE(use_library(prog, "bar", NULL, &opt));
  ASSERT_TRUE(use_library(prog, "foo", NULL, &opt));
  ASSERT_TRUE(use_library(prog, "wombat", NULL, &opt));

  program_linker_build_args(prog, &opt, "", "", "" "", "", "", "", NULL, 0);

  ASSERT_EQ(errors_get_count(opt.check.errors), 0);
  errors_print(opt.check.errors);
  pass_opt_done(&opt);

  const char* expect = "\"foo\" \"bar\" \"wombat\" ";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}


TEST(ProgramTest, BadLibName)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);

  pass_opt_t opt;
  pass_opt_init(&opt);

  ASSERT_FALSE(use_library(prog, "foo\nbar", NULL, &opt));
  ASSERT_FALSE(use_library(prog, "foo\tbar", NULL, &opt));
  ASSERT_FALSE(use_library(prog, "foo;bar", NULL, &opt));
  ASSERT_FALSE(use_library(prog, "foo$bar", NULL, &opt));
  //ASSERT_FALSE(use_library(prog, "foo\\bar", NULL, &opt));

  ASSERT_EQ(errors_get_count(opt.check.errors), 4);
  pass_opt_done(&opt);

  ast_free(prog);
}


TEST(ProgramTest, LibPathsRelative)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);
  ast_scope(prog);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // create a fake package
  ast_t* pkg = create_package(prog, "/foo", "foo", &opt);
  ASSERT_NE((void*)NULL, pkg);

  // pretend we issue a use "path:..." from within that package
  ASSERT_TRUE(use_path(pkg, "bar", NULL, &opt));

  program_linker_build_args(prog, &opt, "static", "dynamic", "", "", "", "", NULL, 0);

  ASSERT_EQ(errors_get_count(opt.check.errors), 0);
  errors_print(opt.check.errors);
  pass_opt_done(&opt);

#ifdef PLATFORM_IS_WINDOWS
  const char* expect = "static\"/foo\\bar\" dynamic\"/foo\\bar\" ";
#else
  const char* expect = "static\"/foo/bar\" dynamic\"/foo/bar\" ";
#endif
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}

TEST(ProgramTest, LibPathsAbsolute)
{
  ast_t* prog = ast_blank(TK_PROGRAM);
  ASSERT_NE((void*)NULL, prog);
  ast_scope(prog);

  pass_opt_t opt;
  pass_opt_init(&opt);

  // create a fake package
  ast_t* pkg = create_package(prog, "/foo", "foo", &opt);
  ASSERT_NE((void*)NULL, pkg);

  // pretend we issue a use "path:..." from within that package
  ASSERT_TRUE(use_path(pkg, "/bar", NULL, &opt));

  program_linker_build_args(prog, &opt, "static", "dynamic", "", "", "", "", NULL, 0);

  ASSERT_EQ(errors_get_count(opt.check.errors), 0);
  errors_print(opt.check.errors);
  pass_opt_done(&opt);

  const char* expect = "static\"/bar\" dynamic\"/bar\" ";
  ASSERT_STREQ(expect, program_lib_args(prog));

  ast_free(prog);
}
