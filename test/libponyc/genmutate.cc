#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/codegen.h>
#include <codegen/genmutate.h>
#include <mem/pool.h>

#include "util.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>

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
    "arithmetic", "+", "-");

  EXPECT_EQ(state.mutant_count, 1u);
  EXPECT_EQ(state.mutants[0].id, 1);
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
      "cat", "a", "b");

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
  EXPECT_NE(json.find("\"mutants\": []"), std::string::npos);

  remove(path.c_str());
  remove_temp_dir(tmpdir);
}


TEST_F(GenMutateTest, WriteInfoEscapesSpecialChars)
{
  mutate_init(&state);

  mutate_record(&state, 1,
    "path\\with\"special\nchars", 1, 1,
    "cat\twith\ttabs", "a\rb", "c\\d");

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
    "arithmetic", "+", "-");
  mutate_record(&state, 2, "bar.pony", 20, 7,
    "conditional", "true", "false");

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
  EXPECT_NE(json.find("\"id\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"id\": 2"), std::string::npos);
  EXPECT_NE(json.find("\"file\": \"foo.pony\""), std::string::npos);
  EXPECT_NE(json.find("\"category\": \"arithmetic\""), std::string::npos);

  remove(path.c_str());
  remove_temp_dir(tmpdir);
}


TEST_F(GenMutateTest, WriteInfoHandlesLongStrings)
{
  mutate_init(&state);

  std::string long_str(1500, 'x');
  mutate_record(&state, 1, long_str.c_str(), 1, 1,
    "cat", "a", "b");

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
  // Default: all operators enabled
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
