#include <gtest/gtest.h>
#include <platform.h>

#include <ast/stringtab.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>

#ifdef PLATFORM_IS_POSIX_BASED

#include <algorithm>
#include <filesystem>
#include <vector>

class NixenvTest: public testing::Test
{
protected:
  strtable_t* strtab;
  char tmpdir[PATH_MAX];
  bool owns_tmpdir = false;

  const char* saved_nix_cc;
  const char* saved_nix_ldflags;
  const char* saved_nix_cflags_compile;
  bool had_nix_cc;
  bool had_nix_ldflags;
  bool had_nix_cflags_compile;

  virtual void SetUp()
  {
    strtab = stringtab_new();

    const char* v;
    v = getenv("NIX_CC");
    had_nix_cc = (v != NULL);
    saved_nix_cc = v ? strdup(v) : NULL;

    v = getenv("NIX_LDFLAGS");
    had_nix_ldflags = (v != NULL);
    saved_nix_ldflags = v ? strdup(v) : NULL;

    v = getenv("NIX_CFLAGS_COMPILE");
    had_nix_cflags_compile = (v != NULL);
    saved_nix_cflags_compile = v ? strdup(v) : NULL;

    const char* temporary = getenv("TMPDIR");
    if(temporary == NULL || temporary[0] == '\0')
      temporary = "/tmp";
    int n = snprintf(tmpdir, sizeof(tmpdir), "%s/nixenv_test_XXXXXX",
      temporary);
    ASSERT_GT(n, 0);
    ASSERT_LT((size_t)n, sizeof(tmpdir));
    char* result = mkdtemp(tmpdir);
    ASSERT_NE(result, (char*)NULL);
    owns_tmpdir = true;
  }

  virtual void TearDown()
  {
    if(had_nix_cc)
      setenv("NIX_CC", saved_nix_cc, 1);
    else
      unsetenv("NIX_CC");

    if(had_nix_ldflags)
      setenv("NIX_LDFLAGS", saved_nix_ldflags, 1);
    else
      unsetenv("NIX_LDFLAGS");

    if(had_nix_cflags_compile)
      setenv("NIX_CFLAGS_COMPILE", saved_nix_cflags_compile, 1);
    else
      unsetenv("NIX_CFLAGS_COMPILE");

    free((void*)saved_nix_cc);
    free((void*)saved_nix_ldflags);
    free((void*)saved_nix_cflags_compile);

    if(owns_tmpdir)
    {
      std::error_code error;
      std::filesystem::remove_all(tmpdir, error);
      EXPECT_FALSE(error) << error.message();
    }

    stringtab_free(strtab);
  }

  void create_nix_support()
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/nix-support", tmpdir);
    ASSERT_EQ(mkdir(path, 0755), 0);
  }

  void write_nix_support_bytes(const std::string& content)
  {
    std::string path = std::string(tmpdir) + "/nix-support/orig-libc-dev";
    FILE* f = fopen(path.c_str(), "wb");
    ASSERT_NE(f, (FILE*)NULL);
    size_t written = fwrite(content.data(), 1, content.size(), f);
    int closed = fclose(f);
    ASSERT_EQ(written, content.size());
    ASSERT_EQ(closed, 0);
  }

  void create_dynamic_linker(const char* content)
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/nix-support/dynamic-linker", tmpdir);
    FILE* f = fopen(path, "w");
    ASSERT_NE(f, (FILE*)NULL);
    fprintf(f, "%s\n", content);
    fclose(f);
  }

  void write_nix_support_file(const char* filename, const char* content)
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/nix-support/%s", tmpdir, filename);
    FILE* f = fopen(path, "w");
    ASSERT_NE(f, (FILE*)NULL);
    fprintf(f, "%s\n", content);
    fclose(f);
  }

  // Create a fake glibc prefix with an actual lib/ directory so dir_exists()
  // succeeds.  Returns the prefix path (inside tmpdir).
  void create_fake_libc_prefix(char* prefix_out, size_t prefix_size)
  {
    snprintf(prefix_out, prefix_size, "%s/fake-glibc", tmpdir);
    mkdir(prefix_out, 0755);
    char libdir[PATH_MAX];
    snprintf(libdir, sizeof(libdir), "%s/lib", prefix_out);
    mkdir(libdir, 0755);
  }
};

TEST_F(NixenvTest, InactiveWhenNixCCUnset)
{
  unsetenv("NIX_CC");

  nixenv_t nix;
  ASSERT_FALSE(nixenv_probe(&nix, strtab));
}

TEST_F(NixenvTest, InactiveWhenNixCCEmpty)
{
  setenv("NIX_CC", "", 1);

  nixenv_t nix;
  ASSERT_FALSE(nixenv_probe(&nix, strtab));
}

TEST_F(NixenvTest, InactiveWhenNixSupportMissing)
{
  // NIX_CC set but no nix-support/ directory.
  setenv("NIX_CC", tmpdir, 1);

  nixenv_t nix;
  ASSERT_FALSE(nixenv_probe(&nix, strtab));
}

TEST_F(NixenvTest, ActiveWhenNixSupportExists)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  unsetenv("NIX_CFLAGS_COMPILE");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.dynamic_linker, (const char*)NULL);
  ASSERT_EQ(nix.lib_dir_count, (size_t)0);
  ASSERT_EQ(nix.include_dir_count, (size_t)0);
}

TEST_F(NixenvTest, DynamicLinkerRead)
{
  create_nix_support();
  create_dynamic_linker(
    "/nix/store/abc123-glibc/lib/ld-linux-x86-64.so.2");
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  unsetenv("NIX_CFLAGS_COMPILE");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_NE(nix.dynamic_linker, (const char*)NULL);
  ASSERT_STREQ(nix.dynamic_linker,
    "/nix/store/abc123-glibc/lib/ld-linux-x86-64.so.2");
}

TEST_F(NixenvTest, DynamicLinkerEmptyFile)
{
  create_nix_support();
  // Write an empty file.
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/nix-support/dynamic-linker", tmpdir);
  FILE* f = fopen(path, "w");
  ASSERT_NE(f, (FILE*)NULL);
  fclose(f);

  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  unsetenv("NIX_CFLAGS_COMPILE");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.dynamic_linker, (const char*)NULL);
}

TEST_F(NixenvTest, ParseLDirs)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  setenv("NIX_LDFLAGS",
    "-L/nix/store/aaa-glibc/lib -L/nix/store/bbb-openssl/lib"
    " -rpath /nix/store/ccc-something/lib"
    " -L/nix/store/ddd-zlib/lib", 1);
  unsetenv("NIX_CFLAGS_COMPILE");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.lib_dir_count, (size_t)3);
  ASSERT_STREQ(nix.lib_dirs[0], "/nix/store/aaa-glibc/lib");
  ASSERT_STREQ(nix.lib_dirs[1], "/nix/store/bbb-openssl/lib");
  ASSERT_STREQ(nix.lib_dirs[2], "/nix/store/ddd-zlib/lib");
}

TEST_F(NixenvTest, ParseLDirsSpaceSeparated)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  setenv("NIX_LDFLAGS", "-L /nix/store/aaa/lib -L/nix/store/bbb/lib", 1);
  unsetenv("NIX_CFLAGS_COMPILE");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.lib_dir_count, (size_t)2);
  ASSERT_STREQ(nix.lib_dirs[0], "/nix/store/aaa/lib");
  ASSERT_STREQ(nix.lib_dirs[1], "/nix/store/bbb/lib");
}

TEST_F(NixenvTest, ParseLDirsEmpty)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  setenv("NIX_LDFLAGS", "", 1);
  unsetenv("NIX_CFLAGS_COMPILE");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.lib_dir_count, (size_t)0);
}

TEST_F(NixenvTest, ParseLDirsTrailingBareFlag)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  setenv("NIX_LDFLAGS",
    "-L/nix/store/aaa/lib -L", 1);
  unsetenv("NIX_CFLAGS_COMPILE");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.lib_dir_count, (size_t)1);
  ASSERT_STREQ(nix.lib_dirs[0], "/nix/store/aaa/lib");
}

TEST_F(NixenvTest, ParseIsystemDirs)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  setenv("NIX_CFLAGS_COMPILE",
    "-isystem /nix/store/aaa-glibc/include"
    " -isystem /nix/store/bbb-openssl/include"
    " -I /nix/store/ccc-something/include", 1);

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  // Only -isystem paths, not -I.
  ASSERT_EQ(nix.include_dir_count, (size_t)2);
  ASSERT_STREQ(nix.include_dirs[0], "/nix/store/aaa-glibc/include");
  ASSERT_STREQ(nix.include_dirs[1], "/nix/store/bbb-openssl/include");
}

TEST_F(NixenvTest, ParseIsystemDirsEmpty)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  setenv("NIX_CFLAGS_COMPILE", "", 1);

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.include_dir_count, (size_t)0);
}

TEST_F(NixenvTest, TruncatesAtMaxDirs)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_CFLAGS_COMPILE");

  // Build a NIX_LDFLAGS with more than NIXENV_MAX_DIRS entries.
  std::string flags;
  for(int i = 0; i < NIXENV_MAX_DIRS + 5; i++)
  {
    char entry[64];
    snprintf(entry, sizeof(entry), "-L/nix/store/dir%d/lib ", i);
    flags += entry;
  }
  setenv("NIX_LDFLAGS", flags.c_str(), 1);

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.lib_dir_count, (size_t)NIXENV_MAX_DIRS);
  ASSERT_STREQ(nix.lib_dirs[0], "/nix/store/dir0/lib");
  ASSERT_STREQ(nix.lib_dirs[NIXENV_MAX_DIRS - 1],
    "/nix/store/dir63/lib");
}

TEST_F(NixenvTest, TruncatesIncludeDirsAtMaxDirs)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");

  std::string flags;
  for(int i = 0; i < NIXENV_MAX_DIRS + 5; i++)
  {
    char entry[80];
    snprintf(entry, sizeof(entry), "-isystem /nix/store/inc%d/include ", i);
    flags += entry;
  }
  setenv("NIX_CFLAGS_COMPILE", flags.c_str(), 1);

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.include_dir_count, (size_t)NIXENV_MAX_DIRS);
  ASSERT_STREQ(nix.include_dirs[0], "/nix/store/inc0/include");
  ASSERT_STREQ(nix.include_dirs[NIXENV_MAX_DIRS - 1],
    "/nix/store/inc63/include");
}

TEST_F(NixenvTest, OrigLibcAddsLibDir)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  unsetenv("NIX_CFLAGS_COMPILE");

  char prefix[PATH_MAX];
  create_fake_libc_prefix(prefix, sizeof(prefix));
  write_nix_support_file("orig-libc", prefix);

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.lib_dir_count, (size_t)1);

  char expected[PATH_MAX];
  snprintf(expected, sizeof(expected), "%s/lib", prefix);
  ASSERT_STREQ(nix.lib_dirs[0], expected);
}

TEST_F(NixenvTest, CcLdflagsAddsLibDirs)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  unsetenv("NIX_CFLAGS_COMPILE");

  write_nix_support_file("cc-ldflags",
    "-L/nix/store/gcc-lib/lib -L/nix/store/gcc-lib2/lib");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.lib_dir_count, (size_t)2);
  ASSERT_STREQ(nix.lib_dirs[0], "/nix/store/gcc-lib/lib");
  ASSERT_STREQ(nix.lib_dirs[1], "/nix/store/gcc-lib2/lib");
}

TEST_F(NixenvTest, AllLibDirSourcesCombined)
{
  create_nix_support();
  setenv("NIX_CC", tmpdir, 1);
  setenv("NIX_LDFLAGS",
    "-L/nix/store/zlib/lib -L/nix/store/openssl/lib", 1);
  unsetenv("NIX_CFLAGS_COMPILE");

  char prefix[PATH_MAX];
  create_fake_libc_prefix(prefix, sizeof(prefix));
  write_nix_support_file("orig-libc", prefix);
  write_nix_support_file("cc-ldflags", "-L/nix/store/gcc/lib");

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  // 2 from NIX_LDFLAGS + 1 from orig-libc + 1 from cc-ldflags = 4
  ASSERT_EQ(nix.lib_dir_count, (size_t)4);
  ASSERT_STREQ(nix.lib_dirs[0], "/nix/store/zlib/lib");
  ASSERT_STREQ(nix.lib_dirs[1], "/nix/store/openssl/lib");

  char expected_libc[PATH_MAX];
  snprintf(expected_libc, sizeof(expected_libc), "%s/lib", prefix);
  ASSERT_STREQ(nix.lib_dirs[2], expected_libc);

  ASSERT_STREQ(nix.lib_dirs[3], "/nix/store/gcc/lib");
}

TEST_F(NixenvTest, OrigLibcDevAddsIncludeDir)
{
  ASSERT_NO_FATAL_FAILURE(create_nix_support());
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  unsetenv("NIX_CFLAGS_COMPILE");

  std::string development = std::string(tmpdir) + "/development";
  std::string runtime = std::string(tmpdir) + "/runtime";
  std::string second = std::string(tmpdir) + "/second";
  ASSERT_TRUE(std::filesystem::create_directories(development + "/include"));
  ASSERT_TRUE(std::filesystem::create_directories(runtime + "/include"));
  ASSERT_TRUE(std::filesystem::create_directories(runtime + "/lib"));
  ASSERT_TRUE(std::filesystem::create_directories(second + "/include"));
  ASSERT_NO_FATAL_FAILURE(write_nix_support_file("orig-libc", runtime.c_str()));
  ASSERT_NO_FATAL_FAILURE(write_nix_support_bytes(
    development + "\r\n" + second + "\n"));

  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.include_dir_count, (size_t)1);
  ASSERT_STREQ(nix.include_dirs[0], (development + "/include").c_str());
  ASSERT_EQ(nix.lib_dir_count, (size_t)1);
  ASSERT_STREQ(nix.lib_dirs[0], (runtime + "/lib").c_str());
}

TEST_F(NixenvTest, OrigLibcDevPreservesIncludeOrderAndCapacity)
{
  ASSERT_NO_FATAL_FAILURE(create_nix_support());
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  std::string development = std::string(tmpdir) + "/development";
  std::string include = development + "/include";
  ASSERT_TRUE(std::filesystem::create_directories(include));
  ASSERT_NO_FATAL_FAILURE(write_nix_support_bytes(development));

  for(size_t count = 0; count <= NIXENV_MAX_DIRS + 1; count++)
  {
    SCOPED_TRACE(count);
    std::string flags;
    std::vector<std::string> expected;
    for(size_t i = 0; i < count; i++)
    {
      std::string path = "/caller/" + std::to_string(i == 2 ? 1 : i);
      expected.push_back(path);
      flags += "-isystem " + path + " ";
    }
    setenv("NIX_CFLAGS_COMPILE", flags.c_str(), 1);

    nixenv_t nix;
    ASSERT_TRUE(nixenv_probe(&nix, strtab));
    ASSERT_EQ(nix.include_dir_count,
      std::min(count + 1, (size_t)NIXENV_MAX_DIRS));
    for(size_t i = 0; i < std::min(count, (size_t)NIXENV_MAX_DIRS); i++)
      ASSERT_STREQ(nix.include_dirs[i], expected[i].c_str());
    if(count < NIXENV_MAX_DIRS)
      ASSERT_STREQ(nix.include_dirs[count], include.c_str());
    ASSERT_EQ(nix.lib_dir_count, (size_t)0);
  }

  std::string duplicates = "-isystem /A -isystem " + include + " -isystem /B";
  setenv("NIX_CFLAGS_COMPILE", duplicates.c_str(), 1);
  nixenv_t nix;
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.include_dir_count, (size_t)4);
  ASSERT_STREQ(nix.include_dirs[0], "/A");
  ASSERT_STREQ(nix.include_dirs[1], include.c_str());
  ASSERT_STREQ(nix.include_dirs[2], "/B");
  ASSERT_STREQ(nix.include_dirs[3], include.c_str());

  std::string libraries;
  for(size_t i = 0; i < NIXENV_MAX_DIRS; i++)
    libraries += "-L/library/" + std::to_string(i) + " ";
  setenv("NIX_LDFLAGS", libraries.c_str(), 1);
  setenv("NIX_CFLAGS_COMPILE", "-isystem /caller", 1);
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.include_dir_count, (size_t)2);
  ASSERT_STREQ(nix.include_dirs[0], "/caller");
  ASSERT_STREQ(nix.include_dirs[1], include.c_str());
  ASSERT_EQ(nix.lib_dir_count, (size_t)NIXENV_MAX_DIRS);
  for(size_t i = 0; i < NIXENV_MAX_DIRS; i++)
    ASSERT_STREQ(nix.lib_dirs[i], ("/library/" + std::to_string(i)).c_str());

  std::string includes;
  for(size_t i = 0; i < NIXENV_MAX_DIRS; i++)
    includes += "-isystem /include/" + std::to_string(i) + " ";
  setenv("NIX_CFLAGS_COMPILE", includes.c_str(), 1);
  setenv("NIX_LDFLAGS", "-L/sentinel", 1);
  ASSERT_TRUE(nixenv_probe(&nix, strtab));
  ASSERT_EQ(nix.include_dir_count, (size_t)NIXENV_MAX_DIRS);
  for(size_t i = 0; i < NIXENV_MAX_DIRS; i++)
    ASSERT_STREQ(nix.include_dirs[i],
      ("/include/" + std::to_string(i)).c_str());
  ASSERT_EQ(nix.lib_dir_count, (size_t)1);
  ASSERT_STREQ(nix.lib_dirs[0], "/sentinel");
}

TEST_F(NixenvTest, OrigLibcDevUnavailableIsOptional)
{
  ASSERT_NO_FATAL_FAILURE(create_nix_support());
  setenv("NIX_CC", tmpdir, 1);
  setenv("NIX_CFLAGS_COMPILE", "-isystem /first -isystem /second", 1);
  setenv("NIX_LDFLAGS", "-L/library", 1);
  ASSERT_NO_FATAL_FAILURE(create_dynamic_linker("/sentinel/ld.so"));
  std::string runtime = std::string(tmpdir) + "/runtime";
  ASSERT_TRUE(std::filesystem::create_directories(runtime + "/include"));
  ASSERT_TRUE(std::filesystem::create_directories(runtime + "/lib"));
  ASSERT_NO_FATAL_FAILURE(write_nix_support_file("orig-libc", runtime.c_str()));
  ASSERT_NO_FATAL_FAILURE(write_nix_support_file("cc-ldflags", "-L/compiler"));

  std::string metadata = std::string(tmpdir) + "/nix-support/orig-libc-dev";
  std::string development = std::string(tmpdir) + "/development";
  ASSERT_TRUE(std::filesystem::create_directory(development));
  for(const char* shape : {"missing", "zero", "empty-line", "unreadable",
    "missing-directory", "regular-file"})
  {
    SCOPED_TRACE(shape);
    std::error_code error;
    std::filesystem::remove(metadata, error);
    ASSERT_FALSE(error) << error.message();
    if(strcmp(shape, "zero") == 0)
      ASSERT_NO_FATAL_FAILURE(write_nix_support_bytes(""));
    else if(strcmp(shape, "empty-line") == 0)
      ASSERT_NO_FATAL_FAILURE(write_nix_support_bytes("\n" + runtime));
    else if(strcmp(shape, "unreadable") == 0)
    {
      std::filesystem::create_symlink("orig-libc-dev", metadata, error);
      ASSERT_FALSE(error) << error.message();
    }
    else if(strcmp(shape, "missing-directory") == 0)
      ASSERT_NO_FATAL_FAILURE(write_nix_support_bytes(development));
    else if(strcmp(shape, "regular-file") == 0)
    {
      FILE* f = fopen((development + "/include").c_str(), "wb");
      ASSERT_NE(f, (FILE*)NULL);
      ASSERT_EQ(fclose(f), 0);
      ASSERT_NO_FATAL_FAILURE(write_nix_support_bytes(development));
    }

    nixenv_t nix;
    ASSERT_TRUE(nixenv_probe(&nix, strtab));
    ASSERT_EQ(nix.include_dir_count, (size_t)2);
    ASSERT_STREQ(nix.include_dirs[0], "/first");
    ASSERT_STREQ(nix.include_dirs[1], "/second");
    ASSERT_STREQ(nix.dynamic_linker, "/sentinel/ld.so");
    ASSERT_EQ(nix.lib_dir_count, (size_t)3);
    ASSERT_STREQ(nix.lib_dirs[0], "/library");
    ASSERT_STREQ(nix.lib_dirs[1], (runtime + "/lib").c_str());
    ASSERT_STREQ(nix.lib_dirs[2], "/compiler");
  }
}

TEST_F(NixenvTest, OrigLibcDevIncludePathLengthBoundary)
{
  ASSERT_NO_FATAL_FAILURE(create_nix_support());
  setenv("NIX_CC", tmpdir, 1);
  unsetenv("NIX_LDFLAGS");
  setenv("NIX_CFLAGS_COMPILE", "-isystem /caller", 1);
  std::string development = std::string(tmpdir) + "/development";
  ASSERT_TRUE(std::filesystem::create_directories(development + "/include"));
  ASSERT_TRUE(std::filesystem::create_directories(development + "/includ"));
  ASSERT_LT(development.size(), (size_t)PATH_MAX - 9);

  for(size_t length : {(size_t)PATH_MAX - 9, (size_t)PATH_MAX - 8})
  {
    SCOPED_TRACE(length);
    std::string prefix(length - development.size(), '/');
    prefix += development;
    ASSERT_EQ(prefix.size(), length);
    std::string expected = prefix + "/include";
    std::string truncated = expected.substr(0, PATH_MAX - 1);
    ASSERT_TRUE(std::filesystem::is_directory(truncated));
    ASSERT_NO_FATAL_FAILURE(write_nix_support_bytes(prefix));

    nixenv_t nix;
    ASSERT_TRUE(nixenv_probe(&nix, strtab));
    ASSERT_EQ(nix.include_dir_count,
      length == PATH_MAX - 9 ? (size_t)2 : (size_t)1);
    ASSERT_STREQ(nix.include_dirs[0], "/caller");
    if(length == PATH_MAX - 9)
      ASSERT_STREQ(nix.include_dirs[1], expected.c_str());
    ASSERT_EQ(nix.lib_dir_count, (size_t)0);
  }
}

#endif // PLATFORM_IS_POSIX_BASED
