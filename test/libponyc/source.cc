#include <gtest/gtest.h>
#include <platform.h>

#include <ast/source.h>
#include <pkg/package.h>

#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#ifdef PLATFORM_IS_WINDOWS
#  include <direct.h>
#endif

/** Tests for loading Pony source from disk. A directory whose name ends in
 * .pony must not be treated as a source file: before the fix it reached
 * source_open, which on some filesystems opens a directory successfully and
 * then reads a meaningless length from ftell, aborting the compiler with a
 * huge allocation. parse_files_in_dir now skips such directories; source_open
 * rejects them as a backstop.
 */

#define TEST_COMPILE(src, pass) DO(test_compile(src, pass))


class SourceTest : public PassTest
{
protected:
  // Paths a test created, removed in TearDown in reverse order (a directory's
  // contents before the directory). TearDown runs even when a test fails
  // partway, which an inline cleanup at the end of the test body would not.
  std::vector<std::string> _created;

  void TearDown() override
  {
    for(size_t i = _created.size(); i > 0; i--)
    {
      const char* p = _created[i - 1].c_str();
      remove(p); // files, and empty directories on POSIX
#ifdef PLATFORM_IS_WINDOWS
      _rmdir(p); // the Windows CRT needs _rmdir for directories
#endif
    }
    _created.clear();
    PassTest::TearDown();
  }

  // mkdir, treating "already exists" as success. gtest-fatal on other
  // failures; call through DO().
  void make_dir(const char* path)
  {
#ifdef PLATFORM_IS_WINDOWS
    int r = _mkdir(path);
#else
    int r = mkdir(path, 0755);
#endif
    ASSERT_TRUE((r == 0) || (errno == EEXIST)) << "couldn't mkdir " << path;
    _created.push_back(path);
  }

  void write_file(const char* path, const char* contents)
  {
    FILE* f = fopen(path, "w");
    ASSERT_NE((void*)NULL, (void*)f) << "couldn't write " << path;
    fputs(contents, f);
    fclose(f);
    _created.push_back(path);
  }
};


// A directory whose name ends in .pony, sitting alongside a real source file,
// is skipped; the package still loads from its real .pony files. Before the
// fix, loading this package aborted (or, on some filesystems, silently
// mishandled the directory).
TEST_F(SourceTest, DirectoryNamedLikeSourceFileIsSkipped)
{
  const char* pkg = "source_test_pkg";
  char thing[FILENAME_MAX];
  char weird[FILENAME_MAX];
  snprintf(thing, sizeof(thing), "%s/thing.pony", pkg);
  snprintf(weird, sizeof(weird), "%s/weird.pony", pkg);

  DO(make_dir(pkg));
  DO(write_file(thing, "primitive Thing\n"));
  DO(make_dir(weird)); // a DIRECTORY named like a source file

  package_add_magic_path(pkg, pkg, &opt);

  const char* src =
    "use \"source_test_pkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  // Succeeds: weird.pony is skipped and thing.pony loads. A crash or an error
  // here is the bug.
  TEST_COMPILE(src, "scope");
}


// A package whose only .pony-named entry is a directory has no source files:
// the directory is skipped, leaving the package empty, so loading it produces
// the ordinary empty-package error rather than aborting. This pins the skip's
// effect on a dimension that holds on every filesystem -- remove the skip and
// the directory instead reaches source_open, yielding a different error ("is
// a directory") or a crash, never this one.
TEST_F(SourceTest, DirectoryOnlyPackageHasNoSourceFiles)
{
  const char* pkg = "source_test_empty_pkg";
  char weird[FILENAME_MAX];
  snprintf(weird, sizeof(weird), "%s/weird.pony", pkg);

  DO(make_dir(pkg));
  DO(make_dir(weird)); // the only .pony-named entry is a directory

  package_add_magic_path(pkg, pkg, &opt);

  const char* src =
    "use \"source_test_empty_pkg\"\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  const char* errs[] =
    { "no source files in package", "can't load package", NULL };
  DO(test_expected_errors(src, "scope", errs));
}


// source_open's backstop: opening a directory fails with a clear error rather
// than aborting. Discovery already skips directories; this guards any other
// caller from the same allocation abort.
TEST_F(SourceTest, SourceOpenRejectsDirectory)
{
  const char* error_msg = NULL;
  source_t* source = source_open(".", &error_msg, opt.strtab);

  // Rejected with an error, not aborted. The exact message is platform-
  // dependent, so don't assert it: where fopen opens a directory
  // (Linux/macOS) source_open's own check reports "is a directory"; on Windows
  // fopen refuses the directory first, so the failure is "can't open file".
  // Either way it is a clean error rather than the misread-length abort.
  ASSERT_EQ((void*)NULL, source);
  ASSERT_NE((void*)NULL, (void*)error_msg);
}
