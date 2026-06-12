#include <gtest/gtest.h>
#include <platform.h>

#include <ast/error.h>
#include <ast/stringtab.h>
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

// The four tests that invoke the embedded clang skip on Windows, where genc
// deliberately errors (MSVC include discovery is unimplemented);
// WindowsShimsReportNotSupported pins that contract instead.
#ifdef PLATFORM_IS_WINDOWS
#  define SKIP_ON_WINDOWS() \
    GTEST_SKIP() << "C shims are not supported when targeting Windows yet"
#else
#  define SKIP_ON_WINDOWS() do {} while(false)
#endif


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
    "use \"cinclude:\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "scope", "cinclude: requires a path");
}

TEST_F(CShimTest, CIncludeMayNotHaveAlias)
{
  const char* src =
    "use foo = \"cinclude:./include\"\n"
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

  // cinclude: paths resolve against the package directory (not the
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


// Compiling shims with the embedded clang (PASS_C / genc). These need the
// clang resource directory next to the test binary (build/libs in a build
// tree), which is present wherever the suite builds.

TEST_F(CShimTest, BadShimReportsClangErrorWithLocation)
{
  SKIP_ON_WINDOWS();

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

#ifdef PLATFORM_IS_WINDOWS
TEST_F(CShimTest, WindowsShimsReportNotSupported)
{
  const char* fixture = "cshim_fixture_windows";
  const char* names[] = {"dummy.pony", "good.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_win(void) { return 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  const char* src =
    "use \"shimpkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  // Until MSVC include discovery is implemented, a shim on a Windows
  // target is a clear error, not a cryptic header-not-found cascade.
  const char* errs[] =
    {"C shims are not yet supported when targeting Windows", NULL};
  DO(test_expected_errors(src, "c", errs));

  remove_fixture(fixture, names);
}
#endif

TEST_F(CShimTest, GoodShimProducesLinkableObject)
{
  SKIP_ON_WINDOWS();

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
  SKIP_ON_WINDOWS();

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

TEST_F(CShimTest, ShimErrorNoteJoinsErrorFrame)
{
  SKIP_ON_WINDOWS();

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
  SKIP_ON_WINDOWS();

  const char* fixture = "cshim_fixture_cross";
  const char* names[] = {"dummy.pony", "good.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_cross(void) { return 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  // A riscv64 target is foreign on every host that runs this suite.
  // opt.triple is LLVM-owned (codegen_pass_cleanup disposes it), so swap
  // in an LLVM-allocated copy and restore the original afterwards.
  char* saved_triple = opt.triple;
  opt.triple = LLVMCreateMessage("riscv64-unknown-linux-gnu");

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

TEST_F(CShimTest, ShimObjectGoesToCreatedOutputDir)
{
  SKIP_ON_WINDOWS();

  const char* fixture = "cshim_fixture_outdir";
  const char* names[] = {"dummy.pony", "good.c", NULL};
  const char* contents[] = {
    "primitive ShimPkg\n",
    "int shim_outdir(void) { return 1; }\n",
    NULL};

  DO(write_fixture(fixture, names, contents));
  package_add_magic_path("shimpkg", fixture, &opt);

  // -o names a directory that doesn't exist yet. codegen creates it, but
  // genc runs first and must create it too, or only shim programs would
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
  SKIP_ON_WINDOWS();

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
