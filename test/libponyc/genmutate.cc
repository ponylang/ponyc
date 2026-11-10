#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/codegen.h>
#include <codegen/genmutate.h>
#include <../libponyrt/mem/pool.h>

#include "util.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <set>

#include "llvm_config_begin.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include "llvm_config_end.h"

#ifdef PLATFORM_IS_WINDOWS
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif


static std::string make_temp_dir()
{
#ifdef PLATFORM_IS_WINDOWS
  char tmp_path[MAX_PATH];
  GetTempPathA(MAX_PATH, tmp_path);
  char dir_path[MAX_PATH];
  GetTempFileNameA(tmp_path, "gmt", 0, dir_path);
  // GetTempFileName creates a file; remove it so we can make a directory
  DeleteFileA(dir_path);
  _mkdir(dir_path);
  return std::string(dir_path);
#else
  char tmpdir[] = "/tmp/genmutate_test_XXXXXX";
  mkdtemp(tmpdir);
  return std::string(tmpdir);
#endif
}


static void remove_temp_dir(const std::string& dir)
{
#ifdef PLATFORM_IS_WINDOWS
  _rmdir(dir.c_str());
#else
  rmdir(dir.c_str());
#endif
}


static std::string make_path(const std::string& dir, const std::string& file)
{
#ifdef PLATFORM_IS_WINDOWS
  return dir + "\\" + file;
#else
  return dir + "/" + file;
#endif
}


class GenMutateTest : public testing::Test
{
protected:
  mutate_state_t state;

  void SetUp() override
  {
    memset(&state, 0, sizeof(state));
  }

  void TearDown() override
  {
    mutate_cleanup(&state);
  }
};


TEST_F(GenMutateTest, InitSetsFields)
{
  mutate_init(&state);

  EXPECT_NE(state.mutants, nullptr);
  EXPECT_EQ(state.mutant_count, 0u);
  EXPECT_EQ(state.mutant_alloc, 64u);
  EXPECT_EQ(state.next_id, 1);
}


TEST_F(GenMutateTest, CleanupOnZeroInitIsNoop)
{
  // state is memset to zero, cleanup should not crash
  mutate_cleanup(&state);
  EXPECT_EQ(state.mutants, nullptr);
  EXPECT_EQ(state.mutant_count, 0u);
}


TEST_F(GenMutateTest, NextIdIncrementsFromOne)
{
  mutate_init(&state);

  EXPECT_EQ(mutate_next_id(&state), 1);
  EXPECT_EQ(mutate_next_id(&state), 2);
  EXPECT_EQ(mutate_next_id(&state), 3);
}


TEST_F(GenMutateTest, RecordStoresFields)
{
  mutate_init(&state);

  mutate_record(&state, 1, "test.pony", 10, 5,
    "arithmetic", "+", "-", 0);

  EXPECT_EQ(state.mutant_count, 1u);
  EXPECT_EQ(state.mutants[0].id, 1);
  EXPECT_EQ(state.mutants[0].coverage_counter, 0);
  EXPECT_STREQ(state.mutants[0].file, "test.pony");
  EXPECT_EQ(state.mutants[0].line, 10u);
  EXPECT_EQ(state.mutants[0].column, 5u);
  EXPECT_STREQ(state.mutants[0].category, "arithmetic");
  EXPECT_STREQ(state.mutants[0].original, "+");
  EXPECT_STREQ(state.mutants[0].replacement, "-");
}


TEST_F(GenMutateTest, RecordGrowsPastInitialAlloc)
{
  mutate_init(&state);

  for(int32_t i = 1; i <= 65; i++)
    mutate_record(&state, i, "test.pony", (size_t)i, 1,
      "cat", "a", "b", i - 1);

  EXPECT_EQ(state.mutant_count, 65u);
  EXPECT_GE(state.mutant_alloc, 65u);

  // Verify first and last entries survived the reallocation
  EXPECT_EQ(state.mutants[0].id, 1);
  EXPECT_EQ(state.mutants[0].line, 1u);
  EXPECT_EQ(state.mutants[64].id, 65);
  EXPECT_EQ(state.mutants[64].line, 65u);
}


TEST_F(GenMutateTest, WriteInfoEmptyMutants)
{
  mutate_init(&state);

  std::string tmpdir = make_temp_dir();
  ASSERT_FALSE(tmpdir.empty());

  EXPECT_TRUE(mutate_write_info(&state, tmpdir.c_str(), "testbin"));

  std::string path = make_path(tmpdir, "testbin.mutants.json");
  std::ifstream f(path);
  ASSERT_TRUE(f.is_open());

  std::stringstream buf;
  buf << f.rdbuf();
  std::string json = buf.str();

  EXPECT_NE(json.find("\"version\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"binary\": \"testbin\""), std::string::npos);
  EXPECT_NE(json.find("\"total_mutants\": 0"), std::string::npos);
  EXPECT_NE(json.find("\"total_coverage_counters\": 0"), std::string::npos);
  EXPECT_NE(json.find("\"mutants\": []"), std::string::npos);

  remove(path.c_str());
  remove_temp_dir(tmpdir);
}


TEST_F(GenMutateTest, WriteInfoEscapesSpecialChars)
{
  mutate_init(&state);

  mutate_record(&state, 1,
    "path\\with\"special\nchars", 1, 1,
    "cat\twith\ttabs", "a\rb", "c\\d", 0);

  std::string tmpdir = make_temp_dir();
  ASSERT_FALSE(tmpdir.empty());

  EXPECT_TRUE(mutate_write_info(&state, tmpdir.c_str(), "test"));

  std::string path = make_path(tmpdir, "test.mutants.json");
  std::ifstream f(path);
  ASSERT_TRUE(f.is_open());

  std::stringstream buf;
  buf << f.rdbuf();
  std::string json = buf.str();

  // Backslash escaped
  EXPECT_NE(json.find("path\\\\with"), std::string::npos);
  // Quote escaped
  EXPECT_NE(json.find("\\\"special"), std::string::npos);
  // Newline escaped
  EXPECT_NE(json.find("\\n"), std::string::npos);
  // Tab escaped
  EXPECT_NE(json.find("\\t"), std::string::npos);
  // Carriage return escaped
  EXPECT_NE(json.find("\\r"), std::string::npos);

  remove(path.c_str());
  remove_temp_dir(tmpdir);
}


TEST_F(GenMutateTest, WriteInfoWithMutantRecords)
{
  mutate_init(&state);

  mutate_record(&state, 1, "foo.pony", 10, 3,
    "arithmetic", "+", "-", 0);
  mutate_record(&state, 2, "bar.pony", 20, 7,
    "conditional", "true", "false", 1);

  std::string tmpdir = make_temp_dir();
  ASSERT_FALSE(tmpdir.empty());

  EXPECT_TRUE(mutate_write_info(&state, tmpdir.c_str(), "mybin"));

  std::string path = make_path(tmpdir, "mybin.mutants.json");
  std::ifstream f(path);
  ASSERT_TRUE(f.is_open());

  std::stringstream buf;
  buf << f.rdbuf();
  std::string json = buf.str();

  EXPECT_NE(json.find("\"total_mutants\": 2"), std::string::npos);
  EXPECT_NE(json.find("\"total_coverage_counters\": 0"), std::string::npos);
  EXPECT_NE(json.find("\"id\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"id\": 2"), std::string::npos);
  EXPECT_NE(json.find("\"file\": \"foo.pony\""), std::string::npos);
  EXPECT_NE(json.find("\"category\": \"arithmetic\""), std::string::npos);
  EXPECT_NE(json.find("\"coverage_counter\": 0"), std::string::npos);
  EXPECT_NE(json.find("\"coverage_counter\": 1"), std::string::npos);

  remove(path.c_str());
  remove_temp_dir(tmpdir);
}


TEST_F(GenMutateTest, WriteInfoHandlesLongStrings)
{
  mutate_init(&state);

  std::string long_str(1500, 'x');
  mutate_record(&state, 1, long_str.c_str(), 1, 1,
    "cat", "a", "b", 0);

  std::string tmpdir = make_temp_dir();
  ASSERT_FALSE(tmpdir.empty());

  EXPECT_TRUE(mutate_write_info(&state, tmpdir.c_str(), "test"));

  std::string path = make_path(tmpdir, "test.mutants.json");
  std::ifstream f(path);
  ASSERT_TRUE(f.is_open());

  std::stringstream buf;
  buf << f.rdbuf();
  std::string json = buf.str();

  // The full 1500-char string must appear untruncated
  EXPECT_NE(json.find(long_str), std::string::npos);

  remove(path.c_str());
  remove_temp_dir(tmpdir);
}


TEST_F(GenMutateTest, FindOrRecordAllocatesNewId)
{
  mutate_init(&state);

  int32_t out_cov;
  int32_t id = mutate_find_or_record(&state,
    "test.pony", 10, 5, "aor", "+", "-", 0, &out_cov);

  EXPECT_EQ(id, 1);
  EXPECT_EQ(out_cov, 0);
  EXPECT_EQ(state.mutant_count, 1u);
  EXPECT_EQ(state.mutants[0].coverage_counter, 0);
}


TEST_F(GenMutateTest, FindOrRecordReturnsExistingCoverage)
{
  mutate_init(&state);

  int32_t out_cov1;
  mutate_find_or_record(&state,
    "test.pony", 10, 5, "aor", "+", "-", 42, &out_cov1);
  EXPECT_EQ(out_cov1, 42);

  int32_t out_cov2;
  int32_t id2 = mutate_find_or_record(&state,
    "test.pony", 10, 5, "aor", "+", "-", 99, &out_cov2);
  EXPECT_EQ(id2, 1);
  EXPECT_EQ(out_cov2, 42);
  EXPECT_EQ(state.mutant_count, 1u);
}


class GenMutateCodegenTest : public PassTest
{
protected:
  bool has_mutant(const char* category, const char* original,
    const char* replacement)
  {
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      if(strcmp(r->category, category) == 0 &&
        strcmp(r->original, original) == 0 &&
        strcmp(r->replacement, replacement) == 0)
        return true;
    }
    return false;
  }

  bool has_mutant_at(size_t line, size_t col, const char* category,
    const char* original, const char* replacement)
  {
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      if(r->line == line && r->column == col &&
        strcmp(r->category, category) == 0 &&
        strcmp(r->original, original) == 0 &&
        strcmp(r->replacement, replacement) == 0)
        return true;
    }
    return false;
  }

  bool has_mutant_at_line(const char* file, size_t line,
    const char* category, const char* original, const char* replacement)
  {
    size_t file_len = strlen(file);
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      size_t rlen = strlen(r->file);
      bool file_ok = (file_len == 0)
        ? (rlen == 0)
        : ((rlen >= file_len)
           && (strcmp(r->file + rlen - file_len, file) == 0));
      if(file_ok && r->line == line &&
        strcmp(r->category, category) == 0 &&
        strcmp(r->original, original) == 0 &&
        strcmp(r->replacement, replacement) == 0)
        return true;
    }
    return false;
  }

  size_t count_category_at(size_t line, const char* category)
  {
    size_t count = 0;
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      if(r->line == line && strcmp(r->category, category) == 0)
        count++;
    }
    return count;
  }

  size_t count_category_at_file(const char* file, size_t line,
    const char* category)
  {
    size_t count = 0;
    size_t flen = strlen(file);
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      if(r->line != line || strcmp(r->category, category) != 0)
        continue;
      if(flen == 0)
      {
        if(strlen(r->file) == 0)
          count++;
      }
      else
      {
        size_t rlen = strlen(r->file);
        if(rlen >= flen
          && strcmp(r->file + rlen - flen, file) == 0)
          count++;
      }
    }
    return count;
  }

  size_t count_category_at_col(size_t line, size_t col,
    const char* category)
  {
    size_t count = 0;
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      if(r->line == line && r->column == col
        && strcmp(r->category, category) == 0)
        count++;
    }
    return count;
  }

  int32_t coverage_counter_at(size_t line, size_t col,
    const char* category)
  {
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      if(r->line == line && r->column == col
        && strcmp(r->category, category) == 0)
        return r->coverage_counter;
    }
    return -1;
  }

  struct mutant_key
  {
    std::string file;
    size_t line;
    size_t column;
    std::string category;
    std::string original;
    std::string replacement;

    bool operator<(const mutant_key& o) const
    {
      if(file != o.file) return file < o.file;
      if(line != o.line) return line < o.line;
      if(column != o.column) return column < o.column;
      if(category != o.category) return category < o.category;
      if(original != o.original) return original < o.original;
      return replacement < o.replacement;
    }

    bool operator==(const mutant_key& o) const
    {
      return file == o.file && line == o.line && column == o.column &&
        category == o.category && original == o.original &&
        replacement == o.replacement;
    }
  };

  std::set<mutant_key> collect_category(const char* category)
  {
    std::set<mutant_key> result;
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      if(strcmp(r->category, category) == 0)
      {
        result.insert({r->file, r->line, r->column,
          r->category, r->original, r->replacement});
      }
    }
    return result;
  }

  std::set<mutant_key> collect_all()
  {
    std::set<mutant_key> result;
    for(size_t i = 0; i < compile->mutate.mutant_count; i++)
    {
      mutant_record_t* r = &compile->mutate.mutants[i];
      result.insert({r->file, r->line, r->column,
        r->category, r->original, r->replacement});
    }
    return result;
  }

  void verify_module()
  {
    char* msg = NULL;
    LLVMBool failed = LLVMVerifyModule(compile->module,
      LLVMReturnStatusAction, &msg);
    if(failed)
    {
      std::string err(msg ? msg : "(no message)");
      LLVMDisposeMessage(msg);
      FAIL() << "LLVMVerifyModule failed:\n" << err;
    }
    if(msg)
      LLVMDisposeMessage(msg);
  }
};


TEST_F(GenMutateCodegenTest, AddEmitsSwitchPhi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t switch_count = 0;
  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(llvm::isa<llvm::SwitchInst>(&inst))
          switch_count++;

        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("mut.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_GT(switch_count, 0u);
  ASSERT_GT(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, AddEmitsCoverageInc)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  bool has_coverage_load = false;
  bool has_atomicrmw = false;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* load = llvm::dyn_cast<llvm::LoadInst>(&inst))
        {
          if(load->getName().starts_with("cov.base"))
            has_coverage_load = true;
        }
        if(auto* rmw = llvm::dyn_cast<llvm::AtomicRMWInst>(&inst))
        {
          if(rmw->getOperation() == llvm::AtomicRMWInst::Add)
            has_atomicrmw = true;
        }
      }
    }
  }

  ASSERT_TRUE(has_coverage_load);
  ASSERT_TRUE(has_atomicrmw);

  ASSERT_GT(compile->mutate.next_coverage_id, 0);
  EXPECT_EQ(compile->mutate.mutants[0].coverage_counter, 0);
}


TEST_F(GenMutateCodegenTest, AddRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("aor", "add", "mul"));
}


TEST_F(GenMutateCodegenTest, SubRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(10) - U64(3)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "sub", "add"));
  ASSERT_TRUE(has_mutant("aor", "sub", "mul"));
}


TEST_F(GenMutateCodegenTest, MulRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y * z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "mul", "add"));
  ASSERT_TRUE(has_mutant("aor", "mul", "sub"));
}


TEST_F(GenMutateCodegenTest, AndRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y and z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("bor", "and", "or"));
  ASSERT_TRUE(has_mutant("bor", "and", "xor"));
}


TEST_F(GenMutateCodegenTest, OrRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y or z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("bor", "or", "and"));
  ASSERT_TRUE(has_mutant("bor", "or", "xor"));
}


TEST_F(GenMutateCodegenTest, XorRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y xor z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("bor", "xor", "and"));
  ASSERT_TRUE(has_mutant("bor", "xor", "or"));
}


TEST_F(GenMutateCodegenTest, EqRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y == z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "eq", "ne"));
  ASSERT_TRUE(has_mutant("ror", "eq", "lt"));
  ASSERT_TRUE(has_mutant("ror", "eq", "le"));
  ASSERT_TRUE(has_mutant("ror", "eq", "ge"));
  ASSERT_TRUE(has_mutant("ror", "eq", "gt"));
}


TEST_F(GenMutateCodegenTest, NeRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y != z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "ne", "eq"));
  ASSERT_TRUE(has_mutant("ror", "ne", "lt"));
  ASSERT_TRUE(has_mutant("ror", "ne", "le"));
  ASSERT_TRUE(has_mutant("ror", "ne", "ge"));
  ASSERT_TRUE(has_mutant("ror", "ne", "gt"));
}


TEST_F(GenMutateCodegenTest, LtRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y < z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "lt", "le"));
  ASSERT_TRUE(has_mutant("ror", "lt", "gt"));
  ASSERT_TRUE(has_mutant("ror", "lt", "ge"));
  ASSERT_TRUE(has_mutant("ror", "lt", "eq"));
  ASSERT_TRUE(has_mutant("ror", "lt", "ne"));
}


TEST_F(GenMutateCodegenTest, LeRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y <= z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "le", "lt"));
  ASSERT_TRUE(has_mutant("ror", "le", "gt"));
  ASSERT_TRUE(has_mutant("ror", "le", "ge"));
  ASSERT_TRUE(has_mutant("ror", "le", "eq"));
  ASSERT_TRUE(has_mutant("ror", "le", "ne"));
}


TEST_F(GenMutateCodegenTest, GeRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y >= z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "ge", "gt"));
  ASSERT_TRUE(has_mutant("ror", "ge", "lt"));
  ASSERT_TRUE(has_mutant("ror", "ge", "le"));
  ASSERT_TRUE(has_mutant("ror", "ge", "eq"));
  ASSERT_TRUE(has_mutant("ror", "ge", "ne"));
}


TEST_F(GenMutateCodegenTest, GtRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y > z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "gt", "ge"));
  ASSERT_TRUE(has_mutant("ror", "gt", "lt"));
  ASSERT_TRUE(has_mutant("ror", "gt", "le"));
  ASSERT_TRUE(has_mutant("ror", "gt", "eq"));
  ASSERT_TRUE(has_mutant("ror", "gt", "ne"));
}


TEST_F(GenMutateCodegenTest, DivRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y / z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "div", "rem"));
}


TEST_F(GenMutateCodegenTest, RemRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y % z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "rem", "div"));
}


TEST_F(GenMutateCodegenTest, ShlRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 2\n"
    "    let x = y << z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("sor", "shl", "shr"));
}


TEST_F(GenMutateCodegenTest, ShrRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 12\n"
    "    let z: U64 = 2\n"
    "    let x = y >> z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("sor", "shr", "shl"));
}


TEST_F(GenMutateCodegenTest, NegRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 5\n"
    "    let x = -y\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("uoi", "neg", "identity"));
}


TEST_F(GenMutateCodegenTest, NotRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 5\n"
    "    let x = not y\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("uoi", "not", "identity"));
}


TEST_F(GenMutateCodegenTest, ConstantShlRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) << U64(2)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("sor", "shl", "shr"));
}


TEST_F(GenMutateCodegenTest, ConstantNegRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: I64 = -I64(5)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("uoi", "neg", "identity"));
}


TEST_F(GenMutateCodegenTest, FpAddRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: F64 = 3.0\n"
    "    let z: F64 = 5.0\n"
    "    let x = y + z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("aor", "add", "mul"));
}


TEST_F(GenMutateCodegenTest, FpEqRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: F64 = 3.0\n"
    "    let z: F64 = 5.0\n"
    "    let x = y == z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "eq", "ne"));
  ASSERT_TRUE(has_mutant("ror", "eq", "lt"));
}


TEST_F(GenMutateCodegenTest, SignedAddRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 3\n"
    "    let z: I64 = 5\n"
    "    let x = y + z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("aor", "add", "mul"));
}


TEST_F(GenMutateCodegenTest, SignedEqRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 3\n"
    "    let z: I64 = 5\n"
    "    let x = y == z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "eq", "ne"));
}


TEST_F(GenMutateCodegenTest, MatchEqRecordsMutant)
{
  const char* src =
    "actor Main\n"                     // line 1
    "  new create(env: Env) =>\n"      // line 2
    "    let y: U64 = 5\n"             // line 3
    "    match y\n"                     // line 4
    "    | 3 => None\n"                // line 5, pattern at col 7
    "    | 5 => None\n"                // line 6, pattern at col 7
    "    end\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  // ROR at pattern | 3
  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "ne"));
  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "lt"));
  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "le"));
  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "ge"));
  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "gt"));

  // CR at pattern | 3
  ASSERT_TRUE(has_mutant_at(5, 7, "cr", "lhs", "n->n+1"));
  ASSERT_TRUE(has_mutant_at(5, 7, "cr", "lhs", "n->n-1"));

  // ROR at pattern | 5
  ASSERT_TRUE(has_mutant_at(6, 7, "ror", "eq", "ne"));
  ASSERT_TRUE(has_mutant_at(6, 7, "ror", "eq", "lt"));
  ASSERT_TRUE(has_mutant_at(6, 7, "ror", "eq", "le"));
  ASSERT_TRUE(has_mutant_at(6, 7, "ror", "eq", "ge"));
  ASSERT_TRUE(has_mutant_at(6, 7, "ror", "eq", "gt"));

  // CR at pattern | 5
  ASSERT_TRUE(has_mutant_at(6, 7, "cr", "lhs", "n->n+1"));
  ASSERT_TRUE(has_mutant_at(6, 7, "cr", "lhs", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, MatchPatternRorOnly)
{
  const char* src =
    "actor Main\n"                     // line 1
    "  new create(env: Env) =>\n"      // line 2
    "    let y: U64 = 5\n"             // line 3
    "    match y\n"                     // line 4
    "    | 3 => None\n"                // line 5, pattern at col 7
    "    end\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ROR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "ne"));
  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "gt"));
  ASSERT_EQ(count_category_at(5, "cr"), 0u);
}


TEST_F(GenMutateCodegenTest, MatchPatternCrBoundary)
{
  const char* src =
    "actor Main\n"                     // line 1
    "  new create(env: Env) =>\n"      // line 2
    "    let y: U64 = 5\n"             // line 3
    "    match y\n"                     // line 4
    "    | 0 => None\n"                // line 5, CR: 0->1
    "    | 1 => None\n"                // line 6, CR: 1->0
    "    end\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant_at(5, 7, "cr", "lhs", "0->1"));
  ASSERT_TRUE(has_mutant_at(6, 7, "cr", "lhs", "1->0"));
}


TEST_F(GenMutateCodegenTest, MatchPatternFloat)
{
  const char* src =
    "actor Main\n"                     // line 1
    "  new create(env: Env) =>\n"      // line 2
    "    let y: F64 = F64(1.0)\n"      // line 3
    "    match y\n"                     // line 4
    "    | 1.5 => None\n"              // line 5, pattern at col 7
    "    end\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  // ROR on the equality comparison
  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "ne"));

  // CR on the float literal: 1.5 -> -1.5
  ASSERT_TRUE(has_mutant_at(5, 7, "cr", "lhs", "x->-x"));
}


TEST_F(GenMutateCodegenTest, MatchPatternBool)
{
  const char* src =
    "actor Main\n"                     // line 1
    "  new create(env: Env) =>\n"      // line 2
    "    let y: Bool = true\n"         // line 3
    "    match y\n"                     // line 4
    "    | true => None\n"             // line 5, pattern at col 7
    "    end\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  // ROR on the equality comparison
  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "ne"));

  // No CR — Bool patterns are TK_TRUE/TK_FALSE, not TK_INT/TK_FLOAT
  ASSERT_EQ(count_category_at(5, "cr"), 0u);
}


TEST_F(GenMutateCodegenTest, MatchPatternUnion)
{
  const char* src =
    "actor Main\n"                     // line 1
    "  new create(env: Env) =>\n"      // line 2
    "    let y: (U64 | None) = U64(4)\n"  // line 3
    "    match y\n"                     // line 4
    "    | 4 => None\n"                // line 5, pattern at col 7
    "    end\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant_at(5, 7, "ror", "eq", "ne"));
  ASSERT_TRUE(has_mutant_at(5, 7, "cr", "lhs", "n->n+1"));
  ASSERT_TRUE(has_mutant_at(5, 7, "cr", "lhs", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, MatchPatternCrOnly)
{
  const char* src =
    "actor Main\n"                     // line 1
    "  new create(env: Env) =>\n"      // line 2
    "    let y: U64 = 5\n"             // line 3
    "    match y\n"                     // line 4
    "    | 3 => None\n"                // line 5, pattern at col 7
    "    end\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(5, "cr"), 0u);
  ASSERT_EQ(count_category_at(5, "ror"), 0u);
}


TEST_F(GenMutateCodegenTest, MatchPatternCategoryIndependence)
{
  const char* sources[] = {
    // int pattern
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 5\n"
    "    match y\n"
    "    | 3 => None\n"
    "    end\n",
    // float pattern
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: F64 = 5.0\n"
    "    match y\n"
    "    | 1.5 => None\n"
    "    end\n",
    // bool pattern
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: Bool = true\n"
    "    match y\n"
    "    | true => None\n"
    "    end\n",
    // union arm
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: (U64 | None) = U64(4)\n"
    "    match y\n"
    "    | 4 => None\n"
    "    end\n",
    // alternatives
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 5\n"
    "    match y\n"
    "    | 3 | 7 => None\n"
    "    end\n",
    // char (U8) pattern
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U8 = 'a'\n"
    "    match y\n"
    "    | 'b' => None\n"
    "    end\n",
    // tuple element
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: (U64, U64) = (3, 4)\n"
    "    match y\n"
    "    | (1, 2) => None\n"
    "    end\n"
  };
  size_t src_count = sizeof(sources) / sizeof(sources[0]);

  const char* categories[] = {"ror", "cr"};
  uint32_t cat_flags[] = {MUTATE_OP_ROR, MUTATE_OP_CR};

  for(size_t si = 0; si < src_count; si++)
  {
    for(size_t ci = 0; ci < 2; ci++)
    {
      opt.mutate = true;
      opt.mutate_operators = cat_flags[ci];
      DO(test_compile(sources[si], "ir"));
      auto alone = collect_category(categories[ci]);

      if(ci == 0)
        ASSERT_GT(count_category_at(5, "ror"), 0u)
          << "Shape " << si << " produced no ror mutants at pattern line";

      opt.mutate_operators = cat_flags[ci] | MUTATE_OP_CR;
      DO(test_compile(sources[si], "ir"));
      auto with_cr = collect_category(categories[ci]);

      opt.mutate_operators = MUTATE_OP_ALL;
      DO(test_compile(sources[si], "ir"));
      auto with_all = collect_category(categories[ci]);

      ASSERT_EQ(alone, with_cr)
        << "Shape " << si << ", category " << categories[ci]
        << " differs alone vs alone+cr";
      ASSERT_EQ(alone, with_all)
        << "Shape " << si << ", category " << categories[ci]
        << " differs alone vs all";
    }
  }
}


TEST_F(GenMutateCodegenTest, CmpEmitsSwitchPhi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 5\n"
    "    let z: U64 = 3\n"
    "    let x = y == z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("mut.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_GT(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, DivEmitsSwitchPhi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y / z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("mut.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_GT(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, ShiftEmitsSwitchPhi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 2\n"
    "    let x = y << z\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("mut.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_GT(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, UnaryEmitsSwitchPhi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 5\n"
    "    let x = -y\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("mut.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_GT(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, NoMutationWithoutFlag)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("mut.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_EQ(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, OperatorFilterAorOnly)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n"
    "    let b = a == U64(8)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("aor", "add", "mul"));
  ASSERT_FALSE(has_mutant("ror", "eq", "ne"));
}


TEST_F(GenMutateCodegenTest, OperatorFilterRorOnly)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n"
    "    let b = a == U64(8)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ROR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "eq", "ne"));
  ASSERT_FALSE(has_mutant("aor", "add", "sub"));
}


TEST_F(GenMutateCodegenTest, OperatorFilterMultiple)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n"
    "    let b = a == U64(8)\n"
    "    let c = a and U64(7)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_BOR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("bor", "and", "or"));
  ASSERT_FALSE(has_mutant("ror", "eq", "ne"));
}


TEST_F(GenMutateCodegenTest, AllOperatorsDefault)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n"
    "    let b = a == U64(8)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("ror", "eq", "ne"));
}


TEST_F(GenMutateCodegenTest, ScopeFilterMatchingFile)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n";

  opt.mutate = true;

  size_t alloc_size = sizeof(mutate_scope_t);
  mutate_scope_t* scopes =
    (mutate_scope_t*)ponyint_pool_alloc_size(alloc_size);
  char* file = (char*)ponyint_pool_alloc_size(1);
  file[0] = '\0';
  scopes[0].file = file;
  scopes[0].start_line = 0;
  scopes[0].end_line = 0;
  opt.mutate_scopes = scopes;
  opt.mutate_scope_count = 1;

  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
}


TEST_F(GenMutateCodegenTest, ScopeFilterNonMatchingFile)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n";

  opt.mutate = true;

  size_t alloc_size = sizeof(mutate_scope_t);
  mutate_scope_t* scopes =
    (mutate_scope_t*)ponyint_pool_alloc_size(alloc_size);
  const char* name = "nonexistent.pony";
  size_t name_len = strlen(name) + 1;
  char* file = (char*)ponyint_pool_alloc_size(name_len);
  memcpy(file, name, name_len);
  scopes[0].file = file;
  scopes[0].start_line = 0;
  scopes[0].end_line = 0;
  opt.mutate_scopes = scopes;
  opt.mutate_scope_count = 1;

  DO(test_compile(src, "ir"));

  ASSERT_EQ(compile->mutate.mutant_count, 0u);
}


TEST_F(GenMutateCodegenTest, ScopeFilterLineRange)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n"
    "    let b = U64(1) - U64(2)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;

  size_t alloc_size = sizeof(mutate_scope_t);
  mutate_scope_t* scopes =
    (mutate_scope_t*)ponyint_pool_alloc_size(alloc_size);
  char* file = (char*)ponyint_pool_alloc_size(1);
  file[0] = '\0';
  scopes[0].file = file;
  scopes[0].start_line = 3;
  scopes[0].end_line = 3;
  opt.mutate_scopes = scopes;
  opt.mutate_scope_count = 1;

  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_FALSE(has_mutant("aor", "sub", "add"));
}


TEST_F(GenMutateCodegenTest, ScopeFilterNoScopeMeansAll)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n"
    "    let b = U64(1) - U64(2)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;

  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("aor", "sub", "add"));
}


TEST_F(GenMutateCodegenTest, NomutateOnFunctionSuppressesMutation)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo.mutated()\n"
    "    Foo.suppressed()\n"
    "\n"
    "primitive Foo\n"
    "  fun mutated(): U64 =>\n"
    "    U64(3) + U64(5)\n"
    "\n"
    "  fun \\nomutate\\ suppressed(): U64 =>\n"
    "    U64(1) + U64(2)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "add", "sub"));

  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    EXPECT_NE(r->line, 11u)
      << "mutant recorded on \\nomutate\\ function line " << r->line;
  }
}


TEST_F(GenMutateCodegenTest, NomutateOnClassSuppressesAllMethods)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n"
    "    Foo.go()\n"
    "\n"
    "primitive \\nomutate\\ Foo\n"
    "  fun go(): U64 =>\n"
    "    U64(1) + U64(2)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);

  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    EXPECT_NE(r->line, 8u)
      << "mutant recorded inside \\nomutate\\ primitive line " << r->line;
  }
}


TEST_F(GenMutateCodegenTest, CoverageCounterSharedAcrossReplacements)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y + z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  ASSERT_GE(compile->mutate.mutant_count, 2u);

  // Filter to line 5 — other lines may have different counters from
  // monomorphized stdlib.
  int32_t first_cov = -1;
  size_t line5_count = 0;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 5)
    {
      if(first_cov == -1)
        first_cov = r->coverage_counter;
      else
        EXPECT_EQ(r->coverage_counter, first_cov)
          << "line 5 mutant " << i << " has different coverage counter";
      line5_count++;
    }
  }
  ASSERT_GE(line5_count, 2u);
}


TEST_F(GenMutateCodegenTest, CmpEmitsCoverageInc)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y == z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ROR;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  bool has_coverage_load = false;
  bool has_atomicrmw = false;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* load = llvm::dyn_cast<llvm::LoadInst>(&inst))
        {
          if(load->getName().starts_with("cov.base"))
            has_coverage_load = true;
        }
        if(auto* rmw = llvm::dyn_cast<llvm::AtomicRMWInst>(&inst))
        {
          if(rmw->getOperation() == llvm::AtomicRMWInst::Add)
            has_atomicrmw = true;
        }
      }
    }
  }

  ASSERT_TRUE(has_coverage_load);
  ASSERT_TRUE(has_atomicrmw);
  ASSERT_GT(compile->mutate.next_coverage_id, 0);
}


TEST_F(GenMutateCodegenTest, DivEmitsCoverageInc)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y / z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  bool has_atomicrmw = false;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* rmw = llvm::dyn_cast<llvm::AtomicRMWInst>(&inst))
        {
          if(rmw->getOperation() == llvm::AtomicRMWInst::Add)
            has_atomicrmw = true;
        }
      }
    }
  }

  ASSERT_TRUE(has_atomicrmw);
  ASSERT_GT(compile->mutate.next_coverage_id, 0);
}


TEST_F(GenMutateCodegenTest, UnaryNegEmitsCoverageInc)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 5\n"
    "    let x = -y\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  bool has_atomicrmw = false;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(auto* rmw = llvm::dyn_cast<llvm::AtomicRMWInst>(&inst))
        {
          if(rmw->getOperation() == llvm::AtomicRMWInst::Add)
            has_atomicrmw = true;
        }
      }
    }
  }

  ASSERT_TRUE(has_atomicrmw);
  ASSERT_GT(compile->mutate.next_coverage_id, 0);
}


TEST_F(GenMutateCodegenTest, CrIntegerRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3\n"
    "    let y = x + 5\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, CrZeroOneSwapRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3\n"
    "    let y = x + 1\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("cr", "rhs", "1->0"));
}


TEST_F(GenMutateCodegenTest, CrFilterExcludesOthers)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3\n"
    "    let y = x + 5\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_FALSE(has_mutant("aor", "add", "sub"));
}


TEST_F(GenMutateCodegenTest, CrEmitsSwitchPhi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3\n"
    "    let y = x + 5\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t switch_count = 0;
  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(llvm::isa<llvm::SwitchInst>(&inst))
          switch_count++;

        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("cr.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_GT(switch_count, 0u);
  ASSERT_GT(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, CrFiresAlongsideAor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3\n"
    "    let y = x + 5\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));
}


TEST_F(GenMutateCodegenTest, CrConstantLhs)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3\n"
    "    let y = 5 + x\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("cr", "lhs", "n->n+1"));
}


TEST_F(GenMutateCodegenTest, RvrIntegerRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = compute()\n"
    "\n"
    "  fun compute(): U64 =>\n"
    "    U64(42)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("rvr", "return", "0"));
  ASSERT_TRUE(has_mutant("rvr", "return", "1"));
}


TEST_F(GenMutateCodegenTest, RvrBoolRecordsMutant)
{
  // Body must not be a single literal — constant bodies are suppressed.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = is_ok(U64(1))\n"
    "\n"
    "  fun is_ok(n: U64): Bool =>\n"
    "    n > U64(0)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("rvr", "return", "true"));
  ASSERT_TRUE(has_mutant("rvr", "return", "false"));
}


TEST_F(GenMutateCodegenTest, RvrFilterExcludesOthers)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_FALSE(has_mutant("aor", "add", "sub"));
}


TEST_F(GenMutateCodegenTest, RvrEmitsSwitchPhi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = compute()\n"
    "\n"
    "  fun compute(): U64 =>\n"
    "    U64(42)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t switch_count = 0;
  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(llvm::isa<llvm::SwitchInst>(&inst))
          switch_count++;

        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("rvr.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_GT(switch_count, 0u);
  ASSERT_GT(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, VcrRecordsMutant)
{
  const char* src =
    "class Acc\n"
    "  var _n: U64 = 0\n"
    "\n"
    "  fun ref bump() =>\n"
    "    _n = _n + 1\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = Acc\n"
    "    a.bump()\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_VCR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("vcr", "call", "remove"));
}


TEST_F(GenMutateCodegenTest, VcrFilterExcludesOthers)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_VCR;
  DO(test_compile(src, "ir"));

  ASSERT_FALSE(has_mutant("aor", "add", "sub"));
}


TEST_F(GenMutateCodegenTest, VcrEmitsSwitchPhi)
{
  const char* src =
    "class Acc\n"
    "  var _n: U64 = 0\n"
    "\n"
    "  fun ref bump() =>\n"
    "    _n = _n + 1\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = Acc\n"
    "    a.bump()\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_VCR;
  DO(test_compile(src, "ir"));

  auto module = llvm::unwrap(compile->module);

  size_t switch_count = 0;
  size_t phi_count = 0;

  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        if(llvm::isa<llvm::SwitchInst>(&inst))
          switch_count++;

        if(auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst))
        {
          if(phi->getName().starts_with("vcr.result"))
            phi_count++;
        }
      }
    }
  }

  ASSERT_GT(switch_count, 0u);
  ASSERT_GT(phi_count, 0u);
}


TEST_F(GenMutateCodegenTest, UoiInsertOnAdd)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("uoi", "result", "neg"));
  ASSERT_TRUE(has_mutant("uoi", "result", "complement"));
}


TEST_F(GenMutateCodegenTest, UoiInsertOnFpAdd)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = F64(3.0) + F64(5.0)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("uoi", "result", "neg"));
  ASSERT_FALSE(has_mutant("uoi", "result", "complement"));
}


TEST_F(GenMutateCodegenTest, UoiInsertOnMul)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) * U64(5)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("uoi", "result", "neg"));
  ASSERT_TRUE(has_mutant("uoi", "result", "complement"));
}


TEST_F(GenMutateCodegenTest, UoiInsertNotEmittedWithoutAor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_FALSE(has_mutant("uoi", "result", "neg"));
  ASSERT_FALSE(has_mutant("aor", "add", "sub"));
}


TEST_F(GenMutateCodegenTest, RvrNoneOnUnion)
{
  // Return type is (String | None); the body decides at runtime which
  // branch to take. The pointer-typed return gets a None replacement
  // because the declared type admits None.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = maybe(true)\n"
    "\n"
    "  fun maybe(flag: Bool): (String | None) =>\n"
    "    if flag then \"hello\" else None end\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("rvr", "return", "None"));
}


TEST_F(GenMutateCodegenTest, CrFloatRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: F64 = 3.0\n"
    "    let y = x + 5.0\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("cr", "rhs", "x->-x"));
}


TEST_F(GenMutateCodegenTest, CrZeroToOneRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3\n"
    "    let y = x + 0\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("cr", "rhs", "0->1"));
}


TEST_F(GenMutateCodegenTest, RvrFloatRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = compute()\n"
    "\n"
    "  fun compute(): F64 =>\n"
    "    F64(42.0)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("rvr", "return", "0.0"));
  ASSERT_TRUE(has_mutant("rvr", "return", "1.0"));
}


TEST_F(GenMutateCodegenTest, CrDivRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 100\n"
    "    let y = x / 5\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, CrDivByOneRecordsMutant)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 100\n"
    "    let y = x / 1\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("cr", "rhs", "1->0"));
}


TEST_F(GenMutateCodegenTest, AmrAddWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "add", "add_unsafe"));
  ASSERT_FALSE(has_mutant("aor", "add", "sub"));
}


TEST_F(GenMutateCodegenTest, AmrAddUnsafeToWrapping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y.add_unsafe(z)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "add", "add_wrapping"));
  ASSERT_FALSE(has_mutant("aor", "add", "sub"));
}


TEST_F(GenMutateCodegenTest, AmrSubWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(10) - U64(3)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "sub", "sub_unsafe"));
  ASSERT_FALSE(has_mutant("aor", "sub", "add"));
}


TEST_F(GenMutateCodegenTest, AmrMulWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y * z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "mul", "mul_unsafe"));
  ASSERT_FALSE(has_mutant("aor", "mul", "add"));
}


TEST_F(GenMutateCodegenTest, AmrDivWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y / z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "div", "div_unsafe"));
  ASSERT_FALSE(has_mutant("aor", "div", "rem"));
}


TEST_F(GenMutateCodegenTest, AmrRemWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y % z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "rem", "rem_unsafe"));
  ASSERT_FALSE(has_mutant("aor", "rem", "div"));
}


TEST_F(GenMutateCodegenTest, AmrNegWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 5\n"
    "    let x = -y\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "neg", "neg_unsafe"));
  ASSERT_FALSE(has_mutant("uoi", "neg", "identity"));
}


TEST_F(GenMutateCodegenTest, AmrNegUnsafeToWrapping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 5\n"
    "    let x = y.neg_unsafe()\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "neg", "neg_wrapping"));
}


TEST_F(GenMutateCodegenTest, AmrShlWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 2\n"
    "    let x = y << z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "shl", "shl_unsafe"));
  ASSERT_FALSE(has_mutant("sor", "shl", "shr"));
}


TEST_F(GenMutateCodegenTest, AmrShrWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 12\n"
    "    let z: U64 = 2\n"
    "    let x = y >> z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "shr", "shr_unsafe"));
  ASSERT_FALSE(has_mutant("sor", "shr", "shl"));
}


TEST_F(GenMutateCodegenTest, AmrFilterExcludesOthers)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = U64(3) + U64(5)\n"
    "    let b = a == U64(8)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("amr", "add", "add_unsafe"));
  ASSERT_FALSE(has_mutant("aor", "add", "sub"));
  ASSERT_FALSE(has_mutant("ror", "eq", "ne"));
}


TEST_F(GenMutateCodegenTest, AmrAndAorTogether)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_TRUE(has_mutant("aor", "add", "mul"));
  ASSERT_TRUE(has_mutant("amr", "add", "add_unsafe"));
}


TEST_F(GenMutateCodegenTest, AmrFpAddWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: F64 = 3.0\n"
    "    let z: F64 = 5.0\n"
    "    let x = y + z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "add", "add_unsafe"));
}


TEST_F(GenMutateCodegenTest, AmrSignedAddWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 3\n"
    "    let z: I64 = 5\n"
    "    let x = y + z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "add", "add_unsafe"));
}


TEST_F(GenMutateCodegenTest, AmrSubUnsafeToWrapping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y.sub_unsafe(z)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "sub", "sub_wrapping"));
}


TEST_F(GenMutateCodegenTest, AmrMulUnsafeToWrapping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 5\n"
    "    let x = y.mul_unsafe(z)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "mul", "mul_wrapping"));
}


TEST_F(GenMutateCodegenTest, AmrDivUnsafeToWrapping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y.div_unsafe(z)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "div", "div_wrapping"));
}


TEST_F(GenMutateCodegenTest, AmrRemUnsafeToWrapping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let z: U64 = 3\n"
    "    let x = y.rem_unsafe(z)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "rem", "rem_wrapping"));
}


TEST_F(GenMutateCodegenTest, AmrShlUnsafeToWrapping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 2\n"
    "    let x = y.shl_unsafe(z)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "shl", "shl_wrapping"));
}


TEST_F(GenMutateCodegenTest, AmrShrUnsafeToWrapping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 12\n"
    "    let z: U64 = 2\n"
    "    let x = y.shr_unsafe(z)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "shr", "shr_wrapping"));
}


TEST_F(GenMutateCodegenTest, AmrFpNegWrappingToUnsafe)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: F64 = 5.0\n"
    "    let x = -y\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("amr", "neg", "neg_unsafe"));
}


TEST_F(GenMutateCodegenTest, AmrAndSorTogether)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let z: U64 = 2\n"
    "    let x = y << z\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_SOR | MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("sor", "shl", "shr"));
  ASSERT_TRUE(has_mutant("amr", "shl", "shl_unsafe"));
}


TEST_F(GenMutateCodegenTest, AmrAndUoiTogether)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 5\n"
    "    let x = -y\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI | MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("uoi", "neg", "identity"));
  ASSERT_TRUE(has_mutant("amr", "neg", "neg_unsafe"));
}


TEST_F(GenMutateCodegenTest, Rule2ConstantAddNoAor)
{
  const char* src =
    "actor Main\n"                   // line 1
    "  new create(env: Env) =>\n"    // line 2
    "    let x: U64 = 3 + 2\n";     // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  ASSERT_EQ(count_category_at(3, "aor"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2ConstantAddHasCr)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3 + 2\n";     // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(3, "cr"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2ConstantAddNoUoi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3 + 2\n";     // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));

  ASSERT_EQ(count_category_at(3, "uoi"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2VariableAddHasAor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y + 1\n";     // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(4, "aor"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2NestedConstantNoAor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = (1 + 2) * 3\n";  // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  ASSERT_EQ(count_category_at(3, "aor"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2ConstantShiftNoSor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 1 << 4\n";    // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_SOR;
  DO(test_compile(src, "ir"));

  ASSERT_EQ(count_category_at(3, "sor"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2ConstantBitwiseNoBor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 0xFF and 0x0F\n";  // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR;
  DO(test_compile(src, "ir"));

  ASSERT_EQ(count_category_at(3, "bor"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2VariableCmpHasRor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3\n"          // line 3
    "    let y: Bool = x == 3\n";   // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ROR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(4, "ror"), 0u);
}


// Bool == gets only eq->ne; Bool != gets only ne->eq; Bool match pattern
// gets only eq->ne. Numeric == still gets all five replacements.
TEST_F(GenMutateCodegenTest, BoolRorSwapOnly)
{
  const char* src =
    "actor Main\n"                                       // 1
    "  new create(env: Env) =>\n"                        // 2
    "    let a: Bool = true\n"                           // 3
    "    let b: Bool = false\n"                          // 4
    "    let eq_res = a == b\n"                          // 5
    "    let ne_res = a != b\n"                          // 6
    "    let m = match a\n"                              // 7
    "    | true => U64(1)\n"                             // 8
    "    | false => U64(0)\n"                            // 9
    "    end\n"                                          // 10
    "    let x: U64 = 10\n"                              // 11
    "    let num_eq = x == U64(5)\n";                    // 12

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ROR;
  DO(test_compile(src, "ir"));

  // Bool ==: exactly 1 ROR record (eq->ne), no ordering variants
  ASSERT_EQ(count_category_at_file("", 5, "ror"), 1u)
    << "Bool == should have exactly 1 ROR record";
  ASSERT_TRUE(has_mutant_at_line("", 5, "ror", "eq", "ne"))
    << "Bool == must have eq->ne";
  ASSERT_FALSE(has_mutant_at_line("", 5, "ror", "eq", "lt"))
    << "Bool == must not have eq->lt";

  // Bool !=: exactly 1 ROR record (ne->eq), no ordering variants
  ASSERT_EQ(count_category_at_file("", 6, "ror"), 1u)
    << "Bool != should have exactly 1 ROR record";
  ASSERT_TRUE(has_mutant_at_line("", 6, "ror", "ne", "eq"))
    << "Bool != must have ne->eq";
  ASSERT_FALSE(has_mutant_at_line("", 6, "ror", "ne", "lt"))
    << "Bool != must not have ne->lt";

  // Bool match pattern: first case gets 1 ROR (eq->ne); last case of
  // exhaustive match gets 0 (branch result is dead — both sides continue)
  ASSERT_EQ(count_category_at_file("", 8, "ror"), 1u)
    << "Bool match | true should have exactly 1 ROR record";
  ASSERT_EQ(count_category_at_file("", 9, "ror"), 0u)
    << "Bool match | false (last case, exhaustive) should have 0 ROR records";

  // Numeric ==: still gets all 5 replacements
  ASSERT_EQ(count_category_at_file("", 12, "ror"), 5u)
    << "Numeric == should have all 5 ROR records";
  ASSERT_TRUE(has_mutant_at_line("", 12, "ror", "eq", "ne"));
  ASSERT_TRUE(has_mutant_at_line("", 12, "ror", "eq", "lt"));
  ASSERT_TRUE(has_mutant_at_line("", 12, "ror", "eq", "le"));
  ASSERT_TRUE(has_mutant_at_line("", 12, "ror", "eq", "ge"));
  ASSERT_TRUE(has_mutant_at_line("", 12, "ror", "eq", "gt"));
}


TEST_F(GenMutateCodegenTest, ExhaustiveMatchLastCaseSuppressed)
{
  // Last case of an exhaustive match with no else has a dead branch —
  // both sides continue to the same block. Mutating the pattern comparison
  // is pointless. Earlier cases and matches with else should still mutate.
  const char* src =
    "primitive A\n"                                       // 1
    "primitive B\n"                                       // 2
    "primitive C\n"                                       // 3
    "type ABC is (A | B | C)\n"                           // 4
    "\n"                                                  // 5
    "actor Main\n"                                        // 6
    "  new create(env: Env) =>\n"                         // 7
    "    _bool(true)\n"                                   // 8
    "    _numelse(U8(1))\n"                               // 9
    "    _tup(true, false)\n"                             // 10
    "\n"                                                  // 11
    "  fun _bool(b: Bool): String =>\n"                   // 12
    "    match \\exhaustive\\ b\n"                        // 13
    "    | true => \"T\"\n"                               // 14
    "    | false => \"F\"\n"                              // 15
    "    end\n"                                           // 16
    "\n"                                                  // 17
    "  fun _numelse(n: U8): String =>\n"                  // 18
    "    match n\n"                                       // 19
    "    | 1 => \"one\"\n"                                // 20
    "    | 2 => \"two\"\n"                                // 21
    "    else \"other\"\n"                                // 22
    "    end\n"                                           // 23
    "\n"                                                  // 24
    "  fun _tup(p: Bool, q: Bool): String =>\n"           // 25
    "    match \\exhaustive\\ (p, q)\n"                   // 26
    "    | (true, true) => \"TT\"\n"                      // 27
    "    | (true, false) => \"TF\"\n"                     // 28
    "    | (false, true) => \"FT\"\n"                     // 29
    "    | (false, false) => \"FF\"\n"                    // 30
    "    end\n";                                          // 31

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ROR;
  DO(test_compile(src, "ir"));

  // Bool exhaustive: | true (first case) has 1 ROR; | false (last) has 0
  ASSERT_EQ(count_category_at_file("", 14, "ror"), 1u)
    << "first Bool case should have 1 ROR";
  ASSERT_EQ(count_category_at_file("", 15, "ror"), 0u)
    << "last Bool case (exhaustive, no else) should have 0 ROR";

  // Numeric with else: both cases mutate (else provides a real fallthrough)
  ASSERT_GT(count_category_at_file("", 20, "ror"), 0u)
    << "first numeric case with else should have ROR";
  ASSERT_GT(count_category_at_file("", 21, "ror"), 0u)
    << "second numeric case with else should have ROR";

  // Tuple exhaustive: (true,true) first case — both elements mutate;
  // (false,false) last case — neither element mutates
  ASSERT_GT(count_category_at_file("", 27, "ror"), 0u)
    << "first tuple case should have ROR";
  ASSERT_EQ(count_category_at_file("", 30, "ror"), 0u)
    << "last tuple case (exhaustive, no else) should have 0 ROR";

  // Middle tuple cases: not last, so they should still mutate
  ASSERT_GT(count_category_at_file("", 28, "ror"), 0u)
    << "middle tuple case should have ROR";
  ASSERT_GT(count_category_at_file("", 29, "ror"), 0u)
    << "second-to-last tuple case should have ROR";
}


TEST_F(GenMutateCodegenTest, Rule2VariableShiftHasSor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 3\n"
    "    let x: U64 = y << 4\n";    // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_SOR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(4, "sor"), 0u);
}


TEST_F(GenMutateCodegenTest, CrIndependenceAdd)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y + 5\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  auto cr_only = collect_category("cr");

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  auto cr_all = collect_category("cr");

  ASSERT_EQ(cr_only, cr_all);
}


TEST_F(GenMutateCodegenTest, CrIndependenceNested)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y + (1 + 2)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  auto cr_only = collect_category("cr");

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  auto cr_all = collect_category("cr");

  ASSERT_EQ(cr_only, cr_all);
}


TEST_F(GenMutateCodegenTest, CrIndependenceShift)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y << 4\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  auto cr_only = collect_category("cr");

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  auto cr_all = collect_category("cr");

  ASSERT_EQ(cr_only, cr_all);
}


TEST_F(GenMutateCodegenTest, CrAtShiftOperands)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 1 << 0\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "lhs", "1->0"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "0->1"));
}


TEST_F(GenMutateCodegenTest, CrAtComparison)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: Bool = y == 0\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "rhs", "0->1"));
}


TEST_F(GenMutateCodegenTest, ConstructorCallNotCrTarget)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(10) + U64(5)\n";  // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("aor", "add", "sub"));
  ASSERT_FALSE(has_mutant("cr", "lhs", "n->n+1"));
  ASSERT_FALSE(has_mutant("cr", "rhs", "n->n+1"));
}


TEST_F(GenMutateCodegenTest, Rule2AllCategoriesOnConstantExpr)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 3 + 2\n";     // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(3, "cr"), 0u);
  ASSERT_EQ(count_category_at(3, "aor"), 0u);
  ASSERT_EQ(count_category_at(3, "uoi"), 0u);
  ASSERT_EQ(count_category_at(3, "amr"), 0u);
}


TEST_F(GenMutateCodegenTest, IndependenceMultiCategory)
{
  const char* src =
    "actor Main\n"                                   // line 1
    "  new create(env: Env) =>\n"                    // line 2
    "    let y: U64 = 10\n"                          // line 3
    "    let a: U64 = y + 5\n"                       // line 4  add
    "    let b: U64 = y * 3\n"                       // line 5  mul
    "    let c: U64 = y / 2\n"                        // line 6  div
    "    let d: U64 = y % 3\n"                       // line 7  rem
    "    let e: U64 = y << 2\n"                      // line 8  shl
    "    let f: U64 = y >> 1\n"                      // line 9  shr
    "    let g: U64 = y and 0xFF\n"                  // line 10 and
    "    let h: U64 = y or 1\n"                      // line 11 or
    "    let i: U64 = y xor 7\n"                     // line 12 xor
    "    let j: Bool = y == 0\n"                     // line 13 eq
    "    let k: Bool = y > 5\n"                      // line 14 gt
    "    let l: U64 = (1 + 2) * 3\n"                // line 15 nested
    "    let m: U64 = ((1 + 2) + 3) * 4\n"          // line 16 nested
    "    let n: U64 = ((1 + 2) + (3 + 4)) * 5\n"   // line 17 nested
    "    let o: U64 = (1 << 2) * 3\n"               // line 18 shift nested
    "    let p: U64 = (1 xor 2) * 3\n"              // line 19 xor nested
    "    let q: U64 = (5 and 3) * 2\n"              // line 20 and nested
    "    let r: U64 = (not 0x20) * 2\n";            // line 21 not nested

  const char* categories[] = {"aor", "bor", "ror", "sor", "uoi", "cr", "amr"};
  size_t cat_count = sizeof(categories) / sizeof(categories[0]);

  uint32_t cat_flags[] = {
    MUTATE_OP_AOR, MUTATE_OP_BOR, MUTATE_OP_ROR, MUTATE_OP_SOR,
    MUTATE_OP_UOI, MUTATE_OP_CR, MUTATE_OP_AMR
  };

  for(size_t ci = 0; ci < cat_count; ci++)
  {
    opt.mutate = true;
    opt.mutate_operators = cat_flags[ci];
    DO(test_compile(src, "ir"));
    auto alone = collect_category(categories[ci]);

    opt.mutate_operators = MUTATE_OP_ALL;
    DO(test_compile(src, "ir"));
    auto with_all = collect_category(categories[ci]);

    ASSERT_EQ(alone, with_all)
      << "Category " << categories[ci] << " differs alone vs all-operators";
  }
}


TEST_F(GenMutateCodegenTest, IndependenceCrPlusSingleCategory)
{
  const char* src =
    "actor Main\n"                                           // line 1
    "  new create(env: Env) =>\n"                            // line 2
    "    let y: U64 = 10\n"                                  // line 3
    "    let a: U64 = y + 5\n"                               // line 4: add
    "    let b: U64 = y * 3\n"                               // line 5: mul
    "    let c: U64 = y / 2\n"                               // line 6: div
    "    let d: U64 = y % 3\n"                               // line 7: rem
    "    let e: U64 = y << 2\n"                              // line 8: shl
    "    let f: U64 = y >> 1\n"                              // line 9: shr
    "    let g: U64 = y and 0xFF\n"                          // line 10: and
    "    let h: U64 = y or 1\n"                              // line 11: or
    "    let i: U64 = y xor 7\n"                             // line 12: xor
    "    let j: Bool = y == 0\n"                             // line 13: eq
    "    let k: Bool = y > 5\n"                              // line 14: gt
    "    let l: U64 = (1 + 2) * 3\n"                        // line 15: nested
    "    let m: U64 = ((1 + 2) + 3) * 4\n"                  // line 16: nested
    "    let n: U64 = ((1 + 2) + (3 + 4)) * 5\n"           // line 17: nested
    "    let o: U64 = (1 << 2) * 3\n"                       // line 18: shift nested
    "    let p: U64 = (1 xor 2) * 3\n"                      // line 19: xor nested
    "    let q: U64 = (5 and 3) * 2\n"                      // line 20: and nested
    "    let r: U64 = (not 0x20) * 2\n"                     // line 21: not nested
    "    let x: I64 = 10\n"                                  // line 22
    "    let s: I64 = (-(1 + 2)) * 3\n"                     // line 23: neg
    "    let t: I64 = 100 / 3\n"                             // line 24: safe div
    "    let u: I64 = if not true then x + 5 else x + 7 end\n" // line 25
    "    let v: I64 = -0x8000000000000000 / -1\n"            // line 26: min/-1
    "    let w: I64 = -0x8000000000000000 % -1\n";           // line 27: min/-1 rem

  const char* categories[] = {"aor", "bor", "ror", "sor", "uoi", "amr"};
  uint32_t cat_flags[] = {
    MUTATE_OP_AOR, MUTATE_OP_BOR, MUTATE_OP_ROR, MUTATE_OP_SOR,
    MUTATE_OP_UOI, MUTATE_OP_AMR
  };
  size_t cat_count = sizeof(categories) / sizeof(categories[0]);

  for(size_t ci = 0; ci < cat_count; ci++)
  {
    opt.mutate = true;
    opt.mutate_operators = cat_flags[ci];
    DO(test_compile(src, "ir"));
    auto alone = collect_category(categories[ci]);

    opt.mutate_operators = cat_flags[ci] | MUTATE_OP_CR;
    DO(test_compile(src, "ir"));
    auto with_cr = collect_category(categories[ci]);

    ASSERT_EQ(alone, with_cr)
      << "Category " << categories[ci] << " differs alone vs with cr";
  }
}


TEST_F(GenMutateCodegenTest, IndependenceNestedDivFloat)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let a: U64 = (10 / 2) * 3\n"                // line 4
    "    let x: F64 = F64(1.0)\n"                     // line 5
    "    let b: F64 = (10.0 / 4.0) * 2.0\n";         // line 6

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  // (10 / 2) * 3: safe integer div produces a phi (no shadow), so the *
  // has a non-constant operand and AOR fires there.
  ASSERT_GT(count_category_at(4, "aor"), 0u);

  // (10.0 / 4.0) * 2.0: float div produces constant shadow, no AOR at *
  ASSERT_EQ(count_category_at(6, "aor"), 0u);

  // With cr, same: integer div still non-constant, float div still constant
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  ASSERT_GT(count_category_at(4, "aor"), 0u);
  ASSERT_EQ(count_category_at(6, "aor"), 0u);
}


TEST_F(GenMutateCodegenTest, IndependenceNegBranchConditional)
{
  // Nesting: neg of a sum, safe div, branch with constant guard.
  // Each non-CR category's set must be identical with and without CR.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: I64 = 10\n"
    "    let a: I64 = (-(1 + 2)) * 3\n"                      // line 4: neg, add, mul
    "    let b: I64 = 100 / 3\n"                              // line 5: safe div
    "    let c: I64 = if not true then x + 5 else x + 7 end\n" // line 6
    "    let d: I64 = x * b\n";                               // line 7

  const char* categories[] = {"aor", "ror", "uoi", "amr"};
  uint32_t cat_flags[] = {
    MUTATE_OP_AOR, MUTATE_OP_ROR, MUTATE_OP_UOI, MUTATE_OP_AMR
  };
  size_t cat_count = sizeof(categories) / sizeof(categories[0]);

  for(size_t ci = 0; ci < cat_count; ci++)
  {
    opt.mutate = true;
    opt.mutate_operators = cat_flags[ci];
    DO(test_compile(src, "ir"));
    auto alone = collect_category(categories[ci]);

    opt.mutate_operators = cat_flags[ci] | MUTATE_OP_CR;
    DO(test_compile(src, "ir"));
    auto with_cr = collect_category(categories[ci]);

    ASSERT_EQ(alone, with_cr)
      << "Category " << categories[ci] << " differs alone vs with cr";
  }
}


TEST_F(GenMutateCodegenTest, ShortCircuitConstFoldLeftSuppressesUoi)
{
  // not (false and (x > 0)): left is constant false, so the and
  // constant-folds. The outer not has a constant operand, so no UOI.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 10\n"
    "    let r: Bool = not (false and (x > 0))\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));

  // No UOI at the outer not: the operand is constant (false and ... = false)
  bool has_uoi_not = false;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "uoi") == 0 &&
       strcmp(r->original, "not") == 0)
      has_uoi_not = true;
  }
  ASSERT_FALSE(has_uoi_not);
}


TEST_F(GenMutateCodegenTest, ShortCircuitVariableAllowsUoi)
{
  // not ((x > 0) and (x < 100)): both sides are variable, so
  // the result is non-constant and UOI fires on the outer not.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 10\n"
    "    let r: Bool = not ((x > 0) and (x < 100))\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));

  bool has_uoi_not = false;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "uoi") == 0 &&
       strcmp(r->original, "not") == 0)
      has_uoi_not = true;
  }
  ASSERT_TRUE(has_uoi_not);
}


TEST_F(GenMutateCodegenTest, ShortCircuitConstFoldRightSuppressesUoi)
{
  // not ((x > 0) and false): right is constant false.
  // The right-constant optimization at the phi returns false directly,
  // so the outer not has a constant operand and UOI does not fire.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 10\n"
    "    let r: Bool = not ((x > 0) and false)\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));

  bool has_uoi_not = false;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "uoi") == 0 &&
       strcmp(r->original, "not") == 0)
      has_uoi_not = true;
  }
  ASSERT_FALSE(has_uoi_not);
}


TEST_F(GenMutateCodegenTest, Rule2NotTrueNoUoi)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: Bool = not true\n";  // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at(3, "uoi"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2NegOneLiteral)
{
  // -1: no UOI identity, one CR (1->0)
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 10\n"
    "    let x: I64 = y + -1\n";  // line 4, neg on line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "operand", "1->0"));
}


TEST_F(GenMutateCodegenTest, Rule2NegOneLiteralNoUoiIdentity)
{
  // -1: the operand of neg is constant 1, so no UOI identity at neg
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 10\n"
    "    let x: I64 = y + -1\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));

  // UOI insertion on the add result (y is variable) should exist
  ASSERT_TRUE(has_mutant_at(4, 20, "uoi", "result", "neg"));
  // No UOI identity at the neg site (constant operand 1)
  ASSERT_FALSE(has_mutant_at(4, 24, "uoi", "neg", "identity"));
}


TEST_F(GenMutateCodegenTest, Rule2NegFloatCr)
{
  // -2.5: CR records x->-x
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: F64 = F64(1.0)\n"
    "    let x: F64 = y + -2.5\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "operand", "x->-x"));
}


TEST_F(GenMutateCodegenTest, Rule2ConstantShiftCrOnly)
{
  // 1 << 4: no SOR, but CR on both literals
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y and (1 << 4)\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_SOR | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_EQ(count_category_at(4, "sor"), 0u);
  ASSERT_GT(count_category_at(4, "cr"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2NestedConstantAcrossBuilds)
{
  // (1 + 2) * 3: no AOR at * in both aor and aor+cr builds
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y + ((1 + 2) * 3)\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));
  auto aor_alone = collect_category("aor");

  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  auto aor_with_cr = collect_category("aor");

  ASSERT_EQ(aor_alone, aor_with_cr);
}


TEST_F(GenMutateCodegenTest, ShiftCheckViaShadow)
{
  // x << (32 + 40) on U64 is a compile error — sum is 72, exceeds 64-bit width
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y << (32 + 40)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_error(src, "ir"));
}


TEST_F(GenMutateCodegenTest, ShiftCheckViaShadowCrOnly)
{
  // Same compile error with cr only — shadow detects the constant
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y << (32 + 40)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_error(src, "ir"));
}


TEST_F(GenMutateCodegenTest, SafeSignedDivremShadowValidIr)
{
  // Safe signed divrem with constant operands produces valid IR
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 10\n"
    "    let a: I64 = y + (10 / 3)\n"     // line 4 safe signed div
    "    let b: I64 = y + (10 % 3)\n";    // line 5 safe signed rem

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(4, "cr"), 0u);
  ASSERT_GT(count_category_at(5, "cr"), 0u);
}


TEST_F(GenMutateCodegenTest, CrCharLiteral)
{
  // Character literal: c == 'a' records n->n+1 and n->n-1
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: U64 = 10\n"
    "    let x: Bool = c == 'a'\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, ComputedValueNotCrTargetAnd)
{
  // x and (1 << 4): no CR at the and (1<<4 is computed)
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y and (1 << 4)\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  // CR at the shift (line 4) for lhs=1 and rhs=4, but not at the and
  ASSERT_TRUE(has_mutant("cr", "lhs", "1->0"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));

  // No CR with original "rhs" at the and since (1<<4) is computed
  ASSERT_FALSE(has_mutant_at(4, 25, "cr", "rhs", "n->n+1"));
}


TEST_F(GenMutateCodegenTest, ComputedValueNotCrTargetAdd)
{
  // x + (2 * 4): no CR at the outer +
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y + (2 * 4)\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  // CR at the mul for 2 and 4, but not at the + for the computed result
  ASSERT_TRUE(has_mutant("cr", "lhs", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));
}


TEST_F(GenMutateCodegenTest, CrShiftWidth1Amount)
{
  // Shift by width-1 (63 on U64): records n->n-1 but not n->n+1
  // because n+1 (64) >= width and is silently dropped
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y << 63\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n-1"));
  ASSERT_FALSE(has_mutant("cr", "rhs", "n->n+1"));
}


TEST_F(GenMutateCodegenTest, CrComparisonOperands)
{
  // x == 0 records 0->1 (rhs); x > 5 records n->n+1 and n->n-1
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let a: Bool = y == 0\n"   // line 4
    "    let b: Bool = y > 5\n";   // line 5

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(4, "cr"), 0u);
  ASSERT_TRUE(has_mutant("cr", "rhs", "0->1"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, CrNegOperand)
{
  // -1: records 1->0 at the neg site, no CR at the outer +
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 10\n"
    "    let x: I64 = y + -1\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "operand", "1->0"));
}


TEST_F(GenMutateCodegenTest, CrNegTwoOperand)
{
  // -2: records n->n+1 and n->n-1 at the neg site
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 10\n"
    "    let x: I64 = y + -2\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "operand", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "operand", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, CrNotOperand)
{
  // x and not 0x20: CR at the not site, none at the and
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y and (not 0x20)\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "operand", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "operand", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, CrWideLiteralU128)
{
  // U128 literal above 2^64: records n->n+1 and n->n-1, not 0->1
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U128 = U128(1)\n"
    "    let x: U128 = y + 0x1_0000_0000_0000_0000\n";  // line 4, 2^64

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n-1"));
  ASSERT_FALSE(has_mutant("cr", "rhs", "0->1"));
}


TEST_F(GenMutateCodegenTest, CrWrapU8)
{
  // U8 255 + n->n+1 wraps to 0
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U8 = U8(1)\n"
    "    let x: U8 = y + 255\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n+1"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "n->n-1"));
}


TEST_F(GenMutateCodegenTest, CrIntLiteralInFloatContext)
{
  // Integer literal 2 in F64 context records float variants (x->-x)
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: F64 = F64(1.0)\n"
    "    let x: F64 = y + 2\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "rhs", "x->-x"));
}


TEST_F(GenMutateCodegenTest, CrIndependenceDiv)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y / 5\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  auto cr_only = collect_category("cr");

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  auto cr_all = collect_category("cr");

  ASSERT_EQ(cr_only, cr_all);
}


TEST_F(GenMutateCodegenTest, CrIndependenceCmp)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let a: Bool = y == 0\n"
    "    let b: Bool = y > 5\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  auto cr_only = collect_category("cr");

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  auto cr_all = collect_category("cr");

  ASSERT_EQ(cr_only, cr_all);
}


TEST_F(GenMutateCodegenTest, CrIndependenceNeg)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 10\n"
    "    let x: I64 = y + -2\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  auto cr_only = collect_category("cr");

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  auto cr_all = collect_category("cr");

  ASSERT_EQ(cr_only, cr_all);
}


TEST_F(GenMutateCodegenTest, CrIndependenceNot)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y and (not 0x20)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  auto cr_only = collect_category("cr");

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  auto cr_all = collect_category("cr");

  ASSERT_EQ(cr_only, cr_all);
}


TEST_F(GenMutateCodegenTest, CrDivByOneLiteral)
{
  // x div 1: records CR on the 1 (1->0, 1->n+1 which is 2)
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y / 1\n";    // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_GT(count_category_at(4, "cr"), 0u);
}


TEST_F(GenMutateCodegenTest, SafeDivremNonPathologicalNoShadow)
{
  // Safe integer div/rem with non-pathological constant operands produces
  // no shadow (the result is a phi node). Downstream operators must still
  // get their mutations.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: I64 = 100 / 3\n"                            // line 3: safe div
    "    let y: I64 = x * 2\n"                               // line 4
    "    let a: I64 = (10 % 3) + 3\n";                       // line 5: safe rem

  // AOR only: safe divrem has no shadow, so * on line 4 has AOR
  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));
  ASSERT_GT(count_category_at(4, "aor"), 0u);

  // AOR+CR: same
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  ASSERT_GT(count_category_at(4, "aor"), 0u);

  // AMR only vs AMR+CR: should match
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));
  size_t amr_only = collect_category("amr").size();

  opt.mutate_operators = MUTATE_OP_AMR | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  size_t amr_plus_cr = collect_category("amr").size();
  ASSERT_EQ(amr_only, amr_plus_cr);

  // UOI only vs UOI+CR: should match
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));
  size_t uoi_only = collect_category("uoi").size();

  opt.mutate_operators = MUTATE_OP_UOI | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  size_t uoi_plus_cr = collect_category("uoi").size();
  ASSERT_EQ(uoi_only, uoi_plus_cr);
}


TEST_F(GenMutateCodegenTest, SafeDivremMinNeg1Shadow)
{
  // min_value / -1 and min_value % -1 are the pathological cases where
  // make_divrem_inner constant-folds (returns a constant, not a phi).
  // The shadow must match. Nested expressions test this: the div result
  // feeds directly into * so the shadow propagates within the expression.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let t: I64 = (-9223372036854775808 / -1) * 2\n"    // line 3: min/-1 div * 2
    "    let v: I64 = (-9223372036854775808 % -1) * 3\n";   // line 4: min/-1 rem * 3

  // With a correct shadow, (-min/-1) is a known constant so the * has
  // both operands constant and AOR is suppressed.
  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at(3, "aor"), 0u);
  ASSERT_EQ(count_category_at(4, "aor"), 0u);

  // AOR+CR: same suppression
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at(3, "aor"), 0u);
  ASSERT_EQ(count_category_at(4, "aor"), 0u);

  // Each non-CR category alone vs +CR: identical sets
  const char* categories[] = {"aor", "amr", "uoi"};
  uint32_t cat_flags[] = {MUTATE_OP_AOR, MUTATE_OP_AMR, MUTATE_OP_UOI};
  for(size_t ci = 0; ci < 3; ci++)
  {
    opt.mutate_operators = cat_flags[ci];
    DO(test_compile(src, "ir"));
    auto alone = collect_category(categories[ci]);

    opt.mutate_operators = cat_flags[ci] | MUTATE_OP_CR;
    DO(test_compile(src, "ir"));
    auto with_cr = collect_category(categories[ci]);

    ASSERT_EQ(alone, with_cr)
      << "Category " << categories[ci] << " differs alone vs with cr";
  }
}


TEST_F(GenMutateCodegenTest, SafeDivFeedingShiftCompiles)
{
  // Safe div result feeding a shift must compile under all mutation flags.
  // Regression: the shadow said the div result was constant, causing
  // the width check to reject it at compile time (100 > 64-bit width).
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 10\n"
    "    let y: U64 = x << (100 / 1)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));

  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));
}


TEST_F(GenMutateCodegenTest, Rule2ConstantDivNoAor)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 10 / 2\n";    // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  ASSERT_EQ(count_category_at(3, "aor"), 0u);
}


TEST_F(GenMutateCodegenTest, Rule2ConstantNegNoAmr)
{
  // -1: constant operand, no AMR
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I64 = 10\n"
    "    let x: I64 = y + -1\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AMR;
  DO(test_compile(src, "ir"));

  // No AMR for neg at line 4 (constant operand 1)
  // AMR at the add on line 4 is fine (y is variable)
  size_t amr_neg_count = 0;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "amr") == 0 &&
       strcmp(r->original, "neg") == 0)
      amr_neg_count++;
  }
  ASSERT_EQ(amr_neg_count, 0u);
}


TEST_F(GenMutateCodegenTest, Rule2ConstantNotNoUoi)
{
  // not 0x20: constant operand, no UOI at the not
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let x: U64 = y and (not 0x20)\n";  // line 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_UOI;
  DO(test_compile(src, "ir"));

  // UOI at the and (variable y) but not at the not (constant 0x20)
  // UOI on not would record "not" as original
  bool has_uoi_not_at_4 = false;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "uoi") == 0 &&
       strcmp(r->original, "not") == 0)
      has_uoi_not_at_4 = true;
  }
  ASSERT_FALSE(has_uoi_not_at_4);
}


TEST_F(GenMutateCodegenTest, CrShiftLhsOperand)
{
  // 1 << 0: check lhs CR records
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: U64 = 1 << 0\n";  // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("cr", "lhs", "1->0"));
  ASSERT_TRUE(has_mutant("cr", "rhs", "0->1"));
}


TEST_F(GenMutateCodegenTest, SafeSignedRemValidIr)
{
  // Safe signed rem with constant operands produces valid IR
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: I8 = I8(10)\n"
    "    let x: I8 = y.rem(I8(-128) % -1)\n";

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_CR;
  DO(test_compile(src, "ir"));
}


TEST_F(GenMutateCodegenTest, I128DivremShadowFalseNegOne)
{
  // I128 divisor 18446744073709551615 (2^64-1) has low 64 bits all-ones,
  // which LLVMConstIntGetSExtValue truncates to -1. The full I128 value
  // is positive, so mutate_safe_divrem_shadow must return NULL. With the
  // bug: the div result incorrectly gets a shadow, suppressing AOR on the
  // downstream add. Without: no shadow, AOR fires on the add.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let z: I128 = (9223372036854775808 / 18446744073709551615)"
                        " + 1\n";                                // line 3

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));
  ASSERT_GT(count_category_at(3, "aor"), 0u);
}

TEST_F(GenMutateCodegenTest, ShortCircuitVerifyBaseline)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = U64(3) + U64(5)\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));
  verify_module();
}

// Table-driven test: {and, or} x left {true, false, x>0} x right
// {true, false, x<100}. BOR count is 1 iff fire, 0 otherwise.
// All code lives inside create so it's reachable; x is a local variable.
TEST_F(GenMutateCodegenTest, ShortCircuitBorFiringTable)
{
  struct row {
    const char* op;
    const char* left_expr;
    const char* right_expr;
    bool expect_fire;
  };

  // x is a local U64, so (x > U64(0)) and (x < U64(100)) produce
  // non-constant LLVM values. true/false are constant.
  row rows[] = {
    // and: left non-deciding const (true) + non-const right -> fire
    {"and", "true", "(x < U64(100))", true},
    // and: left non-deciding const + const right -> no fire
    {"and", "true", "false", false},
    {"and", "true", "true", false},
    // and: left non-const + non-const right -> fire
    {"and", "(x > U64(0))", "(x < U64(100))", true},
    // and: left non-const + non-deciding const right (true) -> fire
    {"and", "(x > U64(0))", "true", true},
    // and: left non-const + deciding const right (false) -> no fire
    {"and", "(x > U64(0))", "false", false},
    // or: left non-deciding const (false) + non-const right -> fire
    {"or", "false", "(x < U64(100))", true},
    // or: left non-deciding const + const right -> no fire
    {"or", "false", "true", false},
    {"or", "false", "false", false},
    // or: left non-const + non-const right -> fire
    {"or", "(x > U64(0))", "(x < U64(100))", true},
    // or: left non-const + non-deciding const right (false) -> fire
    {"or", "(x > U64(0))", "false", true},
    // or: left non-const + deciding const right (true) -> no fire
    {"or", "(x > U64(0))", "true", false},
  };

  for(size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
  {
    row& r = rows[i];
    char src[512];
    snprintf(src, sizeof(src),
      "actor Main\n"                              // 1
      "  new create(env: Env) =>\n"               // 2
      "    let x: U64 = 10\n"                     // 3
      "    let z = %s %s %s\n",                    // 4
      r.left_expr, r.op, r.right_expr);

    opt.mutate = true;
    opt.mutate_operators = MUTATE_OP_BOR;
    DO(test_compile(src, "ir"))
      << "row " << i << ": " << r.left_expr << " " << r.op
      << " " << r.right_expr;

    size_t bor_at_4 = count_category_at_file("", 4, "bor");
    ASSERT_EQ(bor_at_4, r.expect_fire ? 1u : 0u)
      << "row " << i << ": " << r.left_expr << " " << r.op
      << " " << r.right_expr
      << " expected fire=" << r.expect_fire
      << " got bor count=" << bor_at_4;

    // Find the site's real column (if it fired) and verify no xor
    // replacement exists there.
    if(r.expect_fire)
    {
      size_t site_col = 0;
      for(size_t j = 0; j < compile->mutate.mutant_count; j++)
      {
        mutant_record_t* m = &compile->mutate.mutants[j];
        if(m->line == 4 && strcmp(m->category, "bor") == 0)
        {
          site_col = m->column;
          break;
        }
      }
      ASSERT_GT(site_col, 0u)
        << "row " << i << ": fired but no bor record found";
      ASSERT_FALSE(has_mutant_at(4, site_col, "bor", r.op, "xor"))
        << "row " << i << ": unexpected xor replacement for " << r.op;
    }

    verify_module();

    // Also verify under ALL categories.
    opt.mutate_operators = MUTATE_OP_ALL;
    DO(test_compile(src, "ir"))
      << "row " << i << " ALL: " << r.left_expr << " " << r.op
      << " " << r.right_expr;
    size_t bor_at_4_all = count_category_at_file("", 4, "bor");
    ASSERT_EQ(bor_at_4_all, r.expect_fire ? 1u : 0u)
      << "row " << i << " ALL: bor count=" << bor_at_4_all;
    verify_module();
  }
}

// false and <expr with an operator>: right side gets no mutants.
TEST_F(GenMutateCodegenTest, ShortCircuitDecidingLeftSuppresses)
{
  const char* src =
    "actor Main\n"                                     // 1
    "  new create(env: Env) =>\n"                      // 2
    "    let x: U64 = 10\n"                            // 3
    "    let z = false and ((x + U64(1)) > U64(0))\n"; // 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at_file("", 4, "bor"), 0u);
  ASSERT_EQ(count_category_at_file("", 4, "aor"), 0u);

  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at_file("", 4, "aor"), 0u);

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at_file("", 4, "bor"), 0u);
  ASSERT_EQ(count_category_at_file("", 4, "aor"), 0u);
}

// Independence: each category's record set equals alone, +bor,
// and with all categories; bor equals alone and +cr.
TEST_F(GenMutateCodegenTest, ShortCircuitCategoryIndependence)
{
  const char* src =
    "actor Main\n"                                             // 1
    "  new create(env: Env) =>\n"                              // 2
    "    let x: U64 = 10\n"                                    // 3
    "    let a = (x > U64(0)) and (x < U64(100))\n"           // 4
    "    let b = (x > U64(0)) or (x < U64(100))\n"            // 5
    "    let c = (x > U64(0)) xor (x < U64(100))\n"           // 6
    "    let d = ((x > U64(0)) and (x < U64(100)))"
      " or (x == U64(5))\n"                                   // 7
    "    let e = if (x > U64(0)) and false then\n"             // 8
    "      x + U64(1)\n"                                       // 9
    "    else\n"                                                // 10
    "      x + U64(2)\n"                                       // 11
    "    end\n"                                                 // 12
    "    let f = if (x > U64(0)) and (x < U64(100)) then\n"   // 13
    "      x + U64(1)\n"                                       // 14
    "    else\n"                                                // 15
    "      x + U64(2)\n"                                       // 16
    "    end\n"                                                 // 17
    "    let g = e + f\n";                                      // 18

  const char* categories[] = {"ror", "cr", "aor", "bor", "uoi"};

  std::map<std::string, std::set<mutant_key>> alone;
  for(const char* cat : categories)
  {
    opt.mutate = true;
    if(strcmp(cat, "ror") == 0)
      opt.mutate_operators = MUTATE_OP_ROR;
    else if(strcmp(cat, "cr") == 0)
      opt.mutate_operators = MUTATE_OP_CR;
    else if(strcmp(cat, "aor") == 0)
      opt.mutate_operators = MUTATE_OP_AOR;
    else if(strcmp(cat, "bor") == 0)
      opt.mutate_operators = MUTATE_OP_BOR;
    else if(strcmp(cat, "uoi") == 0)
      opt.mutate_operators = MUTATE_OP_UOI;
    DO(test_compile(src, "ir"));
    alone[cat] = collect_category(cat);
  }

  for(const char* cat : categories)
  {
    if(strcmp(cat, "bor") == 0) continue;
    opt.mutate = true;
    if(strcmp(cat, "ror") == 0)
      opt.mutate_operators = MUTATE_OP_ROR | MUTATE_OP_BOR;
    else if(strcmp(cat, "cr") == 0)
      opt.mutate_operators = MUTATE_OP_CR | MUTATE_OP_BOR;
    else if(strcmp(cat, "aor") == 0)
      opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_BOR;
    else if(strcmp(cat, "uoi") == 0)
      opt.mutate_operators = MUTATE_OP_UOI | MUTATE_OP_BOR;
    DO(test_compile(src, "ir"));
    auto with_bor = collect_category(cat);
    ASSERT_EQ(alone[cat], with_bor)
      << cat << " differs alone vs +bor";
  }

  {
    opt.mutate = true;
    opt.mutate_operators = MUTATE_OP_BOR | MUTATE_OP_CR;
    DO(test_compile(src, "ir"));
    auto bor_with_cr = collect_category("bor");
    ASSERT_EQ(alone["bor"], bor_with_cr) << "bor differs alone vs +cr";
  }

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  for(const char* cat : categories)
  {
    auto with_all = collect_category(cat);
    ASSERT_EQ(alone[cat], with_all) << cat << " differs alone vs all";
  }

  // Positive control: then-arm of the firing and on line 14 has AOR.
  ASSERT_GT(count_category_at_file("", 14, "aor"), 0u)
    << "positive control: no aor on firing and's then arm";

  // Zero AOR on the then-arm of (x>0) and false (line 9).
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_BOR;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at_file("", 9, "aor"), 0u)
    << "and-false then-arm has AOR under AOR|BOR";

  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at_file("", 9, "aor"), 0u)
    << "and-false then-arm has AOR under ALL";
}

// Bool xor codegen: records at the site are exactly xor->and and
// xor->or with a shared coverage counter.
TEST_F(GenMutateCodegenTest, ShortCircuitBoolXorCodegen)
{
  const char* src =
    "actor Main\n"                                          // 1
    "  new create(env: Env) =>\n"                           // 2
    "    let x: U64 = 10\n"                                 // 3
    "    let z = (x > U64(0)) xor (x < U64(100))\n";       // 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR;
  DO(test_compile(src, "ir"));

  // Find the column of the xor site.
  size_t xor_col = 0;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "bor") == 0)
    {
      xor_col = r->column;
      break;
    }
  }
  ASSERT_GT(xor_col, 0u) << "no bor record at line 4";

  ASSERT_TRUE(has_mutant_at(4, xor_col, "bor", "xor", "and"))
    << "no xor->and at line 4";
  ASSERT_TRUE(has_mutant_at(4, xor_col, "bor", "xor", "or"))
    << "no xor->or at line 4";
  ASSERT_EQ(count_category_at_file("", 4, "bor"), 2u);

  int32_t cov = coverage_counter_at(4, xor_col, "bor");
  ASSERT_GE(cov, 0) << "could not find bor record at line 4";
  verify_module();

  // Under ALL categories.
  opt.mutate_operators = MUTATE_OP_ALL;
  DO(test_compile(src, "ir"));
  ASSERT_TRUE(has_mutant_at(4, xor_col, "bor", "xor", "and"));
  ASSERT_TRUE(has_mutant_at(4, xor_col, "bor", "xor", "or"));
  ASSERT_EQ(count_category_at_file("", 4, "bor"), 2u);
  verify_module();

  // true xor false: both constant, no mutants.
  const char* const_src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let z = true xor false\n";  // line 3

  opt.mutate_operators = MUTATE_OP_BOR;
  DO(test_compile(const_src, "ir"));
  ASSERT_EQ(count_category_at_file("", 3, "bor"), 0u);
}

// Coverage timing: BOR coverage counter differs from ROR counters.
TEST_F(GenMutateCodegenTest, ShortCircuitCoverageTiming)
{
  const char* src =
    "actor Main\n"                                          // 1
    "  new create(env: Env) =>\n"                           // 2
    "    let x: U64 = 10\n"                                 // 3
    "    let z = (x > U64(0)) and (x < U64(100))\n";       // 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR | MUTATE_OP_ROR;
  DO(test_compile(src, "ir"));

  int32_t bor_cov = -1;
  std::set<int32_t> ror_covs;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "bor") == 0)
      bor_cov = r->coverage_counter;
    if(r->line == 4 && strcmp(r->category, "ror") == 0)
      ror_covs.insert(r->coverage_counter);
  }
  ASSERT_GE(bor_cov, 0) << "no bor record at line 4";
  ASSERT_FALSE(ror_covs.empty()) << "no ror records at line 4";
  ASSERT_EQ(ror_covs.count(bor_cov), 0u)
    << "bor and ror share a coverage counter";

  // The coverage atomicrmw must be in the same block as the condbr
  // that uses the flip condition, not in the right-side block.
  auto module = llvm::unwrap(compile->module);
  bool found_cov_in_condbr_block = false;
  for(auto& fn : *module)
  {
    if(fn.getName().find("create") == llvm::StringRef::npos)
      continue;
    for(auto& bb : fn)
    {
      for(auto& inst : bb)
      {
        if(auto* rmw = llvm::dyn_cast<llvm::AtomicRMWInst>(&inst))
        {
          if(auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(
            rmw->getPointerOperand()))
          {
            if(auto* ci = llvm::dyn_cast<llvm::ConstantInt>(
              gep->getOperand(1)))
            {
              if(ci->getSExtValue() == bor_cov)
              {
                auto* term = bb.getTerminator();
                auto* br = llvm::dyn_cast<llvm::BranchInst>(term);
                ASSERT_NE(br, nullptr)
                  << "bor coverage block has no branch terminator";
                ASSERT_TRUE(br->isConditional())
                  << "bor coverage block ends with unconditional br";
                found_cov_in_condbr_block = true;
              }
            }
          }
        }
      }
    }
  }
  ASSERT_TRUE(found_cov_in_condbr_block)
    << "bor coverage atomicrmw not found in a condbr block";
}

// Coverage timing for Bool xor.
TEST_F(GenMutateCodegenTest, ShortCircuitBoolXorCoverageTiming)
{
  const char* src =
    "actor Main\n"                                          // 1
    "  new create(env: Env) =>\n"                           // 2
    "    let x: U64 = 10\n"                                 // 3
    "    let z = (x > U64(0)) xor (x < U64(100))\n";       // 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR | MUTATE_OP_ROR;
  DO(test_compile(src, "ir"));

  int32_t bor_cov = -1;
  std::set<int32_t> ror_covs;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "bor") == 0)
      bor_cov = r->coverage_counter;
    if(r->line == 4 && strcmp(r->category, "ror") == 0)
      ror_covs.insert(r->coverage_counter);
  }
  ASSERT_GE(bor_cov, 0) << "no bor record at line 4";
  ASSERT_FALSE(ror_covs.empty()) << "no ror records at line 4";
  ASSERT_EQ(ror_covs.count(bor_cov), 0u)
    << "bor and ror share a coverage counter for xor site";

  // The coverage atomicrmw must be in the left-exit block (which ends
  // with a condbr on the enter condition), not in the right-side block.
  auto module = llvm::unwrap(compile->module);
  bool found_cov_in_condbr_block = false;
  for(auto& fn : *module)
  {
    if(fn.getName().find("create") == llvm::StringRef::npos)
      continue;
    for(auto& bb : fn)
    {
      for(auto& inst : bb)
      {
        if(auto* rmw = llvm::dyn_cast<llvm::AtomicRMWInst>(&inst))
        {
          if(auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(
            rmw->getPointerOperand()))
          {
            if(auto* ci = llvm::dyn_cast<llvm::ConstantInt>(
              gep->getOperand(1)))
            {
              if(ci->getSExtValue() == bor_cov)
              {
                auto* term = bb.getTerminator();
                auto* br = llvm::dyn_cast<llvm::BranchInst>(term);
                ASSERT_NE(br, nullptr)
                  << "xor bor coverage block has no branch terminator";
                ASSERT_TRUE(br->isConditional())
                  << "xor bor coverage block ends with unconditional br";
                found_cov_in_condbr_block = true;
              }
            }
          }
        }
      }
    }
  }
  ASSERT_TRUE(found_cov_in_condbr_block)
    << "xor bor coverage atomicrmw not found in a condbr block";
}

// Nested sites get distinct ids and columns.
TEST_F(GenMutateCodegenTest, ShortCircuitNestedDistinctIds)
{
  const char* src =
    "actor Main\n"                                             // 1
    "  new create(env: Env) =>\n"                              // 2
    "    let x: U64 = 10\n"                                    // 3
    "    let z = ((x > U64(0)) and (x < U64(100)))"
      " or (x == U64(5))\n";                                  // 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR;
  DO(test_compile(src, "ir"));

  size_t bor_count = count_category_at_file("", 4, "bor");
  ASSERT_GE(bor_count, 2u)
    << "nested and/or should produce distinct BOR sites";

  std::set<int32_t> ids;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "bor") == 0)
      ids.insert(r->id);
  }
  ASSERT_GE(ids.size(), 2u) << "nested sites should have distinct ids";
}

// \nomutate\ suppresses BOR at the site.
TEST_F(GenMutateCodegenTest, ShortCircuitNomutate)
{
  const char* src =
    "actor Main\n"                                          // 1
    "  new \\nomutate\\ create(env: Env) =>\n"             // 2
    "    let x: U64 = 10\n"                                 // 3
    "    let z = (x > U64(0)) and (x < U64(100))\n";       // 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at_file("", 4, "bor"), 0u);
}

// --mutate-scope line range that excludes the site suppresses it.
TEST_F(GenMutateCodegenTest, ShortCircuitScopeFilter)
{
  const char* src =
    "actor Main\n"                                          // 1
    "  new create(env: Env) =>\n"                           // 2
    "    let x: U64 = 10\n"                                 // 3
    "    let z = (x > U64(0)) and (x < U64(100))\n";       // 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR;

  size_t alloc_size = sizeof(mutate_scope_t);
  mutate_scope_t* scopes =
    (mutate_scope_t*)ponyint_pool_alloc_size(alloc_size);
  char* file = (char*)ponyint_pool_alloc_size(1);
  file[0] = '\0';
  scopes[0].file = file;
  scopes[0].start_line = 1;
  scopes[0].end_line = 3;
  opt.mutate_scope_count = 1;
  opt.mutate_scopes = scopes;
  DO(test_compile(src, "ir"));
  ASSERT_EQ(count_category_at_file("", 4, "bor"), 0u);

  scopes = (mutate_scope_t*)ponyint_pool_alloc_size(alloc_size);
  file = (char*)ponyint_pool_alloc_size(1);
  file[0] = '\0';
  scopes[0].file = file;
  scopes[0].start_line = 1;
  scopes[0].end_line = 4;
  opt.mutate_scopes = scopes;
  DO(test_compile(src, "ir"));
  ASSERT_GT(count_category_at_file("", 4, "bor"), 0u);
}

// IR inspection: branch condition is xor(l_value, icmp eq sel id).
TEST_F(GenMutateCodegenTest, ShortCircuitIrShape)
{
  const char* src =
    "actor Main\n"                                          // 1
    "  new create(env: Env) =>\n"                           // 2
    "    let x: U64 = 10\n"                                 // 3
    "    let z = (x > U64(0)) and (x < U64(100))\n";       // 4

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_BOR;
  DO(test_compile(src, "ir"));
  ASSERT_GT(count_category_at_file("", 4, "bor"), 0u);

  int32_t bor_id = -1;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(r->line == 4 && strcmp(r->category, "bor") == 0)
    {
      bor_id = r->id;
      break;
    }
  }
  ASSERT_GE(bor_id, 0);

  auto module = llvm::unwrap(compile->module);
  bool found_xor = false;
  for(auto& fn : *module)
  {
    if(fn.getName().find("create") == llvm::StringRef::npos)
      continue;
    for(auto& bb : fn)
    {
      for(auto& inst : bb)
      {
        if(auto* xor_inst = llvm::dyn_cast<llvm::BinaryOperator>(&inst))
        {
          if(xor_inst->getOpcode() != llvm::Instruction::Xor)
            continue;
          for(unsigned op = 0; op < 2; op++)
          {
            if(auto* icmp = llvm::dyn_cast<llvm::ICmpInst>(
              xor_inst->getOperand(op)))
            {
              if(icmp->getPredicate() != llvm::ICmpInst::ICMP_EQ)
                continue;
              for(unsigned j = 0; j < 2; j++)
              {
                if(auto* ci = llvm::dyn_cast<llvm::ConstantInt>(
                  icmp->getOperand(j)))
                {
                  if(ci->getSExtValue() == bor_id)
                    found_xor = true;
                }
              }
            }
          }
        }
      }
    }
  }
  ASSERT_TRUE(found_xor)
    << "could not find xor(l_value, icmp eq sel " << bor_id << ")";
}


TEST_F(GenMutateCodegenTest, VcrSkipsReceiverAndArgs)
{
  // The VCR switch must be placed before arg/receiver generation so the
  // skip path bypasses evaluating them entirely. Verify that the switch
  // instruction appears before any call to bump (the arg side-effect).
  const char* src =
    "class Obj\n"                                          // 1
    "  var _count: U64 = 0\n"                              // 2
    "  fun ref bump(): U64 =>\n"                           // 3
    "    _count = _count + U64(1)\n"                       // 4
    "    _count\n"                                         // 5
    "  fun ref do_something(x: U64) => None\n"             // 6
    "\n"                                                   // 7
    "actor Main\n"                                         // 8
    "  new create(env: Env) =>\n"                          // 9
    "    let obj = Obj\n"                                  // 10
    "    obj.do_something(obj.bump())\n";                  // 11

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_VCR;
  DO(test_compile(src, "ir"));

  // There should be a VCR mutant for do_something at line 11.
  ASSERT_GT(count_category_at_file("", 11, "vcr"), 0u)
    << "expected VCR mutant for do_something call";

  // Walk the IR: in the create function, iterate instructions in block order.
  // The VCR switch instruction must appear before any call to bump.
  // If VCR were placed after arg gen, bump's call would precede the switch.
  auto module = llvm::unwrap(compile->module);
  bool found_switch = false;
  bool found_bump_before_switch = false;
  bool found_bump_after_switch = false;

  for(auto& fn : *module)
  {
    if(fn.getName().find("create") == llvm::StringRef::npos)
      continue;
    for(auto& bb : fn)
    {
      for(auto& inst : bb)
      {
        if(auto* si = llvm::dyn_cast<llvm::SwitchInst>(&inst))
        {
          // Check if default case goes to a block named vcr.call
          auto* def_bb = si->getDefaultDest();
          if(def_bb && def_bb->getName() == "vcr.call")
            found_switch = true;
        }
        if(auto* ci = llvm::dyn_cast<llvm::CallInst>(&inst))
        {
          auto* callee = ci->getCalledFunction();
          if(callee && callee->getName().find("bump") != llvm::StringRef::npos)
          {
            if(found_switch)
              found_bump_after_switch = true;
            else
              found_bump_before_switch = true;
          }
        }
      }
    }
  }

  ASSERT_TRUE(found_switch) << "VCR switch not found in create function";
  ASSERT_FALSE(found_bump_before_switch)
    << "bump call found before VCR switch — args are not inside vcr.call";
  ASSERT_TRUE(found_bump_after_switch)
    << "bump call not found after VCR switch — expected it inside vcr.call";
}

TEST_F(GenMutateCodegenTest, VcrAorIndependence)
{
  // AOR records inside a VCR-eligible call's argument expression must be
  // the same whether VCR is enabled or not. The AOR site is in the argument
  // itself (a + U64(1)), not in a separate function body.
  const char* src =
    "class Obj\n"                                          // 1
    "  fun ref do_something(x: U64) => None\n"             // 2
    "\n"                                                   // 3
    "actor Main\n"                                         // 4
    "  new create(env: Env) =>\n"                          // 5
    "    let obj = Obj\n"                                  // 6
    "    let a: U64 = 3\n"                                 // 7
    "    obj.do_something(a + U64(1))\n";                  // 8

  // Compile with AOR only — no VCR.
  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_AOR;
  DO(test_compile(src, "ir"));

  unsigned aor_only = count_category_at_file("", 8, "aor");
  ASSERT_GT(aor_only, 0u)
    << "expected AOR records on line 8 (argument expression)";

  // Compile with AOR + VCR.
  opt.mutate_operators = MUTATE_OP_AOR | MUTATE_OP_VCR;
  DO(test_compile(src, "ir"));

  unsigned aor_with_vcr = count_category_at_file("", 8, "aor");
  ASSERT_EQ(aor_only, aor_with_vcr)
    << "AOR records on line 8 differ with VCR enabled";

  unsigned vcr_count = count_category_at_file("", 8, "vcr");
  ASSERT_GT(vcr_count, 0u)
    << "expected VCR record on line 8 when VCR is enabled";
}

TEST_F(GenMutateCodegenTest, RvrSuppressesSynthesizedPrimitiveEq)
{
  // Synthesized eq/ne on primitives use identity checks (this is that)
  // whose return value is always the same — RVR mutants are equivalent.
  const char* src =
    "primitive Colour\n"                                     // 1
    "  fun tag red(): U8 => 1\n"                             // 2
    "\n"                                                     // 3
    "actor Main\n"                                           // 4
    "  new create(env: Env) =>\n"                            // 5
    "    if Colour == Colour then None end\n";               // 6

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  // The synthesized eq body (this is that) should produce no RVR records.
  // Colour's eq is generated by add_comparable; its body line is internal.
  // Check that no RVR records exist for any line in the Colour type.
  size_t rvr_total = 0;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(strcmp(r->category, "rvr") == 0)
    {
      std::string f(r->file);
      if(f.empty())
        rvr_total++;
    }
  }
  ASSERT_EQ(rvr_total, 0u)
    << "expected no RVR records (synthesized eq/ne should be suppressed)";
}

TEST_F(GenMutateCodegenTest, RvrConstantBodyDropsEquivalent)
{
  // `fun g(): Bool => true` keeps return→false (differs from the
  // constant) and drops return→true (equivalent to the constant).
  const char* src =
    "primitive Val\n"                                        // 1
    "  fun g(): Bool => true\n"                              // 2
    "\n"                                                     // 3
    "actor Main\n"                                           // 4
    "  new create(env: Env) =>\n"                            // 5
    "    Val.g()\n";                                         // 6

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("rvr", "return", "false"))
    << "return->false should survive (differs from constant true)";
  ASSERT_FALSE(has_mutant("rvr", "return", "true"))
    << "return->true should be suppressed (equivalent to constant true)";
}

TEST_F(GenMutateCodegenTest, RvrConstantIntegerDropsEquivalent)
{
  // `fun f(): U64 => 0` keeps return→1 (differs) and drops return→0
  // (equivalent to the constant body).
  const char* src =
    "primitive Val\n"                                        // 1
    "  fun f(): U64 => 0\n"                                  // 2
    "\n"                                                     // 3
    "actor Main\n"                                           // 4
    "  new create(env: Env) =>\n"                            // 5
    "    Val.f()\n";                                         // 6

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("rvr", "return", "1"))
    << "return->1 should survive (differs from constant 0)";
  ASSERT_FALSE(has_mutant("rvr", "return", "0"))
    << "return->0 should be suppressed (equivalent to constant 0)";
}

TEST_F(GenMutateCodegenTest, RvrSuppressesNoneReturnValue)
{
  // When the return value's type is exactly None, the only pointer
  // replacement is None→None — an equivalent mutant.
  const char* src =
    "class Obj\n"                                            // 1
    "  fun apply(): (U64 | None) => None\n"                  // 2
    "\n"                                                     // 3
    "actor Main\n"                                           // 4
    "  new create(env: Env) =>\n"                            // 5
    "    Obj()\n";                                           // 6

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  size_t rvr_total = 0;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(strcmp(r->category, "rvr") == 0)
      rvr_total++;
  }
  ASSERT_EQ(rvr_total, 0u)
    << "expected no RVR records (None return value should be suppressed)";
}

TEST_F(GenMutateCodegenTest, RvrNotSuppressedForNonConstant)
{
  // A function with a non-constant body should still get RVR records.
  const char* src =
    "class Obj\n"                                            // 1
    "  var _x: U64 = 0\n"                                   // 2
    "  fun ref bump(): U64 =>\n"                             // 3
    "    _x = _x + 1\n"                                     // 4
    "    _x\n"                                               // 5
    "\n"                                                     // 6
    "actor Main\n"                                           // 7
    "  new create(env: Env) =>\n"                            // 8
    "    Obj.bump()\n";                                      // 9

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  size_t rvr_total = 0;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(strcmp(r->category, "rvr") == 0)
      rvr_total++;
  }
  ASSERT_GT(rvr_total, 0u)
    << "expected RVR records for non-constant function body";
}

TEST_F(GenMutateCodegenTest, RvrExplicitReturnDropsEquivalent)
{
  // An explicit `return true` on a dedicated line drops return→true
  // (equivalent) and keeps return→false. The body's own implicit
  // return of `false` lands on a different line, so line-filtering
  // isolates the explicit return site.
  const char* src =
    "class Obj\n"                                            // 1
    "  fun check(n: U64): Bool =>\n"                         // 2
    "    if n > U64(0) then\n"                               // 3
    "      return true\n"                                    // 4
    "    end\n"                                              // 5
    "    false\n"                                            // 6
    "\n"                                                     // 7
    "actor Main\n"                                           // 8
    "  new create(env: Env) =>\n"                            // 9
    "    Obj.check(U64(1))\n";                               // 10

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  // The explicit `return true` on line 4 should have return→false
  // but not return→true (equivalent to the constant).
  bool has_false_on_4 = false;
  bool has_true_on_4 = false;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(strcmp(r->category, "rvr") == 0 && r->line == 4)
    {
      if(strcmp(r->replacement, "false") == 0)
        has_false_on_4 = true;
      if(strcmp(r->replacement, "true") == 0)
        has_true_on_4 = true;
    }
  }
  ASSERT_TRUE(has_false_on_4)
    << "return->false should survive for explicit return true";
  ASSERT_FALSE(has_true_on_4)
    << "return->true should be suppressed for explicit return true";
}

TEST_F(GenMutateCodegenTest, RvrNoneFunctionNoMutants)
{
  // A function declared `: None` should get no RVR mutants at all,
  // regardless of the body expression's type.
  const char* src =
    "class Obj\n"                                            // 1
    "  var _x: U64 = 0\n"                                   // 2
    "  fun ref work(): None =>\n"                            // 3
    "    _x = _x + 1\n"                                     // 4
    "\n"                                                     // 5
    "actor Main\n"                                           // 6
    "  new create(env: Env) =>\n"                            // 7
    "    Obj.work()\n";                                      // 8

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  size_t rvr_on_work = 0;
  for(size_t i = 0; i < compile->mutate.mutant_count; i++)
  {
    mutant_record_t* r = &compile->mutate.mutants[i];
    if(strcmp(r->category, "rvr") == 0 && r->line == 4)
      rvr_on_work++;
  }
  ASSERT_EQ(rvr_on_work, 0u)
    << "a :None function should produce no RVR records";
}

TEST_F(GenMutateCodegenTest, RvrUnionMaybeGetsNoneReplacement)
{
  // `fun maybe(): (String | None) => "hello"` should get a None
  // replacement — the declared return type admits None even though
  // the body type is String.
  const char* src =
    "class Obj\n"                                            // 1
    "  fun maybe(): (String | None) => \"hello\"\n"          // 2
    "\n"                                                     // 3
    "actor Main\n"                                           // 4
    "  new create(env: Env) =>\n"                            // 5
    "    Obj.maybe()\n";                                     // 6

  opt.mutate = true;
  opt.mutate_operators = MUTATE_OP_RVR;
  DO(test_compile(src, "ir"));

  ASSERT_TRUE(has_mutant("rvr", "return", "None"))
    << "None replacement should be offered (declared type admits None)";
}
