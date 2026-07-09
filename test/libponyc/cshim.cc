#include <gtest/gtest.h>
#include <platform.h>

#include <ast/error.h>
#include <ast/stringtab.h>
#include <codegen/codegen.h>
#include <codegen/gencshim.h>
#include <pkg/package.h>
#include <pkg/program.h>

#include <llvm-c/Core.h>

#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef PLATFORM_IS_WINDOWS
#  include <direct.h>
#endif

/** Tests for C shims: the cincludedir:/cdefine: use schemes, .c discovery in
 * package directories, and compiling shims with the embedded clang (gencshim).
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
  // Write a fixture directory of named files, relative to the test
  // process's working directory. Failures are recorded as gtest fatal
  // failures; call through DO().
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

    remove_dir(dir);
  }

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

  static void remove_dir(const char* dir)
  {
    // POSIX remove() handles empty directories; the Windows CRT needs
    // _rmdir.
#ifdef PLATFORM_IS_WINDOWS
    _rmdir(dir);
#else
    remove(dir);
#endif
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

TEST_F(CShimTest, CDefineDuplicateValuedThenValueless)
{
  const char* src =
    "use \"cdefine:FOO=1\"\n"
    "use \"cdefine:FOO\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src, "scope",
    "C macro 'FOO' is already defined for this package as 'FOO=1'",
    "first definition is here");
}

TEST_F(CShimTest, CIncludeMissingPath)
{
  const char* src =
    "use \"cincludedir:\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "cincludedir: requires a path");
}

TEST_F(CShimTest, CIncludeMayNotHaveAlias)
{
  const char* src =
    "use foo = \"cincludedir:./include\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "may not have an alias");
}

TEST_F(CShimTest, CDefineMayNotHaveAlias)
{
  const char* src =
    "use foo = \"cdefine:FOO=1\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "may not have an alias");
}

TEST_F(CShimTest, CIncludeAbsolutePathStoredVerbatim)
{
#ifdef PLATFORM_IS_WINDOWS
  GTEST_SKIP() << "uses a POSIX absolute path form";
#endif

  // An absolute cincludedir: path skips package-dir resolution and is stored
  // as given.
  const char* src =
    "use \"cincludedir:/absolute/include dir\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");

  strlist_t* includes = package_c_includes(package);
  ASSERT_NE((void*)NULL, includes);
  EXPECT_STREQ("/absolute/include dir", strlist_data(includes));
}

TEST_F(CShimTest, ErrorfAtContinueWithNoPriorErrorBecomesPrimary)
{
  // errorf_at_continue's empty-collection branch promotes the would-be
  // continuation to a primary error instead of dropping it; unreachable
  // through clang fixtures, so pin it directly.
  errorf_at_continue(opt.check.errors, "orphan.c", 4, 7, "lone note");

  ASSERT_EQ((size_t)1, errors_get_count(opt.check.errors));
  errormsg_t* e = errors_get_first(opt.check.errors);
  ASSERT_NE((void*)NULL, e);
  EXPECT_TRUE(strstr(e->file, "orphan.c") != NULL);
  EXPECT_EQ((size_t)4, e->line);
  EXPECT_EQ((size_t)7, e->pos);
  EXPECT_STREQ("lone note", e->msg);
}

TEST_F(CShimTest, CIncludeGuardedOk)
{
  // allow_guard is a per-scheme flag on cincludedir's own handlers[] row;
  // cdefine's guard test doesn't cover it.
  const char* src =
    "use \"cincludedir:./linux-inc\" if linux\n"
    "use \"cincludedir:./windows-inc\" if windows\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");
}

TEST_F(CShimTest, CIncludePathWithSpacesStoredVerbatim)
{
  // Spaces in include paths are the design's stated reason cincludedir:
  // bypasses quoted_locator; pin that side of the decision like
  // CDefineValueStoredVerbatim pins the cdefine side.
  const char* src =
    "use \"cincludedir:dir with spaces\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");

  char expected[FILENAME_MAX];
  path_cat(package_path(package), "dir with spaces", expected);

  strlist_t* includes = package_c_includes(package);
  ASSERT_NE((void*)NULL, includes);
  EXPECT_STREQ(expected, strlist_data(includes));
}


// Deterministic ordering of the per-package C state. gencshim consumes these
// lists head->tail, so insertion order here is the clang argv order and (for
// sources) the link order; see the package_t comment in package.c.

TEST_F(CShimTest, CDefinesAndIncludesPreserveSourceOrder)
{
  const char* src =
    "use \"cdefine:ZULU=1\"\n"
    "use \"cdefine:ALPHA=2\"\n"
    "use \"cincludedir:zz\"\n"
    "use \"cincludedir:aa\"\n"
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

  // cincludedir: paths resolve against the package directory (not the
  // process's CWD), so the stored path is "<package path><slash><value>".
  char expected_first[FILENAME_MAX];
  char expected_second[FILENAME_MAX];
  path_cat(package_path(package), "zz", expected_first);
  path_cat(package_path(package), "aa", expected_second);

  strlist_t* includes = package_c_includes(package);
  ASSERT_NE((void*)NULL, includes);
  EXPECT_STREQ(expected_first, strlist_data(includes));
  includes = strlist_next(includes);
  ASSERT_NE((void*)NULL, includes);
  EXPECT_STREQ(expected_second, strlist_data(includes));
  EXPECT_EQ((void*)NULL, strlist_next(includes));
}

TEST_F(CShimTest, ShimInUnsafePackageRejected)
{
  // --safe gates compiling a shim exactly like a C FFI call: a package not
  // on the safe list doesn't get its .c compiled, and a .c there is an
  // error. The check is at discovery, so this fails as early as an unsafe
  // FFI call and needs no clang.
  const char* fixture = "cshim_fixture_unsafe";
  const char* names[] = {"dummy.pony", "shim.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_unsafe(void) { return 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  // package_add_safe("") adds only builtin to the safe list;
  // re-running package_init normalizes it to builtin's real path (the
  // normalization lives in package_init). shimpkg isn't on the list, so
  // its .c is rejected; builtin stays allowed so its own FFI is fine.
  package_add_safe("", &opt);
  package_init(&opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  // The shim denial, plus the cascading load failure for the package that
  // requested it.
  { const char* errs[] = {
      "this package isn't allowed to do C FFI, so its C source files can't "
      "be compiled as shims",
      "can't load package 'shimpkg'", NULL};
    DO(test_expected_errors(src, "scope", errs)); }

  remove_fixture(fixture, names);
}

TEST_F(CShimTest, CDefineValueStoredVerbatim)
{
  // Decision 7 in the design doc: directive values bypass quoted_locator
  // so quotes and spaces survive into the clang argv. This pins the raw
  // storage against a future "consistency with lib:/path:" cleanup that
  // would reject or rewrite these values.
  const char* src =
    "use \"cdefine:GREETING=\\\"hello world\\\"\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");

  strlist_t* defines = package_c_defines(package);
  ASSERT_NE((void*)NULL, defines);
  EXPECT_STREQ("GREETING=\"hello world\"", strlist_data(defines));
  EXPECT_EQ((void*)NULL, strlist_next(defines));
}

// Names that aren't C identifiers would dodge the duplicate check (clang's
// -D accepts function-like macros and space-separated values whose
// effective macro name isn't the text before '='), so they're rejected at
// the directive. One case per test: errors accumulate across builds within
// a single test.

TEST_F(CShimTest, CDefineFunctionLikeMacroRejected)
{
  const char* src =
    "use \"cdefine:FOO(x)=1\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "must be a C identifier");
}

TEST_F(CShimTest, CDefineSpaceInNameRejected)
{
  const char* src =
    "use \"cdefine:FOO BAR=1\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "must be a C identifier");
}

TEST_F(CShimTest, CDefineLeadingDigitRejected)
{
  const char* src =
    "use \"cdefine:2BAD=1\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "must be a C identifier");
}

TEST_F(CShimTest, CSourcesDiscoveredSortedAndScoped)
{
  const char* fixture = "cshim_fixture_sorted";

  // Reverse-alphabetical creation order so an unsorted implementation has
  // to get every position wrong, not just flip a coin on two entries. The
  // .cc/.cpp files and the subdirectory .c pin the v1 scope boundary:
  // C only, top directory only — losing either boundary silently shadows
  // `use "lib:"` libraries (the 2c-bis analysis in the design doc), so
  // discovery widening must fail here loudly.
  const char* names[] = {"dummy.pony", "zeta.c", "mid.c", "beta.c",
    "alpha.c", "stray.cc", "stray.cpp", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_zeta(void) { return 1; }\n",
    "int shim_mid(void) { return 2; }\n",
    "int shim_beta(void) { return 3; }\n",
    "int shim_alpha(void) { return 4; }\n",
    "this is not C and must never be compiled\n",
    "neither is this\n",
    NULL};

  DO(write_fixture(fixture, names, contents));

  char subdir[FILENAME_MAX];
  snprintf(subdir, sizeof(subdir), "%s/sub", fixture);
  ASSERT_EQ(0, mkdir_fixture(subdir));
  const char* sub_names[] = {"nested.c", NULL};
  const char* sub_contents[] = {"also never compiled\n", NULL};
  DO(write_fixture(subdir, sub_names, sub_contents));

  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "scope");

  ast_t* shimpkg = ast_get(program, stringtab(opt.strtab, "shimpkg"), NULL);
  ASSERT_NE((void*)NULL, shimpkg);

  const char* expected[] = {"alpha.c", "beta.c", "mid.c", "zeta.c"};
  strlist_t* sources = package_c_sources(shimpkg);

  for(size_t i = 0; i < 4; i++)
  {
    ASSERT_NE((void*)NULL, sources) << "missing source " << expected[i];
    const char* s = strlist_data(sources);
    EXPECT_TRUE(strstr(s, expected[i]) != NULL)
      << "source " << i << ": " << s;
    sources = strlist_next(sources);
  }

  // Exactly four: the .cc/.cpp strays and the subdirectory .c are not
  // discovered.
  EXPECT_EQ((void*)NULL, sources);

  remove_fixture(subdir, sub_names);
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


// Compiling shims with the embedded clang (PASS_C / gencshim). These need the
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

  const char* errs[] =
    {"use of undeclared identifier 'undeclared_thing'", NULL};
  DO(test_expected_errors(src, "c", errs));

  // The clang diagnostic arrives as a ponyc error attributed to the C file,
  // with the location in the error's own fields (so it prints
  // "file:line:pos: msg" like a native error), not embedded in the text.
  errormsg_t* e = errors_get_first(opt.check.errors);
  ASSERT_NE((void*)NULL, e);
  ASSERT_NE((void*)NULL, e->file);
  EXPECT_TRUE(strstr(e->file, "bad.c") != NULL) << "file: " << e->file;
  EXPECT_EQ((size_t)3, e->line);
  EXPECT_EQ((size_t)10, e->pos);

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

  // gencshim recorded exactly one object, and it exists on disk. Under
  // --pass c (a non-link mode) objects persist; tests clean up explicitly.
  ASSERT_EQ((size_t)1, program_c_object_count(program));
  const char* obj = program_c_object_at(program, 0);

  struct stat st;
  ASSERT_EQ(0, stat(obj, &st)) << "missing shim object: " << obj;
  EXPECT_GT(st.st_size, 0);

  remove(obj);
  remove_fixture(fixture, names);
}

TEST_F(CShimTest, BadShimIgnoredBelowPassC)
{
  // Front-end tools (lsp, doc, lint) stop at or before PASS_FINALISER and
  // must never invoke clang — a broken shim can't break them. This pins
  // the limit >= PASS_C gate in ast_passes_program.
  const char* fixture = "cshim_fixture_gate";
  const char* names[] = {"dummy.pony", "bad.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "this does not even tokenize as C\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "final");

  remove_fixture(fixture, names);
}

TEST_F(CShimTest, ShimWarningDoesNotFailBuild)
{
  const char* fixture = "cshim_fixture_warn";
  const char* names[] = {"dummy.pony", "warny.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "#warning this is only a warning\n"
    "int shim_warny(void) { return 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  // Warnings go to stderr, not into errors_t; the build succeeds and the
  // object is produced.
  TEST_COMPILE(src, "c");

  ASSERT_EQ((size_t)1, program_c_object_count(program));
  remove(program_c_object_at(program, 0));
  remove_fixture(fixture, names);
}

// A cdefine:/cincludedir: directive in a package with no .c is an error: the
// flags only affect .c files in the same package, so the directive is inert
// (usually it belongs in the package that holds the shim). The check is in
// gencshim, before any clang work, so it fires identically on every platform.
// The main package here is inline source with no .c, so it is itself the
// orphaned package.

TEST_F(CShimTest, CDefineWithoutCSourceErrors)
{
  const char* src =
    "use \"cdefine:FOO=1\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "c",
    "cdefine:/cincludedir: in a package with no C source files");
}

TEST_F(CShimTest, CIncludeWithoutCSourceErrors)
{
  const char* src =
    "use \"cincludedir:./inc\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "c",
    "cdefine:/cincludedir: in a package with no C source files");
}

TEST_F(CShimTest, MultipleShimsCompileAndRecordInSortedOrder)
{
  // The within-package compile/record order is a documented determinism
  // contract (sorted sources -> object record order -> link order); no
  // other test compiles more than one source per package, so a future
  // change (say, parallel shim compiles) recording out of order must fail
  // here.
  const char* fixture = "cshim_fixture_multi";
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

  TEST_COMPILE(src, "c");

  ASSERT_EQ((size_t)2, program_c_object_count(program));
  const char* first = program_c_object_at(program, 0);
  const char* second = program_c_object_at(program, 1);
  EXPECT_TRUE(strstr(first, "alpha") != NULL) << "first: " << first;
  EXPECT_TRUE(strstr(second, "zeta") != NULL) << "second: " << second;
  EXPECT_TRUE(strstr(first, ".shim.") != NULL) << "marker: " << first;

  remove(first);
  remove(second);
  remove_fixture(fixture, names);
}

TEST_F(CShimTest, ShimWarningWithNoteDoesNotFailBuild)
{
  // A macro redefinition emits a warning WITH a "previous definition"
  // note; both must stay on stderr — a regression routing warning-notes
  // into errors_t would fail user builds.
  const char* fixture = "cshim_fixture_warnnote";
  const char* names[] = {"dummy.pony", "redef_macro.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "#define TWICE 1\n"
    "#define TWICE 2\n"
    "int shim_redef(void) { return TWICE; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "c");

  EXPECT_EQ((size_t)0, errors_get_count(opt.check.errors));
  ASSERT_EQ((size_t)1, program_c_object_count(program));
  remove(program_c_object_at(program, 0));
  remove_fixture(fixture, names);
}

TEST_F(CShimTest, ShimObjectPathTooLongIsAnError)
{
  const char* fixture = "cshim_fixture_longout";
  const char* names[] = {"dummy.pony", "good.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_long(void) { return 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  // An output path so long the object name can't fit FILENAME_MAX must be
  // a clear error, not a mangled filename.
  static char long_output[FILENAME_MAX + 64];
  memset(long_output, 'x', sizeof(long_output) - 1);
  long_output[sizeof(long_output) - 1] = '\0';
  opt.output = long_output;

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  const char* errs[] =
    {"output path for C shim object is too long", NULL};
  DO(test_expected_errors(src, "c", errs));

  remove_fixture(fixture, names);
}

TEST_F(CShimTest, ShimCanIncludePonyHeader)
{
  // pony.h and ponyassert.h resolve relative to the running compiler (like
  // stdlib discovery), so shims can use runtime APIs without the user hard-
  // coding an installation path in cincludedir:. In a build tree this exercises
  // the ../../src/libponyrt and ../../src/common offsets; pony.h's own
  // <pony/detail/atomics.h> include covers the second, and ponyassert.h pulls
  // in platform.h's closure (threads.h/paths.h, plus vcvars.h on Windows),
  // confirming the whole shim-reachable header set resolves. The test binary
  // has no installed-tree sibling ../include/pony or ../include, so these
  // resolve through the build-tree offsets; the installed offsets are covered
  // only by a manual shim-against-install check.
  const char* fixture = "cshim_fixture_ponyh";
  const char* names[] = {"dummy.pony", "uses_pony_h.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "#include <pony.h>\n"
    "#include <ponyassert.h>\n"
    "int shim_msg_size(void) { return (int)sizeof(pony_msg_t); }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "c");

  ASSERT_EQ((size_t)1, program_c_object_count(program));
  remove(program_c_object_at(program, 0));
  remove_fixture(fixture, names);
}

#ifdef PLATFORM_IS_WINDOWS
// Windows-only: <windows.h> lives in the Windows SDK 'um'/'shared' include
// dirs. The other clang-invoking tests only reach 'ucrt' (<string.h>) and the
// MSVC include dir, so without this a wrong um/shared derivation would ship
// silently — add_system_include_args' "none of the dirs exist" guard stays
// satisfied by ucrt. Compiling a shim that includes <windows.h> and calls a
// Win32 API pins all four include dirs and the MS-extension flags together.
TEST_F(CShimTest, ShimCanIncludeWindowsHeader)
{
  const char* fixture = "cshim_fixture_winhdr";
  const char* names[] = {"dummy.pony", "win.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "#include <windows.h>\n"
    "int shim_pid_nonzero(void) { return GetCurrentProcessId() != 0; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "c");

  ASSERT_EQ((size_t)1, program_c_object_count(program));
  remove(program_c_object_at(program, 0));
  remove_fixture(fixture, names);
}
#endif

TEST_F(CShimTest, ShimErrorNoteJoinsErrorFrame)
{
  const char* fixture = "cshim_fixture_note";
  const char* names[] = {"dummy.pony", "redef.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int x;\n"
    "double x;\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  // clang's note attaches to the error's frame (the same shape ponyc's own
  // two-location errors have), not lost and not a separate error.
  TEST_ERROR_WITH_NOTE(src, "c",
    "redefinition of 'x'",
    "previous definition is here");

  remove_fixture(fixture, names);
}

TEST_F(CShimTest, CrossCompiledShimRequiresSysroot)
{
  const char* fixture = "cshim_fixture_cross";
  const char* names[] = {"dummy.pony", "good.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_cross(void) { return 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  // Cross-compile to a target whose toolchain is absent on every host that
  // runs this suite, so the shared cross-toolchain probe
  // (find_cross_toolchain_sysroot, the same one the linker uses) finds nothing
  // in the standard /usr/<triple> locations and emits the "requires --sysroot"
  // error. This exercises the probe's not-found path; its success path is
  // covered by the cross-compile CI jobs through the linker.
  //
  // It must be riscv32, not riscv64: the riscv64 cross-CI container that runs
  // this suite ships a riscv64 toolchain the probe WOULD find, so it would
  // skip the error and try to compile (failing instead on the host CPU name).
  // No host running the suite has a riscv32 toolchain. Don't use any of ponyc's
  // own cross-CI targets (riscv64, arm) here -- each is present in its own
  // container and would defeat the not-found path.
  // opt.triple is LLVM-owned (codegen_pass_cleanup disposes it), so swap
  // in an LLVM-allocated copy and restore the original afterwards.
  char* saved_triple = opt.triple;
  opt.triple = LLVMCreateMessage("riscv32-unknown-linux-gnu");

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  const char* errs[] = {"requires --sysroot", NULL};
  DO(test_expected_errors(src, "c", errs));

  LLVMDisposeMessage(opt.triple);
  opt.triple = saved_triple;

  remove_fixture(fixture, names);
}

TEST_F(CShimTest, CrossCompileDefaultsCpuToTargetGeneric)
{
  // Regression: codegen_pass_init must not default opt->cpu to the build host's
  // CPU when cross-compiling. A host CPU name (e.g. x86's "znver3") is invalid
  // for a foreign target arch; LLVM's target machine tolerates it (falls back
  // to generic) but the clang that gencshim drives to compile C shims rejects
  // it outright ("unknown target CPU"). With no --cpu and a cross triple the
  // CPU must be the empty string -- the target's generic baseline -- which also
  // makes gencshim omit -target-cpu entirely (its guard skips an empty CPU).
  //
  // SetUp already ran codegen_pass_init for the native host, so opt.cpu is the
  // host CPU now; dispose those defaults and re-run with a cross triple and no
  // CPU. codegen_pass_init copies the triple and allocates cpu/features, which
  // the fixture's codegen_pass_cleanup disposes in TearDown; triple_buf is a
  // local, so nothing leaks. riscv64 is used because it is cross on every host
  // that runs this suite (x86_64 and aarch64 alike).
  LLVMDisposeMessage(opt.triple);
  LLVMDisposeMessage(opt.cpu);
  LLVMDisposeMessage(opt.features);
  char triple_buf[] = "riscv64-unknown-linux-gnu";
  opt.triple = triple_buf;
  opt.cpu = NULL;
  opt.features = NULL;

  ASSERT_TRUE(codegen_pass_init(&opt));

  ASSERT_NE((void*)NULL, (void*)opt.cpu);
  EXPECT_STREQ("", opt.cpu);
}

// Helpers for ShimTargetArgsDerivation: search a cc1 argv for a flag, a flag
// immediately followed by an expected value, or count a flag's occurrences.
static bool cshim_args_has(const std::vector<const char*>& a, const char* flag)
{
  for(size_t i = 0; i < a.size(); i++)
    if(strcmp(a[i], flag) == 0)
      return true;
  return false;
}

static bool cshim_args_pair(const std::vector<const char*>& a,
  const char* flag, const char* val)
{
  for(size_t i = 0; i + 1 < a.size(); i++)
    if((strcmp(a[i], flag) == 0) && (strcmp(a[i + 1], val) == 0))
      return true;
  return false;
}

static int cshim_args_count(const std::vector<const char*>& a, const char* flag)
{
  int n = 0;
  for(size_t i = 0; i < a.size(); i++)
    if(strcmp(a[i], flag) == 0)
      n++;
  return n;
}

TEST_F(CShimTest, ShimTargetArgsDerivation)
{
  // The cc1 target/ABI flags must match what the Pony codegen targets, or the
  // shim object is ABI-incompatible with the program it links into. gencshim
  // hands its argv straight to clang, so cshim_target_args is the only
  // observation point -- assert the exact derivation here.
  char triple_buf[] = "x86_64-unknown-linux-gnu";
  char* saved_triple = opt.triple;
  char* saved_cpu = opt.cpu;
  char* saved_features = opt.features;
  char* saved_abi = opt.abi;
  bool saved_pic = opt.pic;

  opt.triple = triple_buf;

  // Empty CPU/features/ABI, no PIC: only -triple, nothing else. The empty-CPU
  // omission is exactly what CrossCompileDefaultsCpuToTargetGeneric relies on
  // (a cross build defaults opt.cpu to "").
  char empty[] = "";
  opt.cpu = empty;
  opt.features = empty;
  opt.abi = empty;
  opt.pic = false;
  {
    std::vector<const char*> args;
    cshim_target_args(&opt, args);
    EXPECT_TRUE(cshim_args_pair(args, "-triple", "x86_64-unknown-linux-gnu"));
    EXPECT_FALSE(cshim_args_has(args, "-target-cpu"));
    EXPECT_FALSE(cshim_args_has(args, "-target-feature"));
    EXPECT_FALSE(cshim_args_has(args, "-target-abi"));
    EXPECT_FALSE(cshim_args_has(args, "-mrelocation-model"));
    EXPECT_FALSE(cshim_args_has(args, "-pic-level"));
  }

  // CPU, comma-separated features (one -target-feature each), ABI, and PIC.
  char cpu[] = "x86-64-v2";
  char features[] = "+sse2,+avx2,+fma";
  char abi[] = "lp64d";
  opt.cpu = cpu;
  opt.features = features;
  opt.abi = abi;
  opt.pic = true;
  {
    std::vector<const char*> args;
    cshim_target_args(&opt, args);
    EXPECT_TRUE(cshim_args_pair(args, "-target-cpu", "x86-64-v2"));
    EXPECT_EQ(3, cshim_args_count(args, "-target-feature"));
    EXPECT_TRUE(cshim_args_pair(args, "-target-feature", "+sse2"));
    EXPECT_TRUE(cshim_args_pair(args, "-target-feature", "+avx2"));
    EXPECT_TRUE(cshim_args_pair(args, "-target-feature", "+fma"));
    EXPECT_TRUE(cshim_args_pair(args, "-target-abi", "lp64d"));
    EXPECT_TRUE(cshim_args_pair(args, "-mrelocation-model", "pic"));
    EXPECT_TRUE(cshim_args_pair(args, "-pic-level", "2"));
  }

  // PIC with empty CPU/features/ABI: the PIC flags must be emitted independently
  // of the others, so a build that targets the generic CPU (empty) still gets a
  // position-independent shim. Guards against the PIC emission being coupled to
  // any of the target fields.
  opt.cpu = empty;
  opt.features = empty;
  opt.abi = empty;
  opt.pic = true;
  {
    std::vector<const char*> args;
    cshim_target_args(&opt, args);
    EXPECT_TRUE(cshim_args_pair(args, "-mrelocation-model", "pic"));
    EXPECT_TRUE(cshim_args_pair(args, "-pic-level", "2"));
    EXPECT_FALSE(cshim_args_has(args, "-target-cpu"));
    EXPECT_FALSE(cshim_args_has(args, "-target-feature"));
    EXPECT_FALSE(cshim_args_has(args, "-target-abi"));
  }

  opt.triple = saved_triple;
  opt.cpu = saved_cpu;
  opt.features = saved_features;
  opt.abi = saved_abi;
  opt.pic = saved_pic;
}

TEST_F(CShimTest, ShimObjectGoesToCreatedOutputDir)
{
  const char* fixture = "cshim_fixture_outdir";
  const char* names[] = {"dummy.pony", "good.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_outdir(void) { return 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  // -o names a directory that doesn't exist yet. codegen creates it, but
  // gencshim runs first and must create it too, or only shim programs would
  // fail this invocation.
  const char* outdir = "cshim_outdir_new";
  remove(outdir);
  opt.output = outdir;

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src, "c");

  ASSERT_EQ((size_t)1, program_c_object_count(program));
  const char* obj = program_c_object_at(program, 0);
  EXPECT_TRUE(strstr(obj, outdir) != NULL) << "object not in -o dir: " << obj;

  struct stat st;
  ASSERT_EQ(0, stat(obj, &st)) << "missing shim object: " << obj;

  remove(obj);
  remove(outdir);
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
