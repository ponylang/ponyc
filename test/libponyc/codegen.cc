#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/gentype.h>

#include "util.h"

#include <codegen/genexe.h>

#ifdef PLATFORM_IS_LINUX
#include <codegen/genopt.h>
#include <llvm-c/Core.h>
#include <string>
#include <vector>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>
#include <cstdlib>
#endif

#include "llvm_config_begin.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>

#include "llvm_config_end.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenTest : public PassTest
{
};


TEST_F(CodegenTest, PackedStructIsPacked)
{
  const char* src =
    "struct \\packed\\ Foo\n"
    "  var a: U8 = 0\n"
    "  var b: U32 = 0\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo";

  TEST_COMPILE(src);

  reach_t* reach = compile->reach;
  reach_type_t* foo = reach_type_name(reach, "Foo", &opt);
  ASSERT_TRUE(foo != NULL);

  LLVMTypeRef type = ((compile_type_t*)foo->c_type)->structure;
  ASSERT_TRUE(LLVMIsPackedStruct(type));
}


TEST_F(CodegenTest, NonPackedStructIsntPacked)
{
  const char* src =
    "struct Foo\n"
    "  var a: U8 = 0\n"
    "  var b: U32 = 0\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Foo";

  TEST_COMPILE(src);

  reach_t* reach = compile->reach;
  reach_type_t* foo = reach_type_name(reach, "Foo", &opt);
  ASSERT_TRUE(foo != NULL);

  LLVMTypeRef type = ((compile_type_t*)foo->c_type)->structure;
  ASSERT_TRUE(!LLVMIsPackedStruct(type));
}

TEST_F(CodegenTest, BoxBoolAsUnionIntoTuple)
{
  // From #2191
  const char* src =
    "type U is (Bool | C)\n"

    "class C\n"
    "  fun apply(): (I32, U) =>\n"
    "    (0, false)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C()";

  TEST_COMPILE(src);

  ASSERT_TRUE(compile != NULL);
}

TEST_F(CodegenTest, UnionOfTuplesToTuple)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: ((Main, Env) | (Env, Main)) = (this, env)\n"
    "    let b: ((Main | Env), (Main | Env)) = a";

  TEST_COMPILE(src);
}


TEST_F(CodegenTest, ViewpointAdaptedFieldReach)
{
  // From issue #2238
  const char* src =
    "class A\n"
    "class B\n"
    "  var f: (A | None) = None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let l = B\n"
    "    l.f = A";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, DoNotOptimiseApplyPrimitive)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    DoNotOptimise[I64](0)";

  TEST_COMPILE(src);
}


TEST_F(CodegenTest, RecoverCast)
{
  // From issue #2639
  const char* src =
    "class A\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: ((None, None) | (A iso, None)) =\n"
    "      if false then\n"
    "        (None, None)\n"
    "      else\n"
    "        recover (A, None) end\n"
    "      end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, VariableDeclNestedTuple)
{
  // From issue #2662
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let array = Array[((U8, U8), U8)]\n"
    "    for ((a, b), c) in array.values() do\n"
    "      None\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, TupleRemoveUnreachableBlockInLoop)
{
  // From issue #2760
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var token = U8(1)\n"
    "    repeat\n"
    "      (token, let source) = (U8(1), U8(2))\n"
    "    until token == 2 end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, TupleRemoveUnreachableBlock)
{
  // From issue #2735
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if true then\n"
    "      (let f, let g) = (U8(2), U8(3))\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, RedundantUnionInForLoopIteratorValueType)
{
  // From issue #2736
  const char* src =
    "type T is (U8 | U8)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let i = I\n"
    "    for m in i do\n"
    "      None\n"
    "    end\n"

    "class ref I is Iterator[T]\n"
    "  new create() => None\n"
    "  fun ref has_next(): Bool => false\n"
    "  fun ref next(): T ? => error";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, WhileLoopBreakOnlyValueNeeded)
{
  // From issue #2788
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x = while true do break true else false end\n"
        "if x then None end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, WhileLoopBreakOnlyInBranches)
{
  // From issue #2788
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "while true do\n"
          "if true then\n"
            "break None\n"
          "else\n"
            "break\n"
          "end\n"
        "else\n"
          "return\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, RepeatLoopBreakOnlyValueNeeded)
{
  // From issue #2788
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x = repeat break true until false else false end\n"
        "if x then None end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, RepeatLoopBreakOnlyInBranches)
{
  // From issue #2788
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "repeat\n"
          "if true then\n"
            "break None\n"
          "else\n"
            "break\n"
          "end\n"
        "until\n"
          "false\n"
        "else\n"
          "return\n"
        "end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, RepeatLoopElseClauseJumpsAway)
{
  // From issue #3326
  // The else clause `error` jumps away and so has no type. gen_repeat must not
  // try to reify that absent type, which previously crashed the compiler.
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "try let x: U8 = repeat 1 until false else error end end";

  TEST_COMPILE(src);
}

TEST_F(CodegenTest, LifetimeIntrinsicsHaveSinglePointerArgument)
{
  // LLVM 22 dropped the leading size argument from llvm.lifetime.start and
  // llvm.lifetime.end; they now take a single pointer. A two-argument call
  // is rejected by the module verifier, so confirm codegen emits exactly one
  // argument. The union plus match capture below forces scoped locals, which
  // is what makes codegen emit lifetime markers.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: (U64 | None) = U64(1)\n"
    "    match x\n"
    "    | let n: U64 => None\n"
    "    end";

  TEST_COMPILE(src);

  auto module = llvm::unwrap(compile->module);

  size_t lifetime_calls = 0;
  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        auto intrinsic = llvm::dyn_cast<llvm::IntrinsicInst>(&inst);
        if(intrinsic == nullptr)
          continue;

        auto id = intrinsic->getIntrinsicID();
        if((id == llvm::Intrinsic::lifetime_start) ||
          (id == llvm::Intrinsic::lifetime_end))
        {
          lifetime_calls++;
          ASSERT_EQ(intrinsic->arg_size(), 1u);
        }
      }
    }
  }

  // Guard against a vacuous pass: the program above must actually emit
  // lifetime markers, or the per-call assertion never runs.
  ASSERT_GT(lifetime_calls, 0u);
}

// codegen_pass_init marks an explicitly provided triple as user_triple so libc
// detection treats it as authoritative (see target_libc_is_musl); a defaulted
// triple leaves it clear.
extern const char* test_argv0;  // the gtest main's argv[0], defined in util.cc

TEST(CodegenPassInitTest, ExplicitTripleSetsUserTriple)
{
  pass_opt_t opt;
  pass_opt_init(&opt);
  opt.argv0 = test_argv0;
  opt.triple = const_cast<char*>("x86_64-unknown-linux-gnu");
  ASSERT_TRUE(codegen_pass_init(&opt));
  EXPECT_TRUE(opt.user_triple);
  codegen_pass_cleanup(&opt);
  pass_opt_done(&opt);
}

TEST(CodegenPassInitTest, DefaultTripleLeavesUserTripleClear)
{
  pass_opt_t opt;
  pass_opt_init(&opt);
  opt.argv0 = test_argv0;
  ASSERT_TRUE(codegen_pass_init(&opt));
  EXPECT_FALSE(opt.user_triple);
  codegen_pass_cleanup(&opt);
  pass_opt_done(&opt);
}

#ifdef PLATFORM_IS_LINUX

namespace
{
  // Create dir and any missing parents (mkdir -p). Errors (including EEXIST)
  // are ignored: the file operations that follow fail loudly if a dir is
  // missing, so a real failure surfaces there.
  void mkdir_p(const std::string& dir)
  {
    for(size_t i = 1; i <= dir.size(); i++)
    {
      if((i == dir.size()) || (dir[i] == '/'))
        mkdir(dir.substr(0, i).c_str(), 0700);
    }
  }

  // Create an empty libc.so.6 under <base><subdir> -- the glibc marker the
  // fingerprint looks for. Its contents are never read, only its presence.
  bool put_libc_so_6(const std::string& base, const char* subdir)
  {
    std::string dir = base + subdir;
    mkdir_p(dir);
    int fd = open((dir + "/libc.so.6").c_str(), O_CREAT | O_WRONLY, 0644);
    if(fd < 0)
      return false;
    close(fd);
    return true;
  }

  int rm_entry(const char* path, const struct stat*, int, struct FTW*)
  {
    remove(path);
    return 0;
  }

  void remove_tree(const std::string& base)
  {
    nftw(base.c_str(), rm_entry, 16, FTW_DEPTH | FTW_PHYS);
  }

  // Replace opt->triple, keeping it an LLVM-owned string so TearDown's codegen
  // cleanup frees it exactly once however the test exits.
  void set_triple(pass_opt_t* opt, const char* triple)
  {
    LLVMDisposeMessage(opt->triple);
    opt->triple = LLVMCreateMessage(triple);
  }
}

// target_libc_is_musl decides the target C library. With no explicit triple it
// fingerprints the sysroot -- glibc ships libc.so.6 in a standard lib
// directory, musl never does. An explicit triple, a musl-named default triple,
// and a non-Linux target each short-circuit that probe.
TEST_F(CodegenTest, TargetLibcMuslDetection)
{
  // Reset opt.sysroot and remove the temp sysroots on every exit path,
  // including a fatal ASSERT_* (which returns from the test body). opt.triple
  // is left LLVM-owned throughout, so TearDown frees it independently.
  std::vector<std::string> roots;
  struct Cleanup
  {
    pass_opt_t* opt;
    std::vector<std::string>* roots;
    ~Cleanup()
    {
      opt->sysroot = NULL;
      for(const std::string& root : *roots)
        remove_tree(root);
    }
  } cleanup{&opt, &roots};

  char glibc_tmpl[] = "/tmp/ponyc_libc_glibc_XXXXXX";
  char musl_tmpl[] = "/tmp/ponyc_libc_musl_XXXXXX";
  char split_tmpl[] = "/tmp/ponyc_libc_split_XXXXXX";
  ASSERT_NE(mkdtemp(glibc_tmpl), nullptr);
  ASSERT_NE(mkdtemp(musl_tmpl), nullptr);
  ASSERT_NE(mkdtemp(split_tmpl), nullptr);
  std::string glibc_root = glibc_tmpl;
  std::string musl_root = musl_tmpl;
  std::string split_root = split_tmpl;
  roots.push_back(glibc_root);
  roots.push_back(musl_root);
  roots.push_back(split_root);

  // usr-merged glibc: libc.so.6 in /usr/lib. musl: none anywhere. Non-usr-
  // merged glibc: libc.so.6 in /lib, where crt1.o would sit in /usr/lib.
  ASSERT_TRUE(put_libc_so_6(glibc_root, "/usr/lib"));
  ASSERT_TRUE(put_libc_so_6(split_root, "/lib"));

  // A gnu, non-musl Linux triple so the fingerprint runs. It tests for
  // libc.so.6 by path with no ELF/arch validation, so a fixed triple keeps the
  // test host-independent. opt.triple stays LLVM-owned (see set_triple).
  set_triple(&opt, "x86_64-unknown-linux-gnu");
  opt.user_triple = false;

  // libc.so.6 present in a standard lib dir -> glibc.
  opt.sysroot = const_cast<char*>(glibc_root.c_str());
  ASSERT_FALSE(target_libc_is_musl(&opt));

  // libc.so.6 absent everywhere -> musl.
  opt.sysroot = const_cast<char*>(musl_root.c_str());
  ASSERT_TRUE(target_libc_is_musl(&opt));

  // Non-usr-merged split (libc.so.6 in /lib, not beside the crt dir) -> glibc.
  opt.sysroot = const_cast<char*>(split_root.c_str());
  ASSERT_FALSE(target_libc_is_musl(&opt));

  // An explicit --triple wins over the fingerprint: musl-looking sysroot, gnu
  // triple -> glibc.
  opt.sysroot = const_cast<char*>(musl_root.c_str());
  opt.user_triple = true;
  ASSERT_FALSE(target_libc_is_musl(&opt));

  // A non-Linux target is never musl, whatever the host filesystem holds.
  opt.user_triple = false;
  set_triple(&opt, "x86_64-unknown-freebsd");
  ASSERT_FALSE(target_libc_is_musl(&opt));

  // A default triple that already names musl is trusted without probing -- the
  // glibc-looking sysroot must not flip it back.
  set_triple(&opt, "x86_64-unknown-linux-musl");
  opt.sysroot = const_cast<char*>(glibc_root.c_str());
  ASSERT_TRUE(target_libc_is_musl(&opt));
}

#endif
