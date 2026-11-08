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
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 5\n"
    "    match y\n"
    "    | 3 => None\n"
    "    | 5 => None\n"
    "    end\n";

  opt.mutate = true;
  DO(test_compile(src, "ir"));

  ASSERT_GT(compile->mutate.mutant_count, 0u);
  ASSERT_TRUE(has_mutant("ror", "eq", "ne"));
  ASSERT_TRUE(has_mutant("ror", "eq", "lt"));
  ASSERT_TRUE(has_mutant("ror", "eq", "le"));
  ASSERT_TRUE(has_mutant("ror", "eq", "ge"));
  ASSERT_TRUE(has_mutant("ror", "eq", "gt"));
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
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = is_ok()\n"
    "\n"
    "  fun is_ok(): Bool =>\n"
    "    true\n";

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
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = maybe()\n"
    "\n"
    "  fun maybe(): (String | None) =>\n"
    "    None\n";

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
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let y: U64 = 10\n"
    "    let a: U64 = y + 5\n"
    "    let b: U64 = (1 + 2) * 3\n"
    "    let c: U64 = y << 2\n"
    "    let d: Bool = y == 0\n"
    "    let e: U64 = (not 0x20) * 2\n";

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


TEST_F(GenMutateCodegenTest, ShortCircuitShadowConstantFalseAnd)
{
  // not (false and (x > 0)): left is constant false, so the and
  // short-circuits. The outer not has a constant shadow, so no UOI.
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


TEST_F(GenMutateCodegenTest, ShortCircuitShadowVariableLeftAnd)
{
  // not ((x > 0) and (x < 100)): both sides are variable, so
  // the result has no shadow and UOI fires on the outer not.
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


TEST_F(GenMutateCodegenTest, ShortCircuitShadowConstantRightAnd)
{
  // not ((x > 0) and false): right is constant false.
  // Both branches of the and produce false (short-circuit: left false →
  // false; fall-through: true and false → false), so the phi has a
  // constant shadow and UOI does not fire on the outer not.
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


TEST_F(GenMutateCodegenTest, SafeSignedDivMinNeg1Shadow)
{
  // Safe integer div with constant operands produces no shadow
  // (the result is a phi node, not a constant). The downstream *
  // must still get AOR.
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
