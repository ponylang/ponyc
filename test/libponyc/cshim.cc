#include <gtest/gtest.h>
#include <platform.h>

#include <ast/stringtab.h>
#include <pkg/package.h>
#include <pkg/program.h>

#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef PLATFORM_IS_WINDOWS
#  include <direct.h>
#endif

/** Tests for C shims: the cinclude:/cdefine: use schemes, .c discovery in
 * package directories, and compiling shims with the embedded clang (genc).
 *
 * Scheme handling and the cdefine: duplicate check run in the scope pass on
 * plain inline source. Tests that need a .c on disk write a fixture
 * directory at runtime and map a package onto it with
 * package_add_magic_path, so parse_files_in_dir discovers the .c exactly as
 * it would in a real package directory.
 */

#define TEST_COMPILE(src, pass) DO(test_compile(src, pass))

#define TEST_ERRORS_1(src, pass, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, pass, errs)); }

// Single error plus a continuation frame, both matched as substrings.
#define TEST_ERROR_WITH_NOTE(src, pass, primary, note) \
  { const char* errs[] = {primary, NULL}; \
    const char* frame_strs[] = {note, NULL}; \
    const char** frames[] = {frame_strs, NULL}; \
    DO(test_expected_error_frames(src, pass, errs, frames)); }


class CShimTest : public PassTest
{
protected:
  // Write a fixture directory of named files next to the test binary.
  // Returns false (with a gtest failure recorded) if any write fails.
  void write_fixture(const char* dir, const char** names,
    const char** contents)
  {
    ASSERT_EQ(0, mkdir_fixture(dir));

    for(size_t i = 0; names[i] != NULL; i++)
    {
      char path[FILENAME_MAX];
      snprintf(path, sizeof(path), "%s/%s", dir, names[i]);

      FILE* f = fopen(path, "w");
      ASSERT_NE((void*)NULL, (void*)f) << "couldn't write " << path;
      fputs(contents[i], f);
      fclose(f);
    }
  }

  void remove_fixture(const char* dir, const char** names)
  {
    for(size_t i = 0; names[i] != NULL; i++)
    {
      char path[FILENAME_MAX];
      snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
      remove(path);
    }

    remove(dir);
  }

private:
  static int mkdir_fixture(const char* dir)
  {
#ifdef PLATFORM_IS_WINDOWS
    int r = _mkdir(dir);
#else
    int r = mkdir(dir, 0755);
#endif
    if(r != 0 && errno == EEXIST)
      return 0;
    return r;
  }
};


// cdefine: scheme handling (no .c or clang needed; runs in the scope pass).

TEST_F(CShimTest, CDefineDuplicateConflictingValues)
{
  const char* src =
    "use \"cdefine:FOO=1\"\n"
    "use \"cdefine:FOO=2\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src, "scope",
    "C macro 'FOO' is already defined for this package as 'FOO=1'",
    "first definition is here");
}

TEST_F(CShimTest, CDefineDuplicateIdenticalValues)
{
  const char* src =
    "use \"cdefine:FOO=1\"\n"
    "use \"cdefine:FOO=1\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src, "scope",
    "C macro 'FOO' is already defined for this package",
    "first definition is here");
}

TEST_F(CShimTest, CDefineDuplicateValuelessThenValued)
{
  const char* src =
    "use \"cdefine:FOO\"\n"
    "use \"cdefine:FOO=2\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src, "scope",
    "C macro 'FOO' is already defined for this package as 'FOO'",
    "first definition is here");
}

TEST_F(CShimTest, CDefineDistinctNamesOk)
{
  const char* src =
    "use \"cdefine:FOO=1\"\n"
    "use \"cdefine:FOOBAR=2\"\n"
    "use \"cdefine:BAR\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");
}

TEST_F(CShimTest, CDefineGuardedCrossPlatformOk)
{
  // A guarded-out directive never reaches the handler, so same-name defines
  // for different targets don't conflict.
  const char* src =
    "use \"cdefine:FOO=1\" if linux\n"
    "use \"cdefine:FOO=2\" if windows\n"
    "use \"cdefine:FOO=3\" if osx\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");
}

TEST_F(CShimTest, CDefineMissingName)
{
  const char* src =
    "use \"cdefine:\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "cdefine: requires a macro name");
}

TEST_F(CShimTest, CDefineValueOnlyMissingName)
{
  const char* src =
    "use \"cdefine:=1\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "cdefine: requires a macro name");
}

TEST_F(CShimTest, CIncludeMissingPath)
{
  const char* src =
    "use \"cinclude:\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "cinclude: requires a path");
}

TEST_F(CShimTest, CDefineMayNotHaveAlias)
{
  const char* src =
    "use foo = \"cdefine:FOO=1\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "may not have an alias");
}


// Deterministic ordering of the per-package C state. genc consumes these
// lists head->tail, so insertion order here is the clang argv order and (for
// sources) the link order; see the package_t comment in package.c.

TEST_F(CShimTest, CDefinesAndIncludesPreserveSourceOrder)
{
  const char* src =
    "use \"cdefine:ZULU=1\"\n"
    "use \"cdefine:ALPHA=2\"\n"
    "use \"cinclude:zz\"\n"
    "use \"cinclude:aa\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");

  strlist_t* defines = package_c_defines(package);
  ASSERT_NE((void*)NULL, defines);
  EXPECT_STREQ("ZULU=1", strlist_data(defines));
  defines = strlist_next(defines);
  ASSERT_NE((void*)NULL, defines);
  EXPECT_STREQ("ALPHA=2", strlist_data(defines));
  EXPECT_EQ((void*)NULL, strlist_next(defines));

  // cinclude: paths resolve relative to the package directory, so check
  // order by suffix rather than exact match.
  strlist_t* includes = package_c_includes(package);
  ASSERT_NE((void*)NULL, includes);
  const char* first = strlist_data(includes);
  EXPECT_STREQ("zz", first + strlen(first) - 2);
  includes = strlist_next(includes);
  ASSERT_NE((void*)NULL, includes);
  const char* second = strlist_data(includes);
  EXPECT_STREQ("aa", second + strlen(second) - 2);
  EXPECT_EQ((void*)NULL, strlist_next(includes));
}

TEST_F(CShimTest, CSourcesDiscoveredSorted)
{
  const char* fixture = "cshim_fixture_sorted";
  const char* names[] = {"dummy.pony", "zeta.c", "alpha.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_zeta(void) { return 1; }\n",
    "int shim_alpha(void) { return 2; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");

  ast_t* shimpkg = ast_get(program, stringtab(opt.strtab, "shimpkg"), NULL);
  ASSERT_NE((void*)NULL, shimpkg);

  // Directory order is not deterministic; discovery must sort, so alpha.c
  // precedes zeta.c regardless of creation order.
  strlist_t* sources = package_c_sources(shimpkg);
  ASSERT_NE((void*)NULL, sources);
  const char* a = strlist_data(sources);
  EXPECT_TRUE(strstr(a, "alpha.c") != NULL) << "first source: " << a;
  sources = strlist_next(sources);
  ASSERT_NE((void*)NULL, sources);
  const char* z = strlist_data(sources);
  EXPECT_TRUE(strstr(z, "zeta.c") != NULL) << "second source: " << z;
  EXPECT_EQ((void*)NULL, strlist_next(sources));

  remove_fixture(fixture, names);
}

TEST_F(CShimTest, COnlyDirectoryIsNotAPackage)
{
  const char* fixture = "cshim_fixture_c_only";
  const char* names[] = {"only.c", NULL};
  const char* contents[] = {"int only(void) { return 1; }\n", NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("conlypkg", fixture, &opt);

  const char* src =
    "use \"conlypkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  // The failed load also fails the use command that requested the package.
  { const char* errs[] = {"no Pony source files in package",
      "can't load package 'conlypkg'", NULL};
    DO(test_expected_errors(src, "scope", errs)); }

  remove_fixture(fixture, names);
}


// Compiling shims with the embedded clang (PASS_C / genc). These need the
// clang resource directory next to the test binary (build/libs in a build
// tree), which is present wherever the suite builds.

TEST_F(CShimTest, BadShimReportsClangErrorWithLocation)
{
  const char* fixture = "cshim_fixture_bad";
  const char* names[] = {"dummy.pony", "bad.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int broken(void)\n"
    "{\n"
    "  return undeclared_thing;\n"
    "}\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  // The clang diagnostic arrives as a ponyc error attributed to the C file,
  // with the line and column in the message.
  const char* errs[] =
    {"3:10: use of undeclared identifier 'undeclared_thing'", NULL};
  DO(test_expected_errors(src, "c", errs));

  remove_fixture(fixture, names);
}

TEST_F(CShimTest, GoodShimProducesLinkableObject)
{
  const char* fixture = "cshim_fixture_good";
  const char* names[] = {"dummy.pony", "good.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_good(int a) { return a + 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "c");

  // genc recorded exactly one object, and it exists on disk. Under
  // --pass c (a non-link mode) objects persist; tests clean up explicitly.
  ASSERT_EQ((size_t)1, program_c_object_count(program));
  const char* obj = program_c_object_at(program, 0);

  struct stat st;
  ASSERT_EQ(0, stat(obj, &st)) << "missing shim object: " << obj;
  EXPECT_GT(st.st_size, 0);

  remove(obj);
  remove_fixture(fixture, names);
}

TEST_F(CShimTest, ShimRespectsCDefine)
{
  const char* fixture = "cshim_fixture_define";
  const char* names[] = {"dummy.pony", "gated.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "#ifndef THE_ANSWER\n"
    "#error THE_ANSWER must be defined\n"
    "#endif\n"
    "int shim_answer(void) { return THE_ANSWER; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  // Without the define the shim fails (proves the #error path is live).
  // Preprocessing continues past #error, so the use of THE_ANSWER below it
  // reports too.
  const char* bad_src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  const char* errs[] = {"THE_ANSWER must be defined",
    "use of undeclared identifier 'THE_ANSWER'", NULL};
  DO(test_expected_errors(bad_src, "c", errs));

  // ...and the cdefine: in the package that owns the shim satisfies it.
  // (Defines are per-package: they go in shimpkg's own source, not Main's.)
  char gated_pony[FILENAME_MAX];
  snprintf(gated_pony, sizeof(gated_pony), "%s/%s", fixture, "dummy.pony");
  FILE* f = fopen(gated_pony, "w");
  ASSERT_NE((void*)NULL, (void*)f);
  fputs("use \"cdefine:THE_ANSWER=42\"\nprimitive ShimPkg\n", f);
  fclose(f);

  TEST_COMPILE(bad_src, "c");

  ASSERT_EQ((size_t)1, program_c_object_count(program));
  remove(program_c_object_at(program, 0));
  remove_fixture(fixture, names);
}
