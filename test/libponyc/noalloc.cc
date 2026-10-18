#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/genopt.h>
#include <heap_to_stack.h>
#include <noalloc_check.h>

#include "util.h"

#include "llvm_config_begin.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CGSCCPassManager.h>

#include "llvm_config_end.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))
#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

// --- Syntax tests ---

class NoAllocSyntaxTest : public PassTest
{};

TEST_F(NoAllocSyntaxTest, NoAllocOnFun)
{
  const char* src =
    "class C\n"
    "  fun \\noalloc\\ f(): U32 => 42\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  DO(test_compile(src, "syntax"));
}

TEST_F(NoAllocSyntaxTest, NoAllocOnBe)
{
  const char* src =
    "actor A\n"
    "  be \\noalloc\\ handle() => None\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  DO(test_compile(src, "syntax"));
}

TEST_F(NoAllocSyntaxTest, NoAllocOnNew)
{
  const char* src =
    "class C\n"
    "  new \\noalloc\\ create() => None\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  DO(test_compile(src, "syntax"));
}

TEST_F(NoAllocSyntaxTest, NoAllocOnClassError)
{
  const char* src =
    "class \\noalloc\\ C\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  const char* errs[] = {
    "a 'noalloc' annotation can only appear on a method declaration",
    NULL
  };

  DO(test_expected_errors(src, "syntax", errs));
}

// --- Conformance tests ---

class NoAllocConformanceTest : public PassTest
{};

TEST_F(NoAllocConformanceTest, NonNoAllocImplOfNoAllocInterfaceError)
{
  const char* src =
    "interface Computable\n"
    "  fun \\noalloc\\ compute(): U32\n"
    "\n"
    "class Calculator is Computable\n"
    "  fun compute(): U32 => 42\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  const char* errs[] = {
    "type does not implement its provides list",
    NULL
  };

  const char* frame0[] = {
    "a method that implements a 'noalloc' method must also be 'noalloc'",
    "is not a subtype of",
    NULL
  };

  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(NoAllocConformanceTest, NonNoAllocImplOfNoAllocTraitError)
{
  const char* src =
    "trait Computable\n"
    "  fun \\noalloc\\ compute(): U32\n"
    "\n"
    "class Calculator is Computable\n"
    "  fun compute(): U32 => 42\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  const char* errs[] = {
    "type does not implement its provides list",
    NULL
  };

  const char* frame0[] = {
    "a method that implements a 'noalloc' method must also be 'noalloc'",
    "is not a subtype of",
    NULL
  };

  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(NoAllocConformanceTest, NoAllocImplOfNoAllocInterface)
{
  const char* src =
    "interface Computable\n"
    "  fun \\noalloc\\ compute(): U32\n"
    "\n"
    "class Calculator is Computable\n"
    "  fun \\noalloc\\ compute(): U32 => 42\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  DO(test_compile(src, "expr"));
}

TEST_F(NoAllocConformanceTest, NoAllocImplOfNonNoAllocInterface)
{
  const char* src =
    "interface Computable\n"
    "  fun compute(): U32\n"
    "\n"
    "class Calculator is Computable\n"
    "  fun \\noalloc\\ compute(): U32 => 42\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  DO(test_compile(src, "expr"));
}

// --- Virtual call restriction tests ---

class NoAllocVirtualCallTest : public PassTest
{};

TEST_F(NoAllocVirtualCallTest, NoAllocCallsNonNoAllocInterfaceError)
{
  const char* src =
    "interface Runnable\n"
    "  fun run(): U32\n"
    "\n"
    "class C\n"
    "  fun \\noalloc\\ go(r: Runnable): U32 =>\n"
    "    r.run()\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src,
    "a 'noalloc' method cannot make a virtual call to a non-'noalloc' method");
}

TEST_F(NoAllocVirtualCallTest, NoAllocCallsNoAllocInterface)
{
  const char* src =
    "interface Runnable\n"
    "  fun \\noalloc\\ run(): U32\n"
    "\n"
    "class C\n"
    "  fun \\noalloc\\ go(r: Runnable): U32 =>\n"
    "    r.run()\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  DO(test_compile(src, "expr"));
}

TEST_F(NoAllocVirtualCallTest, NonNoAllocCallsNonNoAllocInterface)
{
  const char* src =
    "interface Runnable\n"
    "  fun run(): U32\n"
    "\n"
    "class C\n"
    "  fun go(r: Runnable): U32 =>\n"
    "    r.run()\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  DO(test_compile(src, "expr"));
}

TEST_F(NoAllocVirtualCallTest, NoAllocChainCallsNonNoAllocInterfaceError)
{
  const char* src =
    "interface Settable\n"
    "  fun ref apply(v: U32): None\n"
    "\n"
    "class C\n"
    "  fun \\noalloc\\ go(s: Settable) =>\n"
    "    s.>apply(1).>apply(2)\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src,
    "a 'noalloc' method cannot make a virtual call to a non-'noalloc' method");
}

TEST_F(NoAllocVirtualCallTest, NoAllocCallsNonNoAllocBeInterfaceError)
{
  const char* src =
    "interface Notifiable\n"
    "  be notify(v: U32)\n"
    "\n"
    "class C\n"
    "  fun \\noalloc\\ go(n: Notifiable) =>\n"
    "    n.notify(1)\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src,
    "a 'noalloc' method cannot make a virtual call to a non-'noalloc' method");
}

// --- LTO pass tests ---

static void noalloc_diag_handler(const llvm::DiagnosticInfo *DI, void *context)
{
  if(DI->getSeverity() == llvm::DS_Error)
    *static_cast<bool*>(context) = true;
}

class NoAllocLTOTest : public PassTest
{
protected:
  // Run NoAllocCheck on the in-memory module. Returns true if any allocation
  // violation was diagnosed (i.e., the pass emitted a DS_Error).
  bool run_noalloc_check()
  {
    auto module = llvm::unwrap(compile->module);

    for(auto &function : *module)
    {
      if(!function.isDeclaration())
        function.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }

    bool had_error = false;
    auto &ctx = module->getContext();
    ctx.setDiagnosticHandlerCallBack(noalloc_diag_handler, &had_error);

    EXPECT_TRUE(pony_specific_opt(compile));

    llvm::PassBuilder PB;
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    FAM.registerPass([&] { return PB.buildDefaultAAPipeline(); });

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::CGSCCPassManager CGPM;
    CGPM.addPass(pony::HeapToStack());

    llvm::ModulePassManager MPM;
    MPM.addPass(
      llvm::createModuleToPostOrderCGSCCPassAdaptor(std::move(CGPM)));
    MPM.addPass(pony::NoAllocCheck());
    MPM.run(*module, MAM);

    ctx.setDiagnosticHandlerCallBack(nullptr, nullptr);

    return had_error;
  }
};


TEST_F(NoAllocLTOTest, NoAllocPureArithmeticPasses)
{
  const char* src =
    "class C\n"
    "  fun \\noalloc\\ fortytwo(): U32 => 42\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C.fortytwo()";

  opt.release = true;
  TEST_COMPILE(src);
  ASSERT_FALSE(run_noalloc_check());
}

TEST_F(NoAllocLTOTest, NoAllocWithAllocationFails)
{
  const char* src =
    "class C\n"
    "  fun \\noalloc\\ make(): String =>\n"
    "    String\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C.make()";

  opt.release = true;
  TEST_COMPILE(src);
  ASSERT_TRUE(run_noalloc_check());
}

TEST_F(NoAllocLTOTest, NoAllocTransitiveAllocationFails)
{
  const char* src =
    "class Helper\n"
    "  fun allocates(): String => String\n"
    "\n"
    "class C\n"
    "  fun \\noalloc\\ go(h: Helper): String =>\n"
    "    h.allocates()\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C.go(Helper)";

  opt.release = true;
  TEST_COMPILE(src);
  ASSERT_TRUE(run_noalloc_check());
}

TEST_F(NoAllocLTOTest, NoAllocCallingNoAllocPasses)
{
  const char* src =
    "class Helper\n"
    "  fun \\noalloc\\ pure(): U32 => 42\n"
    "\n"
    "class C\n"
    "  fun \\noalloc\\ go(h: Helper): U32 =>\n"
    "    h.pure()\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C.go(Helper)";

  opt.release = true;
  TEST_COMPILE(src);
  ASSERT_FALSE(run_noalloc_check());
}

TEST_F(NoAllocLTOTest, NoAllocBePureBodyPasses)
{
  const char* src =
    "actor A\n"
    "  var _x: U32 = 0\n"
    "  be \\noalloc\\ bump(v: U32) =>\n"
    "    _x = v\n"
    "\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    A.bump(1)";

  opt.release = true;
  TEST_COMPILE(src);
  ASSERT_FALSE(run_noalloc_check());
}
